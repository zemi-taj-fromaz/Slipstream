#include "OeGrpcClientTransport.h"

#include <grpc/support/time.h>

#include <chrono>
#include <stdexcept>
#include <utility>
#include <variant>

namespace {

std::string Address(const std::string& host, const std::uint16_t port) {
    return host + ':' + std::to_string(port);
}

slipstream::grpc_api::Trade::Aggressor ToGrpcAggressor(
    const char aggressor) {
    if (aggressor == 'B') {
        return slipstream::grpc_api::Trade::AGGRESSOR_BUY;
    }
    if (aggressor == 'S') {
        return slipstream::grpc_api::Trade::AGGRESSOR_SELL;
    }
    return slipstream::grpc_api::Trade::AGGRESSOR_UNKNOWN;
}

}

OeGrpcClientTransport::OeGrpcClientTransport(
    const std::string& host,
    const std::uint16_t port,
    spdlog::logger& logger)
    : logger_{logger} {
    auto channel = grpc::CreateChannel(
        Address(host, port),
        grpc::InsecureChannelCredentials());
    if (!channel->WaitForConnected(
            std::chrono::system_clock::now() + std::chrono::seconds{5})) {
        throw std::runtime_error("failed to connect OE gRPC channel");
    }

    stub_ = slipstream::grpc_api::OrderEntry::NewStub(channel);
    stream_ = stub_->AsyncConnect(
        &context_,
        &completion_queue_,
        &connect_tag_);

    void* raw_tag = nullptr;
    bool ok = false;
    if (!completion_queue_.Next(&raw_tag, &ok) ||
        raw_tag != &connect_tag_ || !ok) {
        context_.TryCancel();
        completion_queue_.Shutdown();
        DrainCompletionQueue();
        throw std::runtime_error("failed to start OE gRPC stream");
    }

    connected_ = true;
    StartRead();
}

OeGrpcClientTransport::~OeGrpcClientTransport() {
    if (!finish_completed_) {
        context_.TryCancel();
    }
    completion_queue_.Shutdown();
    DrainCompletionQueue();
}

utils::ConnectionResult OeGrpcClientTransport::Send(
    const MarketEvent& event) {
    if (peer_disconnected_ ||
        session_state_ == slipstream::grpc_api::SessionControl::CLOSE) {
        return utils::ConnectionResult::PeerDisconnected;
    }
    if (session_state_ == slipstream::grpc_api::SessionControl::HALT) {
        return utils::ConnectionResult::Complete;
    }

    const Trade* trade = std::get_if<Trade>(&event.payload);
    if (trade == nullptr) {
        throw std::invalid_argument("OE gRPC transport requires a trade");
    }

    slipstream::grpc_api::OeClientMessage message;
    auto* output = message.mutable_trade();
    output->set_ts_ns(event.ts);
    output->set_symbol(event.symbol);
    output->set_price(trade->price);
    output->set_id(trade->id);
    output->set_qty(trade->qty);
    output->set_aggressor(ToGrpcAggressor(trade->aggressor));
    pending_writes_.push_back(std::move(message));
    StartWrite();
    return utils::ConnectionResult::Complete;
}

utils::ConnectionResult OeGrpcClientTransport::ProcessInboundUntil(
    const std::chrono::steady_clock::time_point deadline) {
    while (!peer_disconnected_) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            return utils::ConnectionResult::Complete;
        }

        const auto remaining =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                deadline - now);
        const gpr_timespec grpc_deadline = gpr_time_add(
            gpr_now(GPR_CLOCK_MONOTONIC),
            gpr_time_from_nanos(remaining.count(), GPR_TIMESPAN));

        void* raw_tag = nullptr;
        bool ok = false;
        const auto result = completion_queue_.AsyncNext(
            &raw_tag,
            &ok,
            grpc_deadline);
        if (result == grpc::CompletionQueue::TIMEOUT) {
            return utils::ConnectionResult::Complete;
        }
        if (result == grpc::CompletionQueue::SHUTDOWN) {
            peer_disconnected_ = true;
            break;
        }
        Dispatch(static_cast<Tag*>(raw_tag), ok);
    }

    return utils::ConnectionResult::PeerDisconnected;
}

utils::ConnectionResult OeGrpcClientTransport::Finish() {
    if (finish_completed_) {
        return final_status_.ok()
            ? utils::ConnectionResult::Complete
            : utils::ConnectionResult::PeerDisconnected;
    }

    finish_requested_ = true;
    StartWritesDone();

    while (!writes_done_started_) {
        void* raw_tag = nullptr;
        bool ok = false;
        if (!completion_queue_.Next(&raw_tag, &ok)) {
            peer_disconnected_ = true;
            return utils::ConnectionResult::PeerDisconnected;
        }
        Dispatch(static_cast<Tag*>(raw_tag), ok);
    }

    while (!finish_completed_) {
        void* raw_tag = nullptr;
        bool ok = false;
        if (!completion_queue_.Next(&raw_tag, &ok)) {
            peer_disconnected_ = true;
            break;
        }
        Dispatch(static_cast<Tag*>(raw_tag), ok);
    }

    return final_status_.ok()
        ? utils::ConnectionResult::Complete
        : utils::ConnectionResult::PeerDisconnected;
}

