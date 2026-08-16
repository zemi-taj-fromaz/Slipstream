//
// Created by babodev on 16.08.2026..
//

#include "NetworkManager.h"

#include <ios>
#include <poll.h>
#include <system_error>
#include <netinet/in.h>
#include <netinet/tcp.h>

namespace slipstream {

    NetworkManager::NetworkManager(
        rigtorp::SPSCQueue<MarketEvent>& in,
        rigtorp::SPSCQueue<codec::OrderEntryClientMessage>& out)
        : ingress(in), egress(out) {}

    NetworkManager::~NetworkManager() {}

    void NetworkManager::Process() {
        constexpr int receive_buffer_size = 1024 * 1024;

        //wait for connection
        md_listener.SetSockOption(SOL_SOCKET, SO_REUSEADDR, 1);
        md_listener.SetSockOption(
            SOL_SOCKET,
            SO_RCVBUF,
            receive_buffer_size);
        md_listener.Bind(9001);
        md_listener.Listen();

        oe_listener.SetSockOption(SOL_SOCKET, SO_REUSEADDR, 1);
        oe_listener.SetSockOption(
            SOL_SOCKET,
            SO_RCVBUF,
            receive_buffer_size);
        oe_listener.Bind(9002);
        oe_listener.Listen();

        utils::Socket md_client = md_listener.Accept();
        utils::Socket oe_client = oe_listener.Accept();
        oe_client.SetSockOption(IPPROTO_TCP, TCP_NODELAY, 1);

        std::size_t md_index = 0;
        std::size_t oe_index = 1;
        std::size_t wake_index = 2;


        wake_fd = ::eventfd(
            0,
            EFD_NONBLOCK | EFD_CLOEXEC);

        if (wake_fd == -1) {
            throw std::runtime_error("eventfd() failed");
        }

        std::array<pollfd, 3> poll_fds{{
            {
                .fd = md_client.NativeHandle(),
                .events = POLLIN,
                .revents = 0
            },
            {
                .fd = oe_client.NativeHandle(),
                .events = POLLIN,
                .revents = 0
            },
            {
                .fd = wake_fd,
                .events = POLLIN,
                .revents = 0
            }
        }};

        while (alive) {
            poll_fds[md_index].events = POLLIN;
            poll_fds[oe_index].events = POLLIN;
            poll_fds[wake_index].events = POLLIN;

            if (!send_queue.empty()) { poll_fds[oe_index].events |= POLLOUT; }

            poll_fds[md_index].revents = 0;
            poll_fds[oe_index].revents = 0;

            constexpr std::int32_t poll_timeout_ms = 1000;
            const int ready = ::poll(poll_fds.data(), static_cast<nfds_t>(poll_fds.size()), poll_timeout_ms);

            if (ready == -1) {
                if (errno == EINTR) {
                    continue;
                }

                throw std::system_error(
                    errno,
                    std::generic_category(),
                    "poll() failed");
            }
            if (ready == 0) {
                continue;
            }
            if (poll_fds[wake_index].revents & POLLIN) {
                resetWakeNotif();
            }

           //TODO drainEgress();

            if (poll_fds[md_index].revents & POLLIN) {
                recvMarketEvent(md_client, md_decoder);
            }
            if (poll_fds[oe_index].revents & POLLIN) {
                recvMarketEvent(oe_client, oe_decoder);
            }

          //  if (poll_fds[oe_index].revents & POLLOUT) {
           //     flushSendQueue();
           // }

            //TODO check heartbeat
        }
    }

    void NetworkManager::recvMarketEvent(
        utils::Socket& client,
        codec::ServerSideDecoder& decoder) {

        std::array<std::byte, 4096> recv_buffer;

        while (true) {
            const ::ssize_t recvd = client.Recv(recv_buffer);
            if (recvd > 0) {
                const std::span<const std::byte> recv_buffer_span({recv_buffer.data(), static_cast<std::size_t>(recvd)});

                std::vector<MarketEvent> recv_messages;
                auto result = decoder.Decode(recv_buffer_span, recv_messages);

                for (auto& recv_message : recv_messages) {
                    if (!ingress.try_push(std::move(recv_message))) {
                        throw std::runtime_error("failed to enqueue MarketEvent");
                    }

                }

                if (result.status == codec::DecodeStatus::error) {
                    throw std::runtime_error("invalid server inbound frame");
                }

                continue;
            }

            if (recvd == 0) {
                // MD client closed its connection cleanly.
                alive = false;
                return;
            }
            // received == -1
            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN ||
                errno == EWOULDBLOCK) {
                // We drained everything currently available.
                return;
                }

            throw std::system_error(
                errno,
                std::generic_category(),
                "market-data recv() failed");
        }
    }


    void NetworkManager::resetWakeNotif() {
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
