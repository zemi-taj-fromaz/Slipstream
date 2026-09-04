#include "GrpcServerTransport.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <deque>
#include <exception>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>

#include <spdlog/spdlog.h>

namespace slipstream {

namespace {

std::string Address(const std::string& host, const std::uint16_t port) {
    return host + ':' + std::to_string(port);
}

std::uint64_t MonotonicNowNs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

std::uint64_t UnixNowNs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

template <std::size_t Size>
void CopySymbol(const std::string& source, char (&destination)[Size]) {
    std::fill(std::begin(destination), std::end(destination), '\0');
    std::copy_n(
        source.data(),
        std::min(source.size(), Size),
        destination);
}

template <std::size_t Size>
std::string SymbolText(const char (&symbol)[Size]) {
    const char* end = std::find(std::begin(symbol), std::end(symbol), '\0');
    return {std::begin(symbol), end};
}

MarketEvent FromGrpcQuote(const grpc_api::Quote& quote) {
    MarketEvent event{};
    event.ts = quote.ts_ns();
    CopySymbol(quote.symbol(), event.symbol);
    event.payload = Quote{
        .bid_price = quote.bid_price(),
        .bid_qty = quote.bid_qty(),
        .ask_price = quote.ask_price(),
        .ask_qty = quote.ask_qty(),
    };
    return event;
}

MarketEvent FromGrpcTrade(const grpc_api::Trade& trade) {
    char aggressor = '?';
    if (trade.aggressor() == grpc_api::Trade::AGGRESSOR_BUY) {
        aggressor = 'B';
    } else if (trade.aggressor() == grpc_api::Trade::AGGRESSOR_SELL) {
        aggressor = 'S';
    }

    MarketEvent event{};
    event.ts = trade.ts_ns();
    CopySymbol(trade.symbol(), event.symbol);
    event.payload = Trade{
        .price = trade.price(),
        .id = trade.id(),
        .qty = trade.qty(),
        .aggressor = aggressor,
    };
    return event;
}

grpc_api::OeServerMessage ToGrpcMessage(
    const codec::OrderEntryClientMessage& message) {
    grpc_api::OeServerMessage output;

    std::visit(
        [&output](const auto& value) {
            using Message = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<Message, codec::NewOrderMessage>) {
                auto* order = output.mutable_new_order();
                order->set_client_order_id(value.client_order_id);
                order->set_symbol(SymbolText(value.symbol));
                order->set_status(
                    value.status == codec::NewOrderStatus::accepted
                        ? grpc_api::NewOrder::ACCEPTED
                        : grpc_api::NewOrder::REJECTED);
                order->set_ts_ns(value.ts_ns);
                order->set_trade_id(value.trade_id);
                order->set_side(
                    value.side == codec::OrderSide::buy
                        ? grpc_api::NewOrder::BUY
                        : grpc_api::NewOrder::SELL);
                order->set_qty(value.qty);
                order->set_limit_price(value.limit_px);
            } else if constexpr (
                std::is_same_v<Message, codec::ExecReportMessage>) {
                auto* report = output.mutable_exec_report();
                report->set_client_order_id(value.client_order_id);
                report->set_ts_ns(value.ts_ns);
                report->set_status(
                    static_cast<grpc_api::ExecReport::Status>(value.status));
                report->set_filled_qty(value.filled_qty);
                report->set_avg_price(value.avg_px);
                report->set_reason_code(
                    static_cast<grpc_api::ExecReport::RejectReason>(
                        value.reason_code));
            } else if constexpr (
                std::is_same_v<Message, codec::HeartbeatMessage>) {
                output.mutable_heartbeat()->set_ts_ns(value.ts_ns);
            } else if constexpr (
                std::is_same_v<Message, codec::SessionControlMessage>) {
                auto* control = output.mutable_session_control();
                control->set_ts_ns(value.ts_ns);
                control->set_state(
                    static_cast<grpc_api::SessionControl::State>(value.state));
            }
        },
        message);

    return output;
}

}

