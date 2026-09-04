#include "OeLatencyRecorder.h"

#include <chrono>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace {

constexpr std::size_t expected_order_count = 5'000;

}

OeLatencyRecorder::OeLatencyRecorder(
    std::string transport,
    std::string output_path)
    : transport_{std::move(transport)},
      output_path_{std::move(output_path)} {
    pending_sends_.reserve(expected_order_count);
    samples_.reserve(expected_order_count);
}

void OeLatencyRecorder::RecordSend(const std::int64_t trade_id) {
    auto [entry, inserted] = pending_sends_.try_emplace(trade_id, 0);
    if (!inserted) {
        throw std::runtime_error("duplicate pending OE trade id");
    }

    entry->second = NowNs();
}

void OeLatencyRecorder::RecordConfirmation(
    const std::int64_t trade_id,
    const char status) {
    const std::uint64_t receive_ns = NowNs();
    const auto entry = pending_sends_.find(trade_id);
    if (entry == pending_sends_.end()) {
        return;
    }

    const std::uint64_t send_ns = entry->second;
    samples_.push_back({
        .trade_id = trade_id,
        .status = status,
        .send_ns = send_ns,
        .receive_ns = receive_ns,
        .latency_ns = receive_ns - send_ns,
    });
    pending_sends_.erase(entry);
}

void OeLatencyRecorder::WriteCsv() const {
    std::ofstream output{output_path_, std::ios::trunc};
    if (!output) {
        throw std::runtime_error("failed to open OE latency CSV");
    }

    output << "transport,trade_id,status,send_ns,receive_ns,latency_ns\n";
    for (const Sample& sample : samples_) {
        output
            << transport_ << ','
            << sample.trade_id << ','
            << sample.status << ','
            << sample.send_ns << ','
            << sample.receive_ns << ','
            << sample.latency_ns << '\n';
    }

    if (!output) {
        throw std::runtime_error("failed to write OE latency CSV");
    }
}

std::size_t OeLatencyRecorder::SampleCount() const noexcept {
    return samples_.size();
}

const std::string& OeLatencyRecorder::OutputPath() const noexcept {
    return output_path_;
}

std::uint64_t OeLatencyRecorder::NowNs() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}
