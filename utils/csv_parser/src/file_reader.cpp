#include "file_reader.h"

#include <algorithm>
#include <charconv>
#include <cerrno>
#include <cstring>
#ifdef _WIN32
#include <fstream>
#else
#include <stdexcept>
#include <system_error>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <stdexcept>
#include <string>
#include <system_error>

FileReader::FileReader(const char* path) {
#ifdef _WIN32
    std::ifstream file{path, std::ios::binary | std::ios::ate};
    if (!file) {
        throw std::system_error(errno, std::generic_category(), "open failed");
    }

    const std::ifstream::pos_type end = file.tellg();
    if (end < 0) {
        ThrowRuntimeError("CSV file size is invalid");
    }

    size_ = static_cast<std::size_t>(end);
    if (size_ == 0) {
        return;
    }

    storage_.resize(size_);
    file.seekg(0, std::ios::beg);
    if (!file.read(storage_.data(), static_cast<std::streamsize>(storage_.size()))) {
        ThrowRuntimeError("CSV file read failed");
    }
    mapping_ = storage_.data();
#else
    fd_ = ::open(path, O_RDONLY | O_CLOEXEC);
    if (fd_ == -1) {
        throw std::system_error(errno, std::generic_category(), "open failed");
    }

    struct stat file_info {};
    if (::fstat(fd_, &file_info) == -1) {
        ThrowSystemError("fstat failed");
    }

    if (!S_ISREG(file_info.st_mode)) {
        ThrowRuntimeError("CSV input is not a regular file");
    }

    if (file_info.st_size < 0) {
        ThrowRuntimeError("CSV file size is invalid");
    }

    size_ = static_cast<std::size_t>(file_info.st_size);
    if (size_ == 0) {
        return;
    }

    mapping_ = ::mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
    if (mapping_ == MAP_FAILED) {
        mapping_ = nullptr;
        ThrowSystemError("mmap failed");
    }

    // This is an optimization hint. Failure does not make the mapping unusable.
    static_cast<void>(::madvise(mapping_, size_, MADV_SEQUENTIAL));
#endif
}

FileReader::~FileReader() {
#ifndef _WIN32
    if (mapping_ != nullptr) {
        static_cast<void>(::munmap(mapping_, size_));
    }
    if (fd_ != -1) {
        static_cast<void>(::close(fd_));
    }
#endif
}

std::vector<MarketEvent> FileReader::Parse() const {
    const std::string_view data = RemoveHeader();
    std::vector<MarketEvent> events;

    const std::size_t newline_count =
        static_cast<std::size_t>(std::ranges::count(data, '\n'));
    events.reserve(newline_count +
                   (!data.empty() && data.back() != '\n' ? 1U : 0U));

    std::string_view remaining = data;
    std::size_t row_number = 1;

    while (!remaining.empty()) {
        ++row_number;
        const std::size_t newline = remaining.find('\n');
        std::string_view line = remaining.substr(0, newline);

        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        if (!line.empty()) {
            const auto fields = SplitRow(line, row_number);

            MarketEvent event{};
            event.ts = ParseTimestamp(fields[0], row_number);
            ParseSymbol(fields[2], event.symbol, row_number);

            if (fields[1] == "Q") {
                event.payload = Quote{
                    .bid_price = ParsePrice(fields[3], row_number),
                    .bid_qty = ParseQuantity(fields[4], row_number),
                    .ask_price = ParsePrice(fields[5], row_number),
                    .ask_qty = ParseQuantity(fields[6], row_number),
                };
            } else if (fields[1] == "T") {
                event.payload = Trade{
                    .price = ParsePrice(fields[7], row_number),
                    .qty = ParseQuantity(fields[8], row_number),
                };
            } else {
                ThrowRowError(row_number, "unknown market event type");
            }

            events.push_back(event);
        }

        if (newline == std::string_view::npos) {
            break;
        }
        remaining.remove_prefix(newline + 1);
    }

    return events;
}

void FileReader::ThrowSystemError(const char* message) {
    const int error = errno;
#ifndef _WIN32
    if (mapping_ != nullptr) {
        static_cast<void>(::munmap(mapping_, size_));
        mapping_ = nullptr;
    }
    if (fd_ != -1) {
        static_cast<void>(::close(fd_));
        fd_ = -1;
    }
#else
    mapping_ = nullptr;
#endif
    throw std::system_error(error, std::generic_category(), message);
}

void FileReader::ThrowRuntimeError(std::string_view message) {
#ifndef _WIN32
    if (mapping_ != nullptr) {
        static_cast<void>(::munmap(mapping_, size_));
        mapping_ = nullptr;
    }
    if (fd_ != -1) {
        static_cast<void>(::close(fd_));
        fd_ = -1;
    }
#else
    mapping_ = nullptr;
#endif
    throw std::runtime_error(std::string(message));
}

void FileReader::ThrowRowError(
    std::size_t row_number,
    std::string_view message) {
    throw std::runtime_error(
        "CSV row " + std::to_string(row_number) + ": " +
        std::string(message));
}

