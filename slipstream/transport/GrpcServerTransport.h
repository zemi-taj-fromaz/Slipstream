#ifndef SLIPSTREAM_GRPCSERVERTRANSPORT_H
#define SLIPSTREAM_GRPCSERVERTRANSPORT_H

#include "ExecutionReport.h"
#include "IServerTransport.h"
#include "Queues.h"
#include "SlipstreamConfig.h"
#include "slipstream.grpc.pb.h"
#include "socket.h"

#include <grpcpp/alarm.h>
#include <grpcpp/grpcpp.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>

namespace slipstream {

class GrpcServerTransport final : public IServerTransport {
public:
    GrpcServerTransport(
        const SlipstreamConfig& config,
        MarketEventQueue& ingress,
        OrderEntryQueue& egress,
        std::atomic<std::uint64_t>& ingress_generation);
    ~GrpcServerTransport() override;

    GrpcServerTransport(const GrpcServerTransport&) = delete;
    GrpcServerTransport& operator=(const GrpcServerTransport&) = delete;
    GrpcServerTransport(GrpcServerTransport&&) = delete;
    GrpcServerTransport& operator=(GrpcServerTransport&&) = delete;

    void Run() override;
    void Stop() override;
    void NotifyOutboundReady() override;

    [[nodiscard]]
    TickToOrderStatistics GetTickToOrderStatistics() const override;

    [[nodiscard]]
    const TickToOrderHistogram&
    GetTickToOrderHistogram() const noexcept override;

private:
    class CompletionTag;
    class OutboundWakeTag;
    class MdConnection;
    class OeConnection;

    friend class OutboundWakeTag;
    friend class MdConnection;
    friend class OeConnection;

    void dispatchCompletion(void* raw_tag, bool ok);
    void requestShutdown();
    void initiateShutdown();
    void onOutboundWake(bool ok);
    void drainEgress();
    void drainSessionControl();
    void processSessionCommand(std::string_view command);
    void queueSessionControl(codec::SessionState state);
    void markOeActivity();
    void checkHeartbeat();
    void queueHeartbeat();
    void recordTickToOrder(
        std::uint64_t trigger_received_at_ns,
        std::uint64_t send_started_at_ns,
        bool measure_tick_to_order);

    const SlipstreamConfig& config_;
    MarketEventQueue& ingress_;
    OrderEntryQueue& egress_;
    std::atomic<std::uint64_t>& ingress_generation_;

    grpc_api::MarketData::AsyncService market_data_service_;
    grpc_api::OrderEntry::AsyncService order_entry_service_;
    std::unique_ptr<grpc::ServerCompletionQueue> completion_queue_;
    std::unique_ptr<grpc::Server> server_;

    std::unique_ptr<OutboundWakeTag> outbound_wake_tag_;
    std::unique_ptr<MdConnection> md_connection_;
    std::unique_ptr<OeConnection> oe_connection_;

    grpc::Alarm outbound_wake_alarm_;
    std::atomic_bool outbound_wake_pending_{false};
    std::atomic_bool shutdown_requested_{false};
    bool shutdown_started_{false};
    std::mutex lifecycle_mutex_;

    utils::UdpSocket session_control_listener_;
    std::chrono::steady_clock::time_point last_oe_activity_{};
    std::chrono::steady_clock::time_point next_heartbeat_{};
    static constexpr auto heartbeat_interval = std::chrono::seconds{5};

    TickToOrderHistogram tick_to_order_histogram_;
};

}

#endif
