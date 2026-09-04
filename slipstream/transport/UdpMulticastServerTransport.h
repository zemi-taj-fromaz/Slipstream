#ifndef SLIPSTREAM_UDPMULTICASTSERVERTRANSPORT_H
#define SLIPSTREAM_UDPMULTICASTSERVERTRANSPORT_H

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

class IMsgController;

namespace slipstream {

class UdpMulticastServerTransport final : public IServerTransport {
public:
    explicit UdpMulticastServerTransport(const SlipstreamConfig& config);
    UdpMulticastServerTransport(
        const SlipstreamConfig& config,
        MarketEventQueue& in,
        OrderEntryQueue& out,
        std::atomic<std::uint64_t>& ingress_generation);
    ~UdpMulticastServerTransport() override;

    UdpMulticastServerTransport(const UdpMulticastServerTransport&) = delete;
    UdpMulticastServerTransport& operator=(const UdpMulticastServerTransport&) = delete;
    UdpMulticastServerTransport(UdpMulticastServerTransport&&) = delete;
    UdpMulticastServerTransport& operator=(UdpMulticastServerTransport&&) = delete;

    void Run() override;
    void Stop() override;
    void NotifyOutboundReady() override;

    [[nodiscard]]
    TickToOrderStatistics GetTickToOrderStatistics() const override;

    [[nodiscard]]
    const TickToOrderHistogram&
    GetTickToOrderHistogram() const noexcept override;

private:
    void resetWakeNotif();
    void drainEgress();
    void flushSendQueue(utils::TcpSocket& oe_client);
    void markOeActivity();
    void checkHeartbeat();
    void queueHeartbeat();
    void queueSessionControl(codec::SessionState state);
    void recvMulticastMarketData(
        utils::UdpSocket& feed,
        IMsgController& controller);
    void processMulticastMarketData(
        const codec::MulticastMarketDataDatagram& datagram,
        std::uint64_t received_at_ns,
        IMsgController& controller);
    void recvOrderEntry(
        utils::TcpSocket& client,
        IMsgController& controller);
    void recvSessionControl(utils::UdpSocket& client);

    utils::UdpSocket md_feed_a{};
    utils::UdpSocket md_feed_b{};
    utils::TcpSocket oe_listener{};
    utils::UdpSocket session_control_listener{};
    const SlipstreamConfig& config_;
    std::atomic_bool alive{true};

    MarketEventQueue* ingress{nullptr};
    OrderEntryQueue* egress{nullptr};
    std::atomic<std::uint64_t>* ingress_generation{nullptr};

    codec::MarketEventDecoder oe_decoder{};
    std::uint64_t expected_sequence{1};

    std::deque<EncodedFrame> send_queue;
    TickToOrderHistogram tick_to_order_histogram;
    int wake_fd{-1};

    std::chrono::steady_clock::time_point last_oe_activity{};
    std::chrono::steady_clock::time_point next_heartbeat{};
    static constexpr auto heartbeat_interval =
        std::chrono::seconds{5};
};

}

#endif