class GrpcServerTransport::CompletionTag {
public:
    virtual ~CompletionTag() = default;
    virtual void Complete(bool ok) = 0;
};

class GrpcServerTransport::OutboundWakeTag final : public CompletionTag {
public:
    explicit OutboundWakeTag(GrpcServerTransport& owner)
        : owner_{owner} {}

    void Complete(const bool ok) override {
        owner_.onOutboundWake(ok);
    }

private:
    GrpcServerTransport& owner_;
};

class GrpcServerTransport::MdConnection final : public CompletionTag {
public:
    explicit MdConnection(GrpcServerTransport& owner)
        : owner_{owner},
          reader_{&context_} {
        owner_.market_data_service_.RequestConnect(
            &context_,
            &reader_,
            owner_.completion_queue_.get(),
            owner_.completion_queue_.get(),
            this);
    }

    void Complete(const bool ok) override {
        switch (state_) {
        case State::WaitingForConnection:
            if (!ok) {
                state_ = State::Finished;
                return;
            }
            state_ = State::Reading;
            reader_.Read(&quote_, this);
            return;

        case State::Reading:
            if (!ok) {
                if (owner_.shutdown_requested_.load(
                        std::memory_order_acquire)) {
                    state_ = State::Finished;
                    return;
                }
                state_ = State::Finishing;
                reader_.Finish(response_, grpc::Status::OK, this);
                return;
            }
            processQuote();
            reader_.Read(&quote_, this);
            return;

        case State::Finishing:
            state_ = State::Finished;
            owner_.requestShutdown();
            return;

        case State::Finished:
            return;
        }
    }

private:
    enum class State {
        WaitingForConnection,
        Reading,
        Finishing,
        Finished,
    };

    void processQuote() {
        MarketEvent event = FromGrpcQuote(quote_);
        if (std::strcmp(event.symbol, owner_.config_.symbol.c_str()) != 0) {
            return;
        }

        const InboundEvent inbound{
            .message = std::move(event),
            .received_at_ns = MonotonicNowNs(),
        };
        if (!owner_.ingress_.push(inbound)) {
            throw std::runtime_error("failed to enqueue gRPC quote");
        }

        owner_.ingress_generation_.fetch_add(1, std::memory_order_release);
        owner_.ingress_generation_.notify_one();
    }

    GrpcServerTransport& owner_;
    grpc::ServerContext context_;
    grpc::ServerAsyncReader<google::protobuf::Empty, grpc_api::Quote> reader_;
    grpc_api::Quote quote_;
    google::protobuf::Empty response_;
    State state_{State::WaitingForConnection};
};

class GrpcServerTransport::OeConnection final {
public:
    struct PendingWrite {
        grpc_api::OeServerMessage message;
        std::uint64_t trigger_received_at_ns{};
        std::uint64_t send_started_at_ns{};
        bool measure_tick_to_order{};
        bool close_after_write{};
    };

    explicit OeConnection(GrpcServerTransport& owner)
        : owner_{owner},
          stream_{&context_},
          connect_tag_{*this, Operation::Connect},
          read_tag_{*this, Operation::Read},
          write_tag_{*this, Operation::Write},
          finish_tag_{*this, Operation::Finish} {
        owner_.order_entry_service_.RequestConnect(
            &context_,
            &stream_,
            owner_.completion_queue_.get(),
            owner_.completion_queue_.get(),
            &connect_tag_);
    }

    void Enqueue(PendingWrite write) {
        if (finishing_ || finished_) {
            return;
        }
        pending_writes_.push_back(std::move(write));
        tryStartWrite();
    }

    [[nodiscard]] bool CanWrite() const noexcept {
        return connected_ && !finishing_ && !finished_;
    }

private:
    enum class Operation {
        Connect,
        Read,
        Write,
        Finish,
    };

    class OperationTag final : public CompletionTag {
    public:
        OperationTag(OeConnection& owner, const Operation operation)
            : owner_{owner}, operation_{operation} {}

        void Complete(const bool ok) override {
            owner_.complete(operation_, ok);
        }

    private:
        OeConnection& owner_;
        Operation operation_;
    };

