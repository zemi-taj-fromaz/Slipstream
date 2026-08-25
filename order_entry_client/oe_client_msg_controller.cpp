#include "oe_client_msg_controller.h"

#include <array>
#include <cerrno>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

namespace {

template <typename... Visitors>
struct Overloaded : Visitors... {
    using Visitors::operator()...;
};

} // namespace

OEClientMsgController::OEClientMsgController(
    const char* host,
    std::uint16_t port,
    spdlog::logger& logger)
    : logger_{logger} {
    constexpr int send_buffer_size = 1024 * 1024;
    socket_.SetTcpNoDelay();
    socket_.SetSendBufferSize(send_buffer_size);
    socket_.Connect(host, port);
    poll_descriptor_.fd = socket_.NativeHandle();
}

utils::ConnectionResult OEClientMsgController::Send(const MarketEvent& event) {
    std::array<std::byte, slipstream::codec::max_market_data_frame_size> buffer{};
    const std::size_t encoded_size =
        slipstream::codec::EncodeMarketEvent(event, buffer);

    return socket_.SendAll({buffer.data(), encoded_size});
}

utils::ConnectionResult OEClientMsgController::ProcessInboundUntil(
    std::chrono::steady_clock::time_point deadline) {
    while (true) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            return utils::ConnectionResult::Complete;
        }

        const auto timeout_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - now)
                .count();
        poll_descriptor_.events = POLLIN;
        poll_descriptor_.revents = 0;

        const int ready = ::poll(
            &poll_descriptor_,
            1,
            static_cast<int>(timeout_ms));

        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }

            throw std::system_error(
                errno,
                std::generic_category(),
                "OE poll() failed");
        }

        if (ready == 0) {
            return utils::ConnectionResult::Complete;
        }

        if (poll_descriptor_.revents & POLLIN) {
            const utils::ConnectionResult result = ReceiveAvailable();
            if (result == utils::ConnectionResult::PeerDisconnected) {
                return result;
            }
        }

        if (poll_descriptor_.revents & POLLHUP) {
            return utils::ConnectionResult::PeerDisconnected;
        }

        if (poll_descriptor_.revents & (POLLERR | POLLNVAL)) {
            throw std::runtime_error(
                "OE socket error while waiting for server response");
        }
    }
}

std::string OEClientMsgController::Symbol(const char (&symbol)[12]) {
    std::size_t length = 0;
    while (length < sizeof(symbol) && symbol[length] != '\0') {
        ++length;
    }
    return {symbol, length};
}

utils::ConnectionResult OEClientMsgController::ReceiveAvailable() {
    std::array<std::byte, 4096> buffer{};

    while (true) {
        const ::ssize_t recvd = socket_.Recv(buffer);
        if (recvd > 0) {
            std::vector<slipstream::codec::OrderEntryClientMessage> messages;
            const auto result = decoder_.Decode(
                {buffer.data(), static_cast<std::size_t>(recvd)},
                messages);

            if (result.status == slipstream::codec::DecodeStatus::error) {
                throw std::runtime_error(
                    "failed to decode OE server response");
            }

            for (const auto& message : messages) {
                HandleMessage(message);
            }

            continue;
        }

        if (recvd == 0) {
            return utils::ConnectionResult::PeerDisconnected;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return utils::ConnectionResult::Complete;
        }

        if (errno == ECONNRESET || errno == ENOTCONN) {
            return utils::ConnectionResult::PeerDisconnected;
        }

        throw std::system_error(
            errno,
            std::generic_category(),
            "OE recv() failed");
    }
}

void OEClientMsgController::HandleMessage(
    const slipstream::codec::OrderEntryClientMessage& message) {
    std::visit(
        Overloaded{
            [this](const slipstream::codec::NewOrderMessage& value) {
                logger_.info(
                    "NewOrder {} trade_id={} symbol={} side={} qty={} limit_px={} client_order_id={}",
                    value.status == slipstream::codec::NewOrderStatus::accepted
                        ? "ACCEPTED"
                        : "REJECTED",
                    value.trade_id,
                    Symbol(value.symbol),
                    static_cast<char>(value.side),
                    value.qty,
                    value.limit_px,
                    value.client_order_id);
            },
            [this](const slipstream::codec::ExecReportMessage& value) {
                logger_.info(
                    "ExecReport client_order_id={} status={} filled_qty={} avg_px={} reason_code={} ts_ns={}",
                    value.client_order_id,
                    static_cast<unsigned>(value.status),
                    value.filled_qty,
                    value.avg_px,
                    static_cast<unsigned>(value.reason_code),
                    value.ts_ns);
            },
            [this](const slipstream::codec::HeartbeatMessage& value) {
                logger_.info("Heartbeat ts_ns={}", value.ts_ns);
            },
            [this](const slipstream::codec::SessionControlMessage& value) {
                logger_.info(
                    "SessionControl state={} ts_ns={}",
                    static_cast<unsigned>(value.state),
                    value.ts_ns);
            },
        },
        message);
}
