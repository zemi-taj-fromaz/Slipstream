#ifndef SLIPSTREAM_CSV_PARSER_FILE_READER_H
#define SLIPSTREAM_CSV_PARSER_FILE_READER_H

#include "market_event.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

class FileReader {
public:
    explicit FileReader(const char* path);
    ~FileReader();

    FileReader(const FileReader&) = delete;
    FileReader& operator=(const FileReader&) = delete;
    FileReader(FileReader&&) = delete;
    FileReader& operator=(FileReader&&) = delete;

    [[nodiscard]] std::vector<MarketEvent> Parse() const;

private:
    static constexpr std::size_t column_count = 9;

    int fd_{-1};
    void* mapping_{nullptr};
    std::size_t size_{0};

    [[noreturn]] void ThrowSystemError(const char* message);
    [[noreturn]] void ThrowRuntimeError(std::string_view message);
    [[noreturn]] static void ThrowRowError(
        std::size_t row_number,
        std::string_view message);

    [[nodiscard]] std::string_view Contents() const noexcept;
    [[nodiscard]] std::string_view RemoveHeader() const;

    [[nodiscard]] static std::array<std::string_view, column_count> SplitRow(
        std::string_view line,
        std::size_t row_number);

    [[nodiscard]] static std::uint64_t ParseUnsigned(
        std::string_view text,
        std::size_t row_number);

    [[nodiscard]] static std::uint32_t ParseQuantity(
        std::string_view text,
        std::size_t row_number);

    [[nodiscard]] static std::int64_t ParsePrice(
        std::string_view text,
        std::size_t row_number);

    [[nodiscard]] static std::uint64_t ParseTimestamp(
        std::string_view text,
        std::size_t row_number);

    static void ParseSymbol(
        std::string_view text,
        char (&symbol)[12],
        std::size_t row_number);
};

#endif // SLIPSTREAM_CSV_PARSER_FILE_READER_H
