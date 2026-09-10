#ifndef SLIPSTREAM_MESSAGE_PROCESSOR_H
#define SLIPSTREAM_MESSAGE_PROCESSOR_H

#include "market_event.h"

#include <fstream>

class IEventObserver {
public:
    virtual ~IEventObserver() = default;

    virtual void OnEvent(const MarketEvent& event) = 0;
};

class CanonicalFileEventObserver final : public IEventObserver {
public:
    explicit CanonicalFileEventObserver(const char* path);

    void OnEvent(const MarketEvent& event) override;

private:
    std::ofstream file_;
};

#endif
