#ifndef SLIPSTREAM_TCPPOLLSERVERTRANSPORT_H
#define SLIPSTREAM_TCPPOLLSERVERTRANSPORT_H

#include "EncodedFrame.h"
#include "ExecutionReport.h"
#include "IServerTransport.h"
#include "Queues.h"
#include "SlipstreamConfig.h"
#include "socket.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <vector>

class IMsgController;

namespace slipstream {

class TcpPollServerTransport final : public IServerTransport {
public:
    explicit TcpPollServerTransport(const SlipstreamConfig& config);
    TcpPollServerTransport(
        const SlipstreamConfig& config,
        MarketEventQueue& in,
        OrderEntryQueue& out,
        std::atomic<std::uint64_t>& ingress_generation);
    ~TcpPollServerTransport() override;

    TcpPollServerTransport(const TcpPollServerTransport&) = delete;
    TcpPollServerTransport& operator=(const TcpPollServerTransport&) = delete;
    TcpPollServerTransport(TcpPollServerTransport&&) = delete;
    TcpPollServerTransport& operator=(TcpPollServerTransport&&) = delete;

    void Run() override;
    void Stop() override;
    void NotifyOutboundReady() override;

    [[nodiscard]]
    TickToOrderStatistics GetTickToOrderStatistics() const;

private:
    void resetWakeNotif();
    void drainEgress();
    void flushSendQueue(utils::Socket<utils::SockType::Tcp>& oe_client);
    void markOeActivity();
    void checkHeartbeat();
    void queueHeartbeat();
    void queueSessionControl(codec::SessionState state);
    void recvMarketEvent(
        utils::Socket<utils::SockType::Tcp>& client,
        codec::MarketEventDecoder& decoder,
        IMsgController& controller);
    void recvSessionControl(
        utils::Socket<utils::SockType::Udp>& client);

    utils::Socket<utils::SockType::Tcp> md_listener{};
    utils::Socket<utils::SockType::Tcp> oe_listener{};
    utils::Socket<utils::SockType::Udp> session_control_listener{};
    const SlipstreamConfig& config_;
    std::atomic_bool alive{true};

    MarketEventQueue* ingress{nullptr};
    OrderEntryQueue* egress{nullptr};
    std::atomic<std::uint64_t>* ingress_generation{nullptr};

    codec::MarketEventDecoder md_decoder{};
    codec::MarketEventDecoder oe_decoder{};

    std::deque<EncodedFrame> send_queue;
    std::vector<std::uint64_t> tick_to_order_samples;
    int wake_fd{-1};

    std::chrono::steady_clock::time_point last_oe_activity{};
    std::chrono::steady_clock::time_point next_heartbeat{};
    static constexpr auto heartbeat_interval =
        std::chrono::seconds{5};
};

}

#endif
