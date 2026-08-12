#include "parser.h"

#include <exception>
#include <memory>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

int main() {
    constexpr auto csv_path = "Quotes_and_Trades.csv";

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "market_data_client.log",
        true);
    const std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    spdlog::logger logger{"market_data_client", sinks.begin(), sinks.end()};

    try {
        const auto events = parse_csv(csv_path);
        logger.info("Parsed {} market event rows from {}", events.size(), csv_path);
    } catch (const std::exception& error) {
        logger.error("Failed to parse {}: {}", csv_path, error.what());
        return 1;
    }

    return 0;
}
