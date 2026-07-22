#include <program2/message_processor.hpp>

#include "test_utils.hpp"

#include <sstream>
#include <string>
#include <string_view>

namespace {

void expect_prefix(
    const std::string& value,
    std::string_view prefix,
    std::string_view test_name)
{
    test_utils::expect_true(
        value.compare(0, prefix.size(), prefix) == 0, test_name);
}

void test_accepts_valid_sum()
{
    std::ostringstream output;
    program2::MessageProcessor processor{output};

    processor.process("128");

    expect_prefix(
        output.str(), "Received valid sum", "128 is a valid message");
}

void test_rejects_sum_that_does_not_satisfy_condition()
{
    std::ostringstream output;
    program2::MessageProcessor processor{output};

    processor.process("96");

    expect_prefix(
        output.str(),
        "Error: sum does not satisfy the condition",
        "96 does not satisfy the sum condition");
}

void test_rejects_invalid_messages()
{
    const std::string invalid_message_prefix{"Error: invalid message"};

    std::ostringstream empty_output;
    program2::MessageProcessor empty_processor{empty_output};
    empty_processor.process("");
    expect_prefix(
        empty_output.str(), invalid_message_prefix, "empty message is invalid");

    std::ostringstream non_digit_output;
    program2::MessageProcessor non_digit_processor{non_digit_output};
    non_digit_processor.process("12a");
    expect_prefix(
        non_digit_output.str(),
        invalid_message_prefix,
        "non-digit message is invalid");

    std::ostringstream long_output;
    program2::MessageProcessor long_processor{long_output};
    long_processor.process(std::string(33, '1'));
    expect_prefix(
        long_output.str(),
        invalid_message_prefix,
        "33-digit message is invalid");
}

}  // namespace

int main()
{
    test_accepts_valid_sum();
    test_rejects_sum_that_does_not_satisfy_condition();
    test_rejects_invalid_messages();

    return test_utils::finish();
}
