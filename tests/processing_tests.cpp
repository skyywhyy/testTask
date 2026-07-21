#include <processing/processing.hpp>

#include "test_utils.hpp"

#include <string>

namespace {

std::string repeated_kb(std::size_t count)
{
    std::string result;
    result.reserve(count * 2);

    for (std::size_t index = 0; index < count; ++index) {
        result += "KB";
    }

    return result;
}

void test_transform()
{
    test_utils::expect_invalid_argument(
        [] {
            std::string value;
            processing::transform(value);
        },
        "transform rejects empty string");

    std::string odd_digit{"7"};
    processing::transform(odd_digit);
    test_utils::expect_equal(
        odd_digit, std::string{"7"}, "transform keeps one odd digit");

    std::string even_digit{"8"};
    processing::transform(even_digit);
    test_utils::expect_equal(
        even_digit, std::string{"KB"}, "transform replaces one even digit");

    std::string even_digits{"2468"};
    processing::transform(even_digits);
    test_utils::expect_equal(
        even_digits,
        std::string{"KBKBKBKB"},
        "transform replaces all even digits");

    std::string odd_digits{"13579"};
    processing::transform(odd_digits);
    test_utils::expect_equal(
        odd_digits, std::string{"97531"}, "transform sorts odd digits");

    std::string repeated_digits{"112233"};
    processing::transform(repeated_digits);
    test_utils::expect_equal(
        repeated_digits,
        std::string{"33KBKB11"},
        "transform handles repeated digits");

    std::string maximum_length(64, '8');
    processing::transform(maximum_length);
    test_utils::expect_equal(
        maximum_length,
        repeated_kb(64),
        "transform accepts 64 digits");

    test_utils::expect_invalid_argument(
        [] {
            std::string value(65, '1');
            processing::transform(value);
        },
        "transform rejects 65 digits");

    test_utils::expect_invalid_argument(
        [] {
            std::string value{"12a3"};
            processing::transform(value);
        },
        "transform rejects non-digit characters");
}

void test_calculate_sum()
{
    test_utils::expect_equal(
        processing::calculate_sum(""), 0, "calculate_sum handles empty input");
    test_utils::expect_equal(
        processing::calculate_sum("KB5KB3KB"),
        8,
        "calculate_sum handles transformed input");
    test_utils::expect_equal(
        processing::calculate_sum("a1KB9"),
        10,
        "calculate_sum ignores non-digit characters");
}

void test_is_valid_sum()
{
    test_utils::expect_false(
        processing::is_valid_sum(""), "is_valid_sum rejects empty input");
    test_utils::expect_false(
        processing::is_valid_sum("7"), "is_valid_sum rejects one digit");
    test_utils::expect_false(
        processing::is_valid_sum("32"), "is_valid_sum rejects two-digit 32");
    test_utils::expect_false(
        processing::is_valid_sum("96"), "is_valid_sum rejects two-digit 96");
    test_utils::expect_true(
        processing::is_valid_sum("128"), "is_valid_sum accepts 128");
    test_utils::expect_true(
        processing::is_valid_sum("160"), "is_valid_sum accepts 160");
    test_utils::expect_false(
        processing::is_valid_sum("129"), "is_valid_sum rejects non-multiple");
    test_utils::expect_false(
        processing::is_valid_sum("12a"), "is_valid_sum rejects non-digits");
    test_utils::expect_false(
        processing::is_valid_sum("999999999999999999999999999999"),
        "is_valid_sum rejects overflow");
}

}  // namespace

int main()
{
    test_transform();
    test_calculate_sum();
    test_is_valid_sum();

    return test_utils::finish();
}
