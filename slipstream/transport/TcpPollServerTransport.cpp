#include "TcpPollServerTransport.h"

#include "message_processor.h"
#include "replay_start.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <memory>
#include <poll.h>
#include <span>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <sys/eventfd.h>
#include <type_traits>
#include <unistd.h>
#include <utility>
#include <variant>

namespace slipstream {

namespace {

std::uint64_t MonotonicNowNs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

}

TcpPollServerTransport::TcpPollServerTransport(
    const SlipstreamConfig& config)
    : config_{config},
      wake_fd{::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)} {
    if (wake_fd == -1) {
        throw std::system_error(
            errno,
            std::generic_category(),
            "eventfd() failed");
    }

}

TcpPollServerTransport::TcpPollServerTransport(
    const SlipstreamConfig& config,
    MarketEventQueue& in,
    OrderEntryQueue& out,
    std::atomic<std::uint64_t>& generation)
    : TcpPollServerTransport(config) {
    ingress = &in;
    egress = &out;
    ingress_generation = &generation;
}

TcpPollServerTransport::~TcpPollServerTransport() {
    if (wake_fd != -1) {
        ::close(wake_fd);
    }
}

void TcpPollServerTransport::Run() {
    constexpr int receive_buffer_size = 1024 * 1024;
    constexpr int send_buffer_size = 1024 * 1024;

    IMsgController quiet_controller;
    IMsgController* md_controller = &quiet_controller;
    IMsgController* oe_controller = &quiet_controller;

    std::unique_ptr<CanonicalFileMsgController> received_quotes;
    std::unique_ptr<CanonicalFileMsgController> received_trades;

    if (utils::ReplayVerificationEnabled()) {
        const std::string received_quotes_path =
            std::string{SLIPSTREAM_VERIFICATION_DIR} +
            "/received_quotes.csv";
        const std::string received_trades_path =
            std::string{SLIPSTREAM_VERIFICATION_DIR} +
            "/received_trades.csv";

        received_quotes = std::make_unique<CanonicalFileMsgController>(
            received_quotes_path.c_str());
        received_trades = std::make_unique<CanonicalFileMsgController>(
            received_trades_path.c_str());
        md_controller = received_quotes.get();
        oe_controller = received_trades.get();
    }

    md_listener.SetReuseAddress();
    md_listener.SetReceiveBufferSize(receive_buffer_size);
    md_listener.Bind(config_.md_host.c_str(), config_.md_port);
    md_listener.Listen();

    oe_listener.SetReuseAddress();
    oe_listener.SetReceiveBufferSize(receive_buffer_size);
    oe_listener.Bind(config_.oe_host.c_str(), config_.oe_port);
    oe_listener.Listen();

    session_control_listener.SetReuseAddress();
    session_control_listener.SetReceiveBufferSize(receive_buffer_size);
    session_control_listener.Bind("127.0.0.1", 9099);

    auto md_client = md_listener.Accept();
    auto oe_client = oe_listener.Accept();
    oe_client.SetTcpNoDelay();
    oe_client.SetSendBufferSize(send_buffer_size);

    markOeActivity();

    constexpr std::size_t md_index = 0;
    constexpr std::size_t oe_index = 1;
    constexpr std::size_t wake_index = 2;
    constexpr std::size_t session_control_index = 3;

    std::array<pollfd, 4> poll_fds{{
        {.fd = md_client.NativeHandle(), .events = POLLIN, .revents = 0},
        {.fd = oe_client.NativeHandle(), .events = POLLIN, .revents = 0},
        {.fd = wake_fd, .events = POLLIN, .revents = 0},
        {
            .fd = session_control_listener.NativeHandle(),
            .events = POLLIN,
            .revents = 0
        }
    }};

    while (alive.load(std::memory_order_acquire)) {
        poll_fds[md_index].events = POLLIN;
        poll_fds[oe_index].events = POLLIN;
        poll_fds[wake_index].events = POLLIN;
        poll_fds[session_control_index].events = POLLIN;

        if (!send_queue.empty()) {
            poll_fds[oe_index].events |= POLLOUT;
        }

        for (auto& poll_fd : poll_fds) {
            poll_fd.revents = 0;
        }

        constexpr std::int32_t poll_timeout_ms = 1000;
        const int ready = ::poll(
            poll_fds.data(),
            static_cast<nfds_t>(poll_fds.size()),
            poll_timeout_ms);

        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }

            throw std::system_error(
                errno,
                std::generic_category(),
                "poll() failed");
        }

        if (poll_fds[wake_index].revents & POLLIN) {
            resetWakeNotif();
            drainEgress();
        }
        if (poll_fds[md_index].revents & POLLIN) {
            recvMarketEvent(md_client, md_decoder, *md_controller);
        }
        if (poll_fds[oe_index].revents & POLLIN) {
            recvMarketEvent(oe_client, oe_decoder, *oe_controller);
        }
        if (poll_fds[session_control_index].revents & POLLIN) {
            recvSessionControl(session_control_listener);
        }
        if (poll_fds[oe_index].revents & POLLOUT) {
            flushSendQueue(oe_client);
        }

        checkHeartbeat();
    }
}

