//
// Created by babodev on 16.08.2026..
//

#include "NetworkManager.h"
#include <poll.h>
#include <system_error>

namespace slipstream {

    NetworkManager::NetworkManager() {}
    NetworkManager::~NetworkManager() {}

    void NetworkManager::Process() {
        //wait for connection
        md_listener.SetSockOption();
        md_listener.Bind();
        md_listener.Listen();

        oe_listener.SetSockOption();
        oe_listener.Bind();
        oe_listener.Listen();

        utils::Socket md_client = md_listener.Accept();
        utils::Socket oe_client = oe_listener.Accept();

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

            if () {

            }

            //poll events
            //drain outgress queue
            // check heart
            //check pollout for sending
            // set or reset pollout flag
        }
    }


}
