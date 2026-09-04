//
// Created by babodev on 16.08.2026..
//

#ifndef SLIPSTREAM_NETWORKMANAGER_H
#define SLIPSTREAM_NETWORKMANAGER_H

#include "SlipstreamConfig.h"
#include "Queues.h"
#include "ExecutionReport.h"
#include "socket.h"

#include <deque>
#include <atomic>
#include <chrono>
#include "EncodedFrame.h"
#include <sys/eventfd.h>
#include <unistd.h>


class IMsgController;

namespace slipstream {
    class NetworkManager {
    public:
        explicit NetworkManager(const SlipstreamConfig& config);
        NetworkManager(
            const SlipstreamConfig& config,
            MarketEventQueue& in,
            OrderEntryQueue& out,
            std::atomic<std::uint64_t>& ingress_generation);
        ~NetworkManager();

        NetworkManager(const NetworkManager&) = delete;
        NetworkManager& operator=(const NetworkManager&) = delete;
        NetworkManager(NetworkManager&&) = delete;
        NetworkManager& operator=(NetworkManager&&) = delete;

        void Run();
        void SignalEvent();
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

        void recvMarketEvent(utils::Socket<utils::SockType::Tcp>& client, codec::MarketEventDecoder& decoder, IMsgController& controller);
        void recvSessionControl(utils::Socket<utils::SockType::Udp>& client);

        utils::Socket<utils::SockType::Tcp> md_listener{};
        utils::Socket<utils::SockType::Tcp> oe_listener{};
        utils::Socket<utils::SockType::Udp> session_control_listener{};
        const SlipstreamConfig& config_;
        bool alive{true};

        MarketEventQueue* ingress{nullptr};
        OrderEntryQueue* egress{nullptr};
        std::atomic<std::uint64_t>* ingress_generation{nullptr};

        codec::MarketEventDecoder md_decoder{};
        codec::MarketEventDecoder oe_decoder{};
       // codec::SessionControlDecoder session_control_decoder{};

        std::deque<EncodedFrame> send_queue;
        TickToOrderHistogram tick_to_order_histogram;
        int wake_fd{-1};

        std::chrono::steady_clock::time_point last_oe_activity{};
        std::chrono::steady_clock::time_point next_heartbeat{};
        static constexpr auto heartbeat_interval =
            std::chrono::seconds{5};
    };
}


#endif //SLIPSTREAM_NETWORKMANAGER_H
