#ifndef SLIPSTREAM_PARSER_H
#define SLIPSTREAM_PARSER_H

#include "market_event.h"

#include <vector>

[[nodiscard]] std::vector<MarketEvent> parse_csv(const char* path);

#endif // SLIPSTREAM_PARSER_H
