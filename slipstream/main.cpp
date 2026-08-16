//
// Created by babodev on 15.08.2026..
//
#include <exception>
#include <memory>
#include <vector>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "NetworkManager.h"

int main() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "slipstream_server.log",
        true);
    const std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    spdlog::logger logger{"slip_stream_server", sinks.begin(), sinks.end()};
  //  const auto events = parse_csv(csv_path);
    slipstream::NetworkManager network_manager;
    network_manager.Process();


    return 0;
}
