//
// Created by babodev on 16.08.2026..
//

#include "NetworkManager.h"
#include "message_processor.h"
#include "replay_start.h"

#include <ios>
#include <memory>
#include <poll.h>
#include <string>
#include <system_error>
#include <netinet/in.h>
#include <netinet/tcp.h>

namespace slipstream {

    NetworkManager::NetworkManager(const SlipstreamConfig& config)
        : config_{config} {}

    NetworkManager::NetworkManager(
        const SlipstreamConfig& config,
        rigtorp::SPSCQueue<MarketEvent>& in,
        rigtorp::SPSCQueue<codec::OrderEntryClientMessage>& out)
        : config_{config},
          ingress(&in),
          egress(&out) {}

    NetworkManager::~NetworkManager() {}

    void NetworkManager::Process() {
        constexpr int receive_buffer_size = 1024 * 1024;
        constexpr int send_buffer_size = 1024 * 1024;

        ConsoleProcessMsg console_processor{"slipstream"};
        IProcessMsgClass* md_processor = &console_processor;
        IProcessMsgClass* oe_processor = &console_processor;

        std::unique_ptr<CanonicalFileProcessMsg> received_quotes;
        std::unique_ptr<CanonicalFileProcessMsg> received_trades;
        std::unique_ptr<FanoutProcessMsg> md_fanout;
        std::unique_ptr<FanoutProcessMsg> oe_fanout;

        if (utils::ReplayVerificationEnabled()) {
            const std::string received_quotes_path =
                std::string{SLIPSTREAM_VERIFICATION_DIR} +
                "/received_quotes.csv";
            const std::string received_trades_path =
                std::string{SLIPSTREAM_VERIFICATION_DIR} +
                "/received_trades.csv";

            received_quotes = std::make_unique<CanonicalFileProcessMsg>(
                received_quotes_path.c_str());
            received_trades = std::make_unique<CanonicalFileProcessMsg>(
                received_trades_path.c_str());
            md_fanout = std::make_unique<FanoutProcessMsg>(
                console_processor,
                *received_quotes);
            oe_fanout = std::make_unique<FanoutProcessMsg>(
                console_processor,
                *received_trades);
            md_processor = md_fanout.get();
            oe_processor = oe_fanout.get();
        }

        //wait for connection
        md_listener.SetReuseAddress();
        md_listener.SetReceiveBufferSize(receive_buffer_size);
        md_listener.Bind(config_.md_host.c_str(), config_.md_port);
        md_listener.Listen();

        oe_listener.SetReuseAddress();
        oe_listener.SetReceiveBufferSize(receive_buffer_size);
        oe_listener.Bind(config_.oe_host.c_str(), config_.oe_port);
        oe_listener.Listen();

        utils::Socket md_client = md_listener.Accept();
        utils::Socket oe_client = oe_listener.Accept();
        oe_client.SetTcpNoDelay();
        oe_client.SetSendBufferSize(send_buffer_size);

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
                recvMarketEvent(md_client, md_decoder, *md_processor);
            }
            if (poll_fds[oe_index].revents & POLLIN) {
                recvMarketEvent(oe_client, oe_decoder, *oe_processor);
            }

          //  if (poll_fds[oe_index].revents & POLLOUT) {
           //     flushSendQueue();
           // }

            //TODO check heartbeat
        }
    }

    void NetworkManager::recvMarketEvent(
        utils::Socket& client,
        codec::ServerSideDecoder& decoder,
        IProcessMsgClass& processor) {

        std::array<std::byte, 4096> recv_buffer;

        while (true) {
            const ::ssize_t recvd = client.Recv(recv_buffer);
            if (recvd > 0) {
                const std::span<const std::byte> recv_buffer_span({recv_buffer.data(), static_cast<std::size_t>(recvd)});

                std::vector<MarketEvent> recv_messages;
                auto result = decoder.Decode(recv_buffer_span, recv_messages);

                for (auto& recv_message : recv_messages) {
                    if (std::strcmp(recv_message.symbol, config_.symbol.c_str())) { continue; }

                    // if (!ingress.try_push(std::move(recv_message))) {
                    //     throw std::runtime_error(
                    //         "failed to enqueue MarketEvent");
                    // }

                    processor.Sink(recv_message);
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