    void complete(const Operation operation, const bool ok) {
        switch (operation) {
        case Operation::Connect:
            onConnected(ok);
            break;
        case Operation::Read:
            onReadCompleted(ok);
            break;
        case Operation::Write:
            onWriteCompleted(ok);
            break;
        case Operation::Finish:
            finished_ = true;
            owner_.requestShutdown();
            break;
        }
    }

    void onConnected(const bool ok) {
        if (!ok) {
            finished_ = true;
            return;
        }

        connected_ = true;
        owner_.markOeActivity();
        startRead();
        owner_.drainEgress();
    }

    void startRead() {
        if (read_in_flight_ || finishing_ || finished_) {
            return;
        }
        read_in_flight_ = true;
        stream_.Read(&inbound_, &read_tag_);
    }

    void onReadCompleted(const bool ok) {
        read_in_flight_ = false;
        if (!ok) {
            if (owner_.shutdown_requested_.load(
                    std::memory_order_acquire)) {
                finished_ = true;
                return;
            }
            finish_requested_ = true;
            tryBeginFinish();
            return;
        }

        if (inbound_.payload_case() == grpc_api::OeClientMessage::kTrade) {
            MarketEvent event = FromGrpcTrade(inbound_.trade());
            if (std::strcmp(event.symbol, owner_.config_.symbol.c_str()) == 0) {
                owner_.markOeActivity();
                const InboundEvent inbound{
                    .message = std::move(event),
                    .received_at_ns = MonotonicNowNs(),
                };
                if (!owner_.ingress_.push(inbound)) {
                    throw std::runtime_error("failed to enqueue gRPC trade");
                }
                owner_.ingress_generation_.fetch_add(
                    1,
                    std::memory_order_release);
                owner_.ingress_generation_.notify_one();
            }
        }

        startRead();
    }

    void tryStartWrite() {
        if (write_in_flight_ || finishing_ || pending_writes_.empty()) {
            return;
        }

        current_write_ = std::move(pending_writes_.front());
        pending_writes_.pop_front();
        current_write_.send_started_at_ns = MonotonicNowNs();
        write_in_flight_ = true;
        stream_.Write(current_write_.message, &write_tag_);
    }

    void onWriteCompleted(const bool ok) {
        write_in_flight_ = false;
        if (!ok) {
            pending_writes_.clear();
            if (owner_.shutdown_requested_.load(
                    std::memory_order_acquire)) {
                finished_ = true;
                return;
            }
            finish_requested_ = true;
            tryBeginFinish();
            return;
        }

        owner_.recordTickToOrder(
            current_write_.trigger_received_at_ns,
            current_write_.send_started_at_ns,
            current_write_.measure_tick_to_order);

        if (current_write_.close_after_write) {
            pending_writes_.clear();
            finish_requested_ = true;
            tryBeginFinish();
            return;
        }

        tryStartWrite();
        tryBeginFinish();
    }

    void tryBeginFinish() {
        if (!finish_requested_ || write_in_flight_ || finishing_ || finished_) {
            return;
        }

        finishing_ = true;
        stream_.Finish(grpc::Status::OK, &finish_tag_);
    }

    GrpcServerTransport& owner_;
    grpc::ServerContext context_;
    grpc::ServerAsyncReaderWriter<
        grpc_api::OeServerMessage,
        grpc_api::OeClientMessage> stream_;
    grpc_api::OeClientMessage inbound_;
    PendingWrite current_write_{};
    std::deque<PendingWrite> pending_writes_;

    OperationTag connect_tag_;
    OperationTag read_tag_;
    OperationTag write_tag_;
    OperationTag finish_tag_;

    bool connected_{false};
    bool read_in_flight_{false};
    bool write_in_flight_{false};
    bool finish_requested_{false};
    bool finishing_{false};
    bool finished_{false};
};

GrpcServerTransport::GrpcServerTransport(
    const SlipstreamConfig& config,
    MarketEventQueue& ingress,
    OrderEntryQueue& egress,
    std::atomic<std::uint64_t>& ingress_generation)
    : config_{config},
      ingress_{ingress},
      egress_{egress},
      ingress_generation_{ingress_generation} {}