std::string_view FileReader::Contents() const noexcept {
    if (mapping_ == nullptr) {
        return {};
    }
    return {
        static_cast<const char*>(mapping_),
        size_,
    };
}

std::string_view FileReader::RemoveHeader() const {
    constexpr std::string_view expected_header =
        "Timestamp,Type,Symbol,BidPrice,BidQty,AskPrice,AskQty,Price,Qty";

    const std::string_view file = Contents();
    std::size_t position = 0;

    while (position < file.size()) {
        const std::size_t newline = file.find('\n', position);
        const std::size_t line_end =
            newline == std::string_view::npos ? file.size() : newline;

        std::string_view line = file.substr(position, line_end - position);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        if (line == expected_header) {
            return newline == std::string_view::npos
                ? std::string_view{}
                : file.substr(newline + 1);
        }

        if (newline == std::string_view::npos) {
            break;
        }
        position = newline + 1;
    }

    throw std::runtime_error("expected CSV header was not found");
}

std::array<std::string_view, FileReader::column_count> FileReader::SplitRow(
    std::string_view line,
    std::size_t row_number) {
    std::array<std::string_view, column_count> fields{};
    std::size_t start = 0;

    for (std::size_t index = 0; index < column_count; ++index) {
        const std::size_t comma = line.find(',', start);

        if (index + 1 == column_count) {
            if (comma != std::string_view::npos) {
                ThrowRowError(row_number, "too many columns");
            }
            fields[index] = line.substr(start);
            return fields;
        }

        if (comma == std::string_view::npos) {
            ThrowRowError(row_number, "too few columns");
        }

        fields[index] = line.substr(start, comma - start);
        start = comma + 1;
    }

    return fields;
}

std::uint64_t FileReader::ParseUnsigned(
    std::string_view text,
    std::size_t row_number) {
    std::uint64_t value{};
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value);

    if (text.empty() || error != std::errc{} ||
        end != text.data() + text.size()) {
        ThrowRowError(row_number, "invalid unsigned integer");
    }
    return value;
}

std::uint32_t FileReader::ParseQuantity(
    std::string_view text,
    std::size_t row_number) {
    std::uint32_t value{};
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value);

    if (text.empty() || error != std::errc{} ||
        end != text.data() + text.size()) {
        ThrowRowError(row_number, "invalid quantity");
    }
    return value;
}

std::int64_t FileReader::ParsePrice(
    std::string_view text,
    std::size_t row_number) {
    if (text.empty()) {
        ThrowRowError(row_number, "missing price");
    }

    const std::size_t dot = text.find('.');
    const std::string_view whole_text = text.substr(0, dot);
    const std::string_view fraction_text =
        dot == std::string_view::npos ? std::string_view{} : text.substr(dot + 1);

    if (whole_text.empty() || fraction_text.size() > 4) {
        ThrowRowError(row_number, "invalid fixed-point price");
    }

    const std::uint64_t whole = ParseUnsigned(whole_text, row_number);
    std::uint64_t fraction = 0;
    if (!fraction_text.empty()) {
        fraction = ParseUnsigned(fraction_text, row_number);
        for (std::size_t digits = fraction_text.size(); digits < 4; ++digits) {
            fraction *= 10;
        }
    }

    return static_cast<std::int64_t>(whole * 10'000 + fraction);
}

std::uint64_t FileReader::ParseTimestamp(
    std::string_view text,
    std::size_t row_number) {
    if (text.find(':') == std::string_view::npos) {
        return ParseUnsigned(text, row_number);
    }

    if (text.size() != 12 || text[2] != ':' || text[5] != ':' || text[8] != '.') {
        ThrowRowError(row_number, "invalid HH:MM:SS.mmm timestamp");
    }

    const auto digit = [text](std::size_t index) {
        return static_cast<std::uint64_t>(text[index] - '0');
    };

    const std::uint64_t hours = digit(0) * 10 + digit(1);
    const std::uint64_t minutes = digit(3) * 10 + digit(4);
    const std::uint64_t seconds = digit(6) * 10 + digit(7);
    const std::uint64_t milliseconds =
        digit(9) * 100 + digit(10) * 10 + digit(11);

    constexpr std::uint64_t nanoseconds_per_second = 1'000'000'000;
    constexpr std::uint64_t nanoseconds_per_millisecond = 1'000'000;
    const std::uint64_t seconds_since_midnight =
        hours * 3'600 + minutes * 60 + seconds;

    return seconds_since_midnight * nanoseconds_per_second +
           milliseconds * nanoseconds_per_millisecond;
}

void FileReader::ParseSymbol(
    std::string_view text,
    char (&symbol)[12],
    std::size_t row_number) {
    if (text.empty() || text.size() > std::size(symbol)) {
        ThrowRowError(row_number, "symbol length must be between 1 and 12");
    }

    std::fill(std::begin(symbol), std::end(symbol), '\0');
    std::memcpy(symbol, text.data(), text.size());
}