void TcpPollServerTransport::Stop() {
    alive.store(false, std::memory_order_release);
    NotifyOutboundReady();
}

void TcpPollServerTransport::recvMarketEvent(
    utils::Socket<utils::SockType::Tcp>& client,
    codec::MarketEventDecoder& decoder,
    IMsgController& controller) {
    std::array<std::byte, 4096> recv_buffer;

    while (true) {
        const ::ssize_t recvd = client.Recv(recv_buffer);
        if (recvd > 0) {
            const std::uint64_t received_at_ns = MonotonicNowNs();
            const std::span<const std::byte> received_bytes{
                recv_buffer.data(),
                static_cast<std::size_t>(recvd)};

            std::vector<MarketEvent> recv_messages;
            const auto result = decoder.Decode(
                received_bytes,
                recv_messages);

            for (auto& recv_message : recv_messages) {
                if (std::strcmp(
                        recv_message.symbol,
                        config_.symbol.c_str()) != 0) {
                    continue;
                }

                if (std::holds_alternative<Trade>(
                        recv_message.payload)) {
                    markOeActivity();
                }

                if (ingress != nullptr) {
                    const InboundEvent inbound{
                        .message = recv_message,
                        .received_at_ns = received_at_ns,
                    };
                    if (!ingress->push(inbound)) {
                        throw std::runtime_error(
                            "failed to enqueue MarketEvent");
                    }

                    ingress_generation->fetch_add(
                        1,
                        std::memory_order_release);
                    ingress_generation->notify_one();
                }

                controller.Sink(recv_message);
            }

            if (result.status == codec::DecodeStatus::error) {
                throw std::runtime_error(
                    "invalid server inbound frame");
            }

            continue;
        }

        if (recvd == 0) {
            alive.store(false, std::memory_order_release);
            return;
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
            "market-data recv() failed");
    }
}

void TcpPollServerTransport::recvSessionControl(
    utils::Socket<utils::SockType::Udp>& client) {
    std::array<std::byte, 64> recv_buffer;

    while (true) {
        const ::ssize_t recvd = client.RecvDatagram(recv_buffer);
        if (recvd > 0) {
            const std::string_view command{
                reinterpret_cast<const char*>(recv_buffer.data()),
                static_cast<std::size_t>(recvd)};

            if (command == "HALT") {
                queueSessionControl(codec::SessionState::halt);
            } else if (command == "OPEN") {
                queueSessionControl(codec::SessionState::open);
            } else if (command == "CLOSE") {
                queueSessionControl(codec::SessionState::close);
            } else {
                spdlog::warn("Unknown session command: {}", command);
            }

            continue;
        }

        if (recvd == 0) {
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
            "session-control recv() failed");
    }
}

void TcpPollServerTransport::NotifyOutboundReady() {
    const std::uint64_t signal = 1;

    while (true) {
        const ::ssize_t result = ::write(
            wake_fd,
            &signal,
            sizeof(signal));

        if (result == sizeof(signal)) {
            return;
        }
        if (result == -1 && errno == EINTR) {
            continue;
        }
        if (result == -1 && errno == EAGAIN) {
            return;
        }

        throw std::system_error(
            errno,
            std::generic_category(),
            "failed to signal eventfd");
    }
}

