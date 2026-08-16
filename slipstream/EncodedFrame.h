//
// Created by babodev on 16.08.2026..
//

#ifndef SLIPSTREAM_ENCODEDFRAME_H
#define SLIPSTREAM_ENCODEDFRAME_H

#include "NetworkManager.h"
#include "../codec/include/slipstream_codec/market_data_codec.h"
#include "slipstream_codec/market_data_codec.hpp"

struct EncodedFrame {
    static constexpr std::size_t capacity = slipstream::codec::max_order_frame_size;
    std::array<std::byte, capacity> data{};

    std::size_t size{};
    std::size_t sent{};

    bool complete() const noexcept { return sent == size; }
    std::span<const std::byte> data() const noexcept { return std::span<const std::byte>{data.data() + sent, size - sent}; }
    void advance(std::size_t bytes) noexcept {
        if (bytes > size - sent) throw std::runtime_error("exceeded advance size");
        sent += bytes;
    }
};

#endif //SLIPSTREAM_ENCODEDFRAME_H
