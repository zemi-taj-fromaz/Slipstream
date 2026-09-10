#include "file_reader.h"
#include "parser.h"

std::vector<MarketEvent> parse_csv(const char* path) {
    const FileReader reader{path};
    return reader.Parse();
}
