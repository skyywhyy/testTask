#include <program1/shared_buffer.hpp>

#include "test_utils.hpp"

#include <chrono>
#include <future>
#include <optional>
#include <string>

namespace {

using namespace std::chrono_literals;

void test_round_trip()
{
    program1::SharedBuffer buffer;

    test_utils::expect_true(buffer.put("first"), "put accepts a value");

    const std::optional<std::string> value = buffer.take();
    test_utils::expect_true(value.has_value(), "take returns a value");

    if (value.has_value()) {
        test_utils::expect_equal(
            *value, std::string{"first"}, "take returns the stored value");
    }
}

void test_take_waits_for_data()
{
    program1::SharedBuffer buffer;
    auto result = std::async(std::launch::async, [&buffer] { return buffer.take(); });

    test_utils::expect_true(
        result.wait_for(100ms) == std::future_status::timeout,
        "take waits while the buffer is empty");

    buffer.put("value");

    const bool ready = result.wait_for(2s) == std::future_status::ready;
    test_utils::expect_true(ready, "take wakes after put");

    if (!ready) {
        buffer.stop();
        return;
    }

    const std::optional<std::string> value = result.get();
    test_utils::expect_true(value.has_value(), "waiting take receives data");

    if (value.has_value()) {
        test_utils::expect_equal(
            *value, std::string{"value"}, "waiting take receives correct data");
    }
}

void test_put_waits_for_space_and_preserves_order()
{
    program1::SharedBuffer buffer;
    buffer.put("first");

    auto second_put = std::async(
        std::launch::async, [&buffer] { return buffer.put("second"); });

    test_utils::expect_true(
        second_put.wait_for(100ms) == std::future_status::timeout,
        "put waits while the buffer is full");

    const std::optional<std::string> first = buffer.take();
    test_utils::expect_true(first.has_value(), "take returns first queued value");

    if (first.has_value()) {
        test_utils::expect_equal(
            *first, std::string{"first"}, "buffer preserves first value");
    }

    const bool ready = second_put.wait_for(2s) == std::future_status::ready;
    test_utils::expect_true(ready, "put wakes after take");

    if (!ready) {
        buffer.stop();
        return;
    }

    test_utils::expect_true(second_put.get(), "second put succeeds");

    const std::optional<std::string> second = buffer.take();
    test_utils::expect_true(second.has_value(), "take returns second queued value");

    if (second.has_value()) {
        test_utils::expect_equal(
            *second, std::string{"second"}, "buffer preserves second value");
    }
}

void test_stop_wakes_waiting_take()
{
    program1::SharedBuffer buffer;
    auto result = std::async(std::launch::async, [&buffer] { return buffer.take(); });

    test_utils::expect_true(
        result.wait_for(100ms) == std::future_status::timeout,
        "take waits before stop");

    buffer.stop();

    const bool ready = result.wait_for(2s) == std::future_status::ready;
    test_utils::expect_true(ready, "stop wakes waiting take");

    if (ready) {
        test_utils::expect_false(
            result.get().has_value(), "take returns nullopt after stop");
    }
}

void test_stop_wakes_waiting_put_and_keeps_buffered_value()
{
    program1::SharedBuffer buffer;
    buffer.put("first");

    auto second_put = std::async(
        std::launch::async, [&buffer] { return buffer.put("second"); });

    test_utils::expect_true(
        second_put.wait_for(100ms) == std::future_status::timeout,
        "second put waits before stop");

    buffer.stop();

    const bool ready = second_put.wait_for(2s) == std::future_status::ready;
    test_utils::expect_true(ready, "stop wakes waiting put");

    if (ready) {
        test_utils::expect_false(second_put.get(), "put fails after stop");
    }

    const std::optional<std::string> first = buffer.take();
    test_utils::expect_true(
        first.has_value(), "take drains a value buffered before stop");

    if (first.has_value()) {
        test_utils::expect_equal(
            *first, std::string{"first"}, "stop does not lose buffered data");
    }

    test_utils::expect_false(
        buffer.take().has_value(), "take returns nullopt after stopped buffer drains");
    test_utils::expect_false(buffer.put("third"), "put stays disabled after stop");
}

}  // namespace

int main()
{
    test_round_trip();
    test_take_waits_for_data();
    test_put_waits_for_space_and_preserves_order();
    test_stop_wakes_waiting_take();
    test_stop_wakes_waiting_put_and_keeps_buffered_value();

    return test_utils::finish();
}
