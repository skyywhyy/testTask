#pragma once

#include <iostream>
#include <string_view>

namespace test_utils {

inline int failure_count = 0;

inline void report_failure(std::string_view test_name, std::string_view message)
{
    std::cerr << "[FAIL] " << test_name << ": " << message << '\n';
    ++failure_count;
}

template <typename Actual, typename Expected>
void expect_equal(
    const Actual& actual,
    const Expected& expected,
    std::string_view test_name)
{
    if (actual != expected) {
        std::cerr << "[FAIL] " << test_name << ": expected " << expected
                  << ", got " << actual << '\n';
        ++failure_count;
    }
}

inline void expect_true(bool condition, std::string_view test_name)
{
    if (!condition) {
        report_failure(test_name, "expected true");
    }
}

inline void expect_false(bool condition, std::string_view test_name)
{
    if (condition) {
        report_failure(test_name, "expected false");
    }
}

inline int finish(std::string_view suite_name)
{
    if (failure_count == 0) {
        std::cout << "All " << suite_name << " tests passed\n";
        return 0;
    }

    std::cerr << failure_count << ' ' << suite_name << " test(s) failed\n";
    return 1;
}

}  // namespace test_utils
