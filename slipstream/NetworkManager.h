//
// Created by babodev on 16.08.2026..
//

#ifndef SLIPSTREAM_NETWORKMANAGER_H
#define SLIPSTREAM_NETWORKMANAGER_H

#include "socket.h"

#include <queue>
#include "EncodedFrame.h"
#include <sys/eventfd.h>
#include <rigtorp/SPSCQueue.h>

namespace slipstream {
    class NetworkManager {
    public:
        NetworkManager(rigtorp::SPSCQueue<MarketEvent>& in, rigtorp::SPSCQueue<codec::OrderMessage>& out);
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
                sizeof(signal)):
        }
    private:

        void resetWakeNotif();

        void recvMarketEvent(utils::Socket& client);

        utils::Socket md_listener{};
        utils::Socket oe_listener{};
        bool alive{true};

        rigtorp::SPSCQueue<MarketEvent>& ingress;
        rigtorp::SPSCQueue<codec::OrderMessage>& egress;

        codec::MarketDataStreamDecoder md_decoder{};

        std::deque<EncodedFrame> send_queue;
        int wake_fd{-1};
    };
}


#endif //SLIPSTREAM_NETWORKMANAGER_H
