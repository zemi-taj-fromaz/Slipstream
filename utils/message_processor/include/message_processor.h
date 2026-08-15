#ifndef SLIPSTREAM_MESSAGE_PROCESSOR_H
#define SLIPSTREAM_MESSAGE_PROCESSOR_H

#include "market_event.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <vector>

class IProcessMsgClass {
public:
    virtual ~IProcessMsgClass() = default;

    virtual void Sink(const MarketEvent& event);
};

class FileProcessMsg final : public IProcessMsgClass {
public:
    explicit FileProcessMsg(const char* path);

    void Sink(const MarketEvent& event) override;

private:
    std::ofstream file_;
    std::chrono::steady_clock::time_point last_sink_{};
    std::uint64_t last_event_timestamp_{0};
    bool has_last_event_{false};
};

class ConsoleProcessMsg final : public IProcessMsgClass {
public:
    void Sink(const MarketEvent& event) override;
};

class FanoutProcessMsg final : public IProcessMsgClass {
public:
    FanoutProcessMsg(
        IProcessMsgClass& first,
        IProcessMsgClass& second);

    void Sink(const MarketEvent& event) override;

private:
    IProcessMsgClass& first_;
    IProcessMsgClass& second_;
};

void ProcessRowsByTimestamp(
    const std::vector<MarketEvent>& rows,
    IProcessMsgClass& processor);

#endif // SLIPSTREAM_MESSAGE_PROCESSOR_H
