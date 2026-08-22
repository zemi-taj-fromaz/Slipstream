//
// Created by babodev on 16.08.2026..
//

#ifndef SLIPSTREAM_NETWORKMANAGER_H
#define SLIPSTREAM_NETWORKMANAGER_H

#include "SlipstreamConfig.h"
#include "socket.h"

#include <queue>
#include <atomic>
#include <chrono>
#include "EncodedFrame.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <rigtorp/SPSCQueue.h>

class IMsgController;

namespace slipstream {
    class NetworkManager {
    public:
        explicit NetworkManager(const SlipstreamConfig& config);
        NetworkManager(
            const SlipstreamConfig& config,
            rigtorp::SPSCQueue<MarketEvent>& in,
            rigtorp::SPSCQueue<codec::OrderEntryClientMessage>& out,
            std::atomic<std::uint64_t>& ingress_generation);
        ~NetworkManager();

        NetworkManager(const NetworkManager&) = delete;
        NetworkManager& operator=(const NetworkManager&) = delete;
        NetworkManager(NetworkManager&&) = delete;
        NetworkManager& operator=(NetworkManager&&) = delete;

        void Process();
        void SignalEvent();
    private:

        void resetWakeNotif();
        void drainEgress();
        void flushSendQueue(utils::Socket& oe_client);
        void markOeActivity();
        void checkHeartbeat();
        void queueHeartbeat();

        void recvMarketEvent(
            utils::Socket& client,
            codec::ServerSideDecoder& decoder,
            IMsgController& controller);

        utils::Socket md_listener{};
        utils::Socket oe_listener{};
        const SlipstreamConfig& config_;
        bool alive{true};

        rigtorp::SPSCQueue<MarketEvent>* ingress{nullptr};
        rigtorp::SPSCQueue<codec::OrderEntryClientMessage>* egress{nullptr};
        std::atomic<std::uint64_t>* ingress_generation{nullptr};

        codec::ServerSideDecoder md_decoder{};
        codec::ServerSideDecoder oe_decoder{};

        std::deque<EncodedFrame> send_queue;
        int wake_fd{-1};

        std::chrono::steady_clock::time_point last_oe_activity{};
        std::chrono::steady_clock::time_point next_heartbeat{};
        static constexpr auto heartbeat_interval =
            std::chrono::seconds{5};
    };
}


#endif //SLIPSTREAM_NETWORKMANAGER_H
