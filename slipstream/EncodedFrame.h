//
// Created by babodev on 16.08.2026..
//

#ifndef SLIPSTREAM_ENCODEDFRAME_H
#define SLIPSTREAM_ENCODEDFRAME_H

#include "slipstream_codec/market_data_codec.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace slipstream {

struct EncodedFrame {
    static constexpr std::size_t capacity = codec::max_order_frame_size;
    std::array<std::byte, capacity> bytes{};

    std::size_t size{};
    std::size_t sent{};
    std::uint64_t trigger_received_at_ns{};
    bool measure_tick_to_order{};

    [[nodiscard]] bool complete() const noexcept { return sent == size; }
    [[nodiscard]] std::span<const std::byte> remainingBytes() const noexcept {
        return {bytes.data() + sent, size - sent};
    }
    void advance(std::size_t byte_count) {
        if (byte_count > size - sent) {
            throw std::runtime_error("exceeded advance size");
        }
        sent += byte_count;
    }
};

} // namespace slipstream

#endif //SLIPSTREAM_ENCODEDFRAME_H
