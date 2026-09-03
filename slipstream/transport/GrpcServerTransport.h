#ifndef SLIPSTREAM_GRPCSERVERTRANSPORT_H
#define SLIPSTREAM_GRPCSERVERTRANSPORT_H

#include "IServerTransport.h"

namespace slipstream {

class GrpcServerTransport final : public IServerTransport {
public:
    void Run() override;
    void Stop() override;
    void NotifyOutboundReady() override;
};

}

#endif
