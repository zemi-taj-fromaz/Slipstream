//
// Created by Adminstudio on 8/26/2026.
//

#ifndef SLIPSTREAM_SPSC_QUEUE_H
#define SLIPSTREAM_SPSC_QUEUE_H

#include <array>
#include <atomic>
#include <bit>
#include <concepts>
#include <cstddef>
#include <new>

namespace utils {

template<std::size_t N>
concept PowerOfTwo = std::has_single_bit(N);

// one empty slot ringbuffer design

template <typename T, std::size_t Capacity>
requires PowerOfTwo<Capacity> && std::default_initializable<T> && std::copyable<T>
class spsc_queue {
public:
  spsc_queue() = default;

  bool push(const T& element) {

    const auto writeIdx = this->writeIdx.load(std::memory_order_relaxed);

    const auto nextWriteIdx = (writeIdx + 1) & capacityMask;

    if (nextWriteIdx == cachedReadIdx) {
      cachedReadIdx = this->readIdx.load(std::memory_order_acquire);
      if (nextWriteIdx == cachedReadIdx) {
        return false;
      }
    }

    data[writeIdx] = element;
    this->writeIdx.store(nextWriteIdx, std::memory_order_release);
    return true;
  }

  bool pop(T& element) {
    const auto readIdx = this->readIdx.load(std::memory_order_relaxed);

    if (readIdx == cachedWriteIdx) {
      cachedWriteIdx = writeIdx.load(std::memory_order_acquire);
      if (readIdx == cachedWriteIdx) {
        return false;
      }
    }
    const auto nextReadIdx = (readIdx + 1) & capacityMask;
    element = data[readIdx];
    this->readIdx.store(nextReadIdx, std::memory_order_release);
    return true;
  }

private:
  std::array<T, Capacity> data{};
    static constexpr std::size_t capacityMask = Capacity - 1;

    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> writeIdx{0uz};
    alignas(std::hardware_destructive_interference_size) std::size_t cachedWriteIdx{0uz};
    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> readIdx{0uz};
    alignas(std::hardware_destructive_interference_size) std::size_t cachedReadIdx{0uz};
};
}

#endif // SLIPSTREAM_SPSC_QUEUE_H
