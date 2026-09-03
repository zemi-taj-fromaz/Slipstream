#ifndef SLIPSTREAM_MESSAGE_PROCESSOR_H
#define SLIPSTREAM_MESSAGE_PROCESSOR_H

#include "market_event.h"

#include <fstream>
#include <string>

class IMsgController {
public:
    virtual ~IMsgController() = default;

    virtual void Sink(const MarketEvent& event);
};

class CanonicalFileMsgController final : public IMsgController {
public:
    explicit CanonicalFileMsgController(const char* path);

    void Sink(const MarketEvent& event) override;

private:
    std::ofstream file_;
};

class ConsoleMsgController final : public IMsgController {
public:
    explicit ConsoleMsgController(std::string process_name = {});

    void Sink(const MarketEvent& event) override;

private:
    std::string process_name_;
};

class FanoutMsgController final : public IMsgController {
public:
    FanoutMsgController(
        IMsgController& first,
        IMsgController& second);

    void Sink(const MarketEvent& event) override;

private:
    IMsgController& first_;
    IMsgController& second_;
};

#endif