void OeGrpcClientTransport::Dispatch(Tag* tag, const bool ok) {
    if (tag == nullptr) {
        throw std::runtime_error("OE gRPC completion returned a null tag");
    }

    switch (tag->operation) {
    case Operation::Connect:
        connected_ = ok;
        peer_disconnected_ = !ok;
        break;
    case Operation::Read:
        read_in_flight_ = false;
        if (ok) {
            HandleInbound();
            StartRead();
        } else {
            read_closed_ = true;
            TryStartFinish();
        }
        break;
    case Operation::Write:
        write_in_flight_ = false;
        if (!ok) {
            pending_writes_.clear();
            peer_disconnected_ = true;
        } else {
            StartWrite();
            StartWritesDone();
        }
        break;
    case Operation::WritesDone:
        writes_done_completed_ = true;
        if (!ok) {
            peer_disconnected_ = true;
        }
        TryStartFinish();
        break;
    case Operation::Finish:
        finish_completed_ = true;
        connected_ = false;
        peer_disconnected_ = !final_status_.ok();
        break;
    }
}

void OeGrpcClientTransport::StartRead() {
    if (!connected_ || read_in_flight_ || read_closed_ || finish_started_) {
        return;
    }
    read_in_flight_ = true;
    stream_->Read(&inbound_, &read_tag_);
}

void OeGrpcClientTransport::StartWrite() {
    if (!connected_ || write_in_flight_ || pending_writes_.empty() ||
        writes_done_started_ || finish_started_) {
        return;
    }
    current_write_ = std::move(pending_writes_.front());
    pending_writes_.pop_front();
    write_in_flight_ = true;
    stream_->Write(current_write_, &write_tag_);
}

void OeGrpcClientTransport::StartWritesDone() {
    if (!finish_requested_ || writes_done_started_ || write_in_flight_ ||
        !pending_writes_.empty() || finish_started_) {
        return;
    }
    writes_done_started_ = true;
    stream_->WritesDone(&writes_done_tag_);
}

void OeGrpcClientTransport::TryStartFinish() {
    if (finish_started_ || !read_closed_ || !writes_done_completed_) {
        return;
    }
    finish_started_ = true;
    stream_->Finish(&final_status_, &finish_tag_);
}

void OeGrpcClientTransport::HandleInbound() {
    switch (inbound_.payload_case()) {
    case slipstream::grpc_api::OeServerMessage::kNewOrder: {
        const auto& value = inbound_.new_order();
        logger_.info(
            "NewOrder {} trade_id={} symbol={} side={} qty={} limit_px={} client_order_id={}",
            value.status() == slipstream::grpc_api::NewOrder::ACCEPTED
                ? "ACCEPTED"
                : "REJECTED",
            value.trade_id(),
            value.symbol(),
            value.side() == slipstream::grpc_api::NewOrder::BUY ? 'B' : 'S',
            value.qty(),
            value.limit_price(),
            value.client_order_id());
        break;
    }
    case slipstream::grpc_api::OeServerMessage::kExecReport: {
        const auto& value = inbound_.exec_report();
        logger_.info(
            "ExecReport client_order_id={} status={} filled_qty={} avg_px={} reason_code={} ts_ns={}",
            value.client_order_id(),
            static_cast<unsigned>(value.status()),
            value.filled_qty(),
            value.avg_price(),
            static_cast<unsigned>(value.reason_code()),
            value.ts_ns());
        break;
    }
    case slipstream::grpc_api::OeServerMessage::kHeartbeat:
        logger_.info("Heartbeat ts_ns={}", inbound_.heartbeat().ts_ns());
        break;
    case slipstream::grpc_api::OeServerMessage::kSessionControl:
        session_state_ = inbound_.session_control().state();
        logger_.info(
            "SessionControl state={} ts_ns={}",
            static_cast<unsigned>(session_state_),
            inbound_.session_control().ts_ns());
        if (session_state_ == slipstream::grpc_api::SessionControl::CLOSE) {
            peer_disconnected_ = true;
        }
        break;
    case slipstream::grpc_api::OeServerMessage::PAYLOAD_NOT_SET:
        break;
    }
}

void OeGrpcClientTransport::DrainCompletionQueue() {
    void* raw_tag = nullptr;
    bool ok = false;
    while (completion_queue_.Next(&raw_tag, &ok)) {
    }
}
