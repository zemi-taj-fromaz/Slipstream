#ifndef SLIPSTREAM_ISERVERTRANSPORT_H
#define SLIPSTREAM_ISERVERTRANSPORT_H

namespace slipstream {

class IServerTransport {
public:
    virtual ~IServerTransport() = default;

    virtual void Run() = 0;
    virtual void Stop() = 0;
    virtual void NotifyOutboundReady() = 0;
};

}

#endif