GrpcServerTransport::~GrpcServerTransport() {
    Stop();
}

void GrpcServerTransport::Run() {
    constexpr int receive_buffer_size = 1024 * 1024;
    session_control_listener_.SetReuseAddress();
    session_control_listener_.SetReceiveBufferSize(receive_buffer_size);
    session_control_listener_.Bind("127.0.0.1", 9099);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(
        Address(config_.md_host, config_.md_port),
        grpc::InsecureServerCredentials());
    builder.AddListeningPort(
        Address(config_.oe_host, config_.oe_port),
        grpc::InsecureServerCredentials());
    builder.RegisterService(&market_data_service_);
    builder.RegisterService(&order_entry_service_);

    {
        std::scoped_lock lock{lifecycle_mutex_};
        completion_queue_ = builder.AddCompletionQueue();
        server_ = builder.BuildAndStart();
    }
    if (!server_) {
        completion_queue_->Shutdown();
        void* raw_tag = nullptr;
        bool ok = false;
        while (completion_queue_->Next(&raw_tag, &ok)) {
        }
        throw std::runtime_error("failed to start gRPC server");
    }

    outbound_wake_tag_ = std::make_unique<OutboundWakeTag>(*this);
    md_connection_ = std::make_unique<MdConnection>(*this);
    oe_connection_ = std::make_unique<OeConnection>(*this);

    if (shutdown_requested_.load(std::memory_order_acquire)) {
        initiateShutdown();
    }

    std::exception_ptr failure;
    try {
        while (true) {
            void* raw_tag = nullptr;
            bool ok = false;
            const auto result = completion_queue_->AsyncNext(
                &raw_tag,
                &ok,
                std::chrono::system_clock::now() + std::chrono::seconds{1});

            if (result == grpc::CompletionQueue::GOT_EVENT) {
                dispatchCompletion(raw_tag, ok);
            } else if (result == grpc::CompletionQueue::SHUTDOWN) {
                break;
            }

            drainSessionControl();
            checkHeartbeat();

            if (shutdown_requested_.load(std::memory_order_acquire)) {
                initiateShutdown();
            }
        }
    } catch (...) {
        failure = std::current_exception();
        shutdown_requested_.store(true, std::memory_order_release);
        initiateShutdown();

        void* raw_tag = nullptr;
        bool ok = false;
        while (completion_queue_->Next(&raw_tag, &ok)) {
        }
    }

    if (failure) {
        std::rethrow_exception(failure);
    }
}

void GrpcServerTransport::Stop() {
    requestShutdown();
}

