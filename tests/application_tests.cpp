#include <program1/application.hpp>

#include "test_utils.hpp"

#include <stdexcept>
#include <sstream>
#include <string>

namespace {

class FailingResultBuffer : public std::stringbuf {
protected:
    std::streamsize xsputn(const char* text, std::streamsize count) override
    {
        const std::string value{text, static_cast<std::size_t>(count)};

        if (value.find("Processed:") != std::string::npos) {
            throw std::runtime_error{"output failed"};
        }

        return std::stringbuf::xsputn(text, count);
    }
};

void test_processes_input_and_exits()
{
    std::istringstream input{"123456\nexit\n"};
    std::ostringstream output;
    std::ostringstream error_output;
    program1::Application application{input, output, error_output};

    test_utils::expect_equal(
        application.run(), 0, "application exits successfully");
    test_utils::expect_true(
        output.str().find("Processed: KB5KB3KB1") != std::string::npos,
        "application prints transformed value");
    test_utils::expect_true(
        output.str().find("Sum: 9") != std::string::npos,
        "application prints calculated sum");
    test_utils::expect_true(
        error_output.str().empty(), "valid input does not produce errors");
}

void test_rejects_invalid_input_and_continues()
{
    std::istringstream input{"12a3\n13579\nexit\n"};
    std::ostringstream output;
    std::ostringstream error_output;
    program1::Application application{input, output, error_output};

    test_utils::expect_equal(
        application.run(), 0, "application continues after invalid input");
    test_utils::expect_true(
        error_output.str().find("Invalid input") != std::string::npos,
        "application reports invalid input");
    test_utils::expect_true(
        output.str().find("Processed: 97531") != std::string::npos,
        "application processes the next valid input");
    test_utils::expect_true(
        output.str().find("Sum: 25") != std::string::npos,
        "application calculates the next valid sum");
}

void test_stops_on_end_of_input()
{
    std::istringstream input;
    std::ostringstream output;
    std::ostringstream error_output;
    program1::Application application{input, output, error_output};

    test_utils::expect_equal(
        application.run(), 0, "application handles end of input");
}

void test_propagates_worker_error_without_terminating()
{
    std::istringstream input{"123456\n"};
    FailingResultBuffer output_buffer;
    std::ostream output{&output_buffer};
    output.exceptions(std::ios::badbit);
    std::ostringstream error_output;
    program1::Application application{input, output, error_output};

    bool exception_received = false;

    try {
        application.run();
    } catch (const std::exception&) {
        exception_received = true;
    }

    test_utils::expect_true(
        exception_received, "worker output error reaches the main thread");
}

}  // namespace

int main()
{
    test_processes_input_and_exits();
    test_rejects_invalid_input_and_continues();
    test_stops_on_end_of_input();
    test_propagates_worker_error_without_terminating();

    return test_utils::finish();
}
