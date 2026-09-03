#ifndef SLIPSTREAM_MD_GRPC_CLIENT_TRANSPORT_H
#define SLIPSTREAM_MD_GRPC_CLIENT_TRANSPORT_H

#include "client_transport.h"
#include "slipstream.grpc.pb.h"

#include <grpcpp/grpcpp.h>

#include <cstdint>
#include <memory>
#include <string>

class MdGrpcClientTransport final : public IClientTransport {
public:
    MdGrpcClientTransport(const std::string& host, std::uint16_t port);
    ~MdGrpcClientTransport() override;

    utils::ConnectionResult Send(const MarketEvent& event) override;
    utils::ConnectionResult Finish() override;

private:
    grpc::ClientContext context_;
    google::protobuf::Empty response_;
    std::unique_ptr<slipstream::grpc_api::MarketData::Stub> stub_;
    std::unique_ptr<grpc::ClientWriter<slipstream::grpc_api::Quote>> writer_;
    bool finished_{false};
};

#endif
