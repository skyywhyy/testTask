#pragma once

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>

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

template <typename Function>
void expect_invalid_argument(Function&& function, std::string_view test_name)
{
    try {
        std::forward<Function>(function)();
        report_failure(test_name, "expected std::invalid_argument");
    } catch (const std::invalid_argument&) {
    } catch (const std::exception& error) {
        report_failure(test_name, error.what());
    } catch (...) {
        report_failure(test_name, "unexpected non-standard exception");
    }
}

inline int finish()
{
    if (failure_count == 0) {
        std::cout << "All processing tests passed\n";
        return 0;
    }

    std::cerr << failure_count << " processing test(s) failed\n";
    return 1;
}

}  // namespace test_utils