void GrpcServerTransport::NotifyOutboundReady() {
    if (outbound_wake_pending_.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    std::scoped_lock lock{lifecycle_mutex_};
    if (completion_queue_ == nullptr ||
        outbound_wake_tag_ == nullptr || shutdown_started_) {
        outbound_wake_pending_.store(false, std::memory_order_release);
        return;
    }

    outbound_wake_alarm_.Set(
        completion_queue_.get(),
        std::chrono::system_clock::now(),
        outbound_wake_tag_.get());
}

TickToOrderStatistics
GrpcServerTransport::GetTickToOrderStatistics() const {
    return tick_to_order_histogram_.GetStatistics();
}

const TickToOrderHistogram&
GrpcServerTransport::GetTickToOrderHistogram() const noexcept {
    return tick_to_order_histogram_;
}

void GrpcServerTransport::dispatchCompletion(void* raw_tag, const bool ok) {
    if (raw_tag == nullptr) {
        throw std::runtime_error("gRPC completion returned a null tag");
    }
    static_cast<CompletionTag*>(raw_tag)->Complete(ok);
}

void GrpcServerTransport::requestShutdown() {
    if (shutdown_requested_.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    NotifyOutboundReady();
}

void GrpcServerTransport::initiateShutdown() {
    std::scoped_lock lock{lifecycle_mutex_};
    if (shutdown_started_) {
        return;
    }

    shutdown_started_ = true;
    if (server_ != nullptr) {
        server_->Shutdown(std::chrono::system_clock::now());
    }
    if (completion_queue_ != nullptr) {
        completion_queue_->Shutdown();
    }
}

void GrpcServerTransport::onOutboundWake(const bool ok) {
    outbound_wake_pending_.store(false, std::memory_order_release);
    if (!ok || shutdown_requested_.load(std::memory_order_acquire)) {
        return;
    }
    drainEgress();
}

void GrpcServerTransport::drainEgress() {
    if (oe_connection_ == nullptr || !oe_connection_->CanWrite()) {
        return;
    }

    OutboundMessage outbound{};
    while (egress_.pop(outbound)) {
        oe_connection_->Enqueue(OeConnection::PendingWrite{
            .message = ToGrpcMessage(outbound.message),
            .trigger_received_at_ns = outbound.trigger_received_at_ns,
            .measure_tick_to_order = outbound.measure_tick_to_order,
            .close_after_write = false,
        });
    }
}

void GrpcServerTransport::drainSessionControl() {
    std::array<std::byte, 64> buffer{};
    while (!shutdown_requested_.load(std::memory_order_acquire)) {
        const ::ssize_t received = session_control_listener_.RecvDatagram(buffer);
        if (received > 0) {
            processSessionCommand(std::string_view{
                reinterpret_cast<const char*>(buffer.data()),
                static_cast<std::size_t>(received)});
            continue;
        }
        if (received == 0) {
            continue;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
        throw std::system_error(
            errno,
            std::generic_category(),
            "session-control recv failed");
    }
}

void GrpcServerTransport::processSessionCommand(const std::string_view command) {
    if (command == "HALT") {
        queueSessionControl(codec::SessionState::halt);
    } else if (command == "OPEN") {
        queueSessionControl(codec::SessionState::open);
    } else if (command == "CLOSE") {
        queueSessionControl(codec::SessionState::close);
    } else {
        spdlog::warn("Unknown session command: {}", command);
    }
}

void GrpcServerTransport::queueSessionControl(const codec::SessionState state) {
    if (oe_connection_ == nullptr || !oe_connection_->CanWrite()) {
        if (state == codec::SessionState::close) {
            requestShutdown();
        }
        return;
    }

    const codec::SessionControlMessage control{
        .ts_ns = UnixNowNs(),
        .state = state,
    };
    oe_connection_->Enqueue(OeConnection::PendingWrite{
        .message = ToGrpcMessage(control),
        .close_after_write = state == codec::SessionState::close,
    });
}

void GrpcServerTransport::markOeActivity() {
    last_oe_activity_ = std::chrono::steady_clock::now();
    next_heartbeat_ = last_oe_activity_ + heartbeat_interval;
}

void GrpcServerTransport::checkHeartbeat() {
    if (shutdown_requested_.load(std::memory_order_acquire) ||
        oe_connection_ == nullptr || !oe_connection_->CanWrite()) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (now < next_heartbeat_) {
        return;
    }

    const auto silent_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(
            now - last_oe_activity_)
            .count();
    spdlog::warn(
        "OE client has been silent for {} seconds; sending heartbeat",
        silent_seconds);
    queueHeartbeat();
    next_heartbeat_ = now + heartbeat_interval;
}

void GrpcServerTransport::queueHeartbeat() {
    if (oe_connection_ == nullptr || !oe_connection_->CanWrite()) {
        return;
    }
    const codec::HeartbeatMessage heartbeat{.ts_ns = UnixNowNs()};
    oe_connection_->Enqueue(OeConnection::PendingWrite{
        .message = ToGrpcMessage(heartbeat),
    });
}

void GrpcServerTransport::recordTickToOrder(
    const std::uint64_t trigger_received_at_ns,
    const std::uint64_t send_started_at_ns,
    const bool measure_tick_to_order) {
    if (!measure_tick_to_order) {
        return;
    }

    if (send_started_at_ns >= trigger_received_at_ns) {
        tick_to_order_histogram_.Record(
            send_started_at_ns - trigger_received_at_ns);
    }
}

}
