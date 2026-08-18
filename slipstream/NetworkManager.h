//
// Created by babodev on 16.08.2026..
//

#ifndef SLIPSTREAM_NETWORKMANAGER_H
#define SLIPSTREAM_NETWORKMANAGER_H

#include "SlipstreamConfig.h"
#include "socket.h"

#include <queue>
#include "EncodedFrame.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <rigtorp/SPSCQueue.h>

class IProcessMsgClass;

namespace slipstream {
    class NetworkManager {
    public:
        explicit NetworkManager(const SlipstreamConfig& config);
        NetworkManager(
            const SlipstreamConfig& config,
            rigtorp::SPSCQueue<MarketEvent>& in,
            rigtorp::SPSCQueue<codec::OrderEntryClientMessage>& out);
        ~NetworkManager();

        NetworkManager(const NetworkManager&) = delete;
        NetworkManager& operator=(const NetworkManager&) = delete;
        NetworkManager(NetworkManager&&) = delete;
        NetworkManager& operator=(NetworkManager&&) = delete;

        void Process();
        void SignalEvent() {
            const std::uint64_t signal = 1;
            if (wake_fd == -1) {
                throw std::runtime_error("eventfd() failed");
            }
            ::write(
                wake_fd,
                &signal,
                sizeof(signal));
        }
    private:

        void resetWakeNotif();

        void recvMarketEvent(
            utils::Socket& client,
            codec::ServerSideDecoder& decoder,
            IProcessMsgClass& processor);

        utils::Socket md_listener{};
        utils::Socket oe_listener{};
        const SlipstreamConfig& config_;
        bool alive{true};

        rigtorp::SPSCQueue<MarketEvent>* ingress{nullptr};
        rigtorp::SPSCQueue<codec::OrderEntryClientMessage>* egress{nullptr};

        codec::ServerSideDecoder md_decoder{};
        codec::ServerSideDecoder oe_decoder{};

        std::deque<EncodedFrame> send_queue;
        int wake_fd{-1};
    };
}


#endif //SLIPSTREAM_NETWORKMANAGER_H