void TcpPollServerTransport::drainEgress() {
    if (egress == nullptr) {
        return;
    }

    OutboundMessage outbound{};
    while (egress->pop(outbound)) {
        EncodedFrame frame{};
        frame.trigger_received_at_ns =
            outbound.trigger_received_at_ns;
        frame.measure_tick_to_order =
            outbound.measure_tick_to_order;

        std::visit(
            [&frame](const auto& value) {
                using Message = std::decay_t<decltype(value)>;

                if constexpr (
                    std::is_same_v<Message, codec::NewOrderMessage>) {
                    frame.size = codec::EncodeNewOrder(value, frame.bytes);
                } else if constexpr (
                    std::is_same_v<Message, codec::ExecReportMessage>) {
                    frame.size = codec::EncodeExecReport(value, frame.bytes);
                } else if constexpr (
                    std::is_same_v<Message, codec::HeartbeatMessage>) {
                    frame.size = codec::EncodeHeartbeat(value, frame.bytes);
                } else if constexpr (
                    std::is_same_v<Message, codec::SessionControlMessage>) {
                    frame.size = codec::EncodeSessionControl(value, frame.bytes);
                }
            },
            outbound.message);

        send_queue.push_back(std::move(frame));
    }
}

void TcpPollServerTransport::flushSendQueue(
    utils::Socket<utils::SockType::Tcp>& oe_client) {
    while (!send_queue.empty()) {
        EncodedFrame& frame = send_queue.front();
        const std::uint64_t send_started_at_ns = MonotonicNowNs();
        if (oe_client.SendAll(frame.remainingBytes()) ==
            utils::ConnectionResult::PeerDisconnected) {
            alive.store(false, std::memory_order_release);
            return;
        }

        if (frame.measure_tick_to_order) {
            if (send_started_at_ns >= frame.trigger_received_at_ns) {
                tick_to_order_histogram.Record(
                    send_started_at_ns - frame.trigger_received_at_ns);
            }
        }

        send_queue.pop_front();
    }
}

TickToOrderStatistics
TcpPollServerTransport::GetTickToOrderStatistics() const {
    return tick_to_order_histogram.GetStatistics();
}

const TickToOrderHistogram&
TcpPollServerTransport::GetTickToOrderHistogram() const noexcept {
    return tick_to_order_histogram;
}

void TcpPollServerTransport::markOeActivity() {
    last_oe_activity = std::chrono::steady_clock::now();
    next_heartbeat = last_oe_activity + heartbeat_interval;
}

void TcpPollServerTransport::checkHeartbeat() {
    if (!alive.load(std::memory_order_acquire)) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (now < next_heartbeat) {
        return;
    }

    const auto silent_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(
            now - last_oe_activity)
            .count();

    spdlog::warn(
        "OE client has been silent for {} seconds; sending heartbeat",
        silent_seconds);

    queueHeartbeat();
    next_heartbeat = now + heartbeat_interval;
}

void TcpPollServerTransport::queueHeartbeat() {
    const auto unix_time =
        std::chrono::system_clock::now().time_since_epoch();
    const auto timestamp_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(unix_time)
            .count();

    const codec::HeartbeatMessage heartbeat{
        .ts_ns = static_cast<std::uint64_t>(timestamp_ns),
    };

    EncodedFrame frame{};
    frame.size = codec::EncodeHeartbeat(heartbeat, frame.bytes);
    send_queue.push_back(std::move(frame));
}

void TcpPollServerTransport::queueSessionControl(
    codec::SessionState state) {
    const auto unix_time =
        std::chrono::system_clock::now().time_since_epoch();
    const auto timestamp_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(unix_time)
            .count();

    const codec::SessionControlMessage session_control{
        .ts_ns = static_cast<std::uint64_t>(timestamp_ns),
        .state = state
    };

    EncodedFrame frame{};
    frame.size = codec::EncodeSessionControl(
        session_control,
        frame.bytes);
    send_queue.push_back(std::move(frame));
}

void TcpPollServerTransport::resetWakeNotif() {
    std::uint64_t notification_count{};

    const ::ssize_t result = ::read(
        wake_fd,
        &notification_count,
        sizeof(notification_count));

    if (result == sizeof(notification_count)) {
        return;
    }
    if (result == -1 &&
        (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return;
    }

    throw std::system_error(
        errno,
        std::generic_category(),
        "failed to reset eventfd notification");
}

}
