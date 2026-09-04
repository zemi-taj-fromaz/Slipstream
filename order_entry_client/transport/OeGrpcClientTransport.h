#ifndef SLIPSTREAM_OE_GRPC_CLIENT_TRANSPORT_H
#define SLIPSTREAM_OE_GRPC_CLIENT_TRANSPORT_H

#include "client_transport.h"
#include "OeLatencyRecorder.h"
#include "slipstream.grpc.pb.h"

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>

#include <spdlog/logger.h>

class OeGrpcClientTransport final : public IClientTransport {
public:
    OeGrpcClientTransport(
        const std::string& host,
        std::uint16_t port,
        spdlog::logger& logger,
        OeLatencyRecorder& latency_recorder);
    ~OeGrpcClientTransport() override;

    utils::ConnectionResult Send(const MarketEvent& event) override;
    utils::ConnectionResult ProcessInboundUntil(
        std::chrono::steady_clock::time_point deadline) override;
    utils::ConnectionResult Finish() override;

private:
    enum class Operation {
        Connect,
        Read,
        Write,
        WritesDone,
        Finish,
    };

    struct Tag {
        Operation operation;
    };

    void Dispatch(Tag* tag, bool ok);
    void StartRead();
    void StartWrite();
    void StartWritesDone();
    void TryStartFinish();
    void HandleInbound();
    void DrainCompletionQueue();

    spdlog::logger& logger_;
    OeLatencyRecorder& latency_recorder_;
    grpc::ClientContext context_;
    grpc::CompletionQueue completion_queue_;
    grpc::Status final_status_;
    std::unique_ptr<slipstream::grpc_api::OrderEntry::Stub> stub_;
    std::unique_ptr<grpc::ClientAsyncReaderWriter<
        slipstream::grpc_api::OeClientMessage,
        slipstream::grpc_api::OeServerMessage>> stream_;

    slipstream::grpc_api::OeServerMessage inbound_;
    slipstream::grpc_api::OeClientMessage current_write_;
    std::deque<slipstream::grpc_api::OeClientMessage> pending_writes_;

    Tag connect_tag_{Operation::Connect};
    Tag read_tag_{Operation::Read};
    Tag write_tag_{Operation::Write};
    Tag writes_done_tag_{Operation::WritesDone};
    Tag finish_tag_{Operation::Finish};

    slipstream::grpc_api::SessionControl::State session_state_{
        slipstream::grpc_api::SessionControl::OPEN};
    bool connected_{false};
    bool read_in_flight_{false};
    bool read_closed_{false};
    bool write_in_flight_{false};
    bool finish_requested_{false};
    bool writes_done_started_{false};
    bool writes_done_completed_{false};
    bool finish_started_{false};
    bool finish_completed_{false};
    bool peer_disconnected_{false};
};

#endif
