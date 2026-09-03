#include "MdGrpcClientTransport.h"

#include <chrono>
#include <stdexcept>
#include <variant>

namespace {

std::string Address(const std::string& host, const std::uint16_t port) {
    return host + ':' + std::to_string(port);
}

}

MdGrpcClientTransport::MdGrpcClientTransport(
    const std::string& host,
    const std::uint16_t port) {
    auto channel = grpc::CreateChannel(
        Address(host, port),
        grpc::InsecureChannelCredentials());
    if (!channel->WaitForConnected(
            std::chrono::system_clock::now() + std::chrono::seconds{5})) {
        throw std::runtime_error("failed to connect MD gRPC channel");
    }

    stub_ = slipstream::grpc_api::MarketData::NewStub(channel);
    writer_ = stub_->Connect(&context_, &response_);
    if (!writer_) {
        throw std::runtime_error("failed to start MD gRPC stream");
    }
}

MdGrpcClientTransport::~MdGrpcClientTransport() {
    if (!finished_) {
        context_.TryCancel();
    }
}

utils::ConnectionResult MdGrpcClientTransport::Send(
    const MarketEvent& event) {
    if (finished_) {
        return utils::ConnectionResult::PeerDisconnected;
    }

    const Quote* quote = std::get_if<Quote>(&event.payload);
    if (quote == nullptr) {
        throw std::invalid_argument("MD gRPC transport requires a quote");
    }

    slipstream::grpc_api::Quote message;
    message.set_ts_ns(event.ts);
    message.set_symbol(event.symbol);
    message.set_bid_price(quote->bid_price);
    message.set_bid_qty(quote->bid_qty);
    message.set_ask_price(quote->ask_price);
    message.set_ask_qty(quote->ask_qty);

    return writer_->Write(message)
        ? utils::ConnectionResult::Complete
        : utils::ConnectionResult::PeerDisconnected;
}

utils::ConnectionResult MdGrpcClientTransport::Finish() {
    if (finished_) {
        return utils::ConnectionResult::Complete;
    }

    finished_ = true;
    writer_->WritesDone();
    const grpc::Status status = writer_->Finish();
    return status.ok()
        ? utils::ConnectionResult::Complete
        : utils::ConnectionResult::PeerDisconnected;
}
