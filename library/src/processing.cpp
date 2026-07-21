#include <processing/processing.hpp>

#include <algorithm>
#include <charconv>
#include <functional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace {

constexpr bool is_ascii_digit(char symbol) noexcept
{
    return symbol >= '0' && symbol <= '9';
}

}  // namespace

namespace processing {

void transform(std::string& value)
{
    if (value.empty()) {
        throw std::invalid_argument{"Input must not be empty"};
    }

    if (value.size() > 64) {
        throw std::invalid_argument{"Input must contain at most 64 digits"};
    }

    if (!std::all_of(value.begin(), value.end(), is_ascii_digit)) {
        throw std::invalid_argument{"Input must contain only digits"};
    }

    std::sort(value.begin(), value.end(), std::greater<char>{});

    std::string result;
    result.reserve(value.size() * 2);

    for (char symbol : value) {
        const int digit = symbol - '0';

        if (digit % 2 == 0) {
            result += "KB";
        } else {
            result += symbol;
        }
    }

    value = std::move(result);
}

int calculate_sum(std::string_view value) noexcept
{
    int sum = 0;

    for (char symbol : value) {
        if (is_ascii_digit(symbol)) {
            sum += symbol - '0';
        }
    }

    return sum;
}

bool is_valid_sum(std::string_view value) noexcept
{
    if (value.size() <= 2) {
        return false;
    }

    if (!std::all_of(value.begin(), value.end(), is_ascii_digit)) {
        return false;
    }

    int parsed_value = 0;
    const char* const begin = value.data();
    const char* const end = begin + value.size();
    const auto [position, error] = std::from_chars(begin, end, parsed_value);

    return error == std::errc{} && position == end && parsed_value % 32 == 0;
}

}  // namespace processing
