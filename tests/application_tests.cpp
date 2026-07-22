#include <program1/application.hpp>

#include "test_utils.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
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

class LoopbackListener {
public:
    LoopbackListener()
        : descriptor_{::socket(AF_INET, SOCK_STREAM, 0)}
    {
        if (descriptor_ == -1) {
            throw std::runtime_error{"failed to create listener socket"};
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(0);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (::bind(
                descriptor_,
                reinterpret_cast<const sockaddr*>(&address),
                sizeof(address)) == -1
            || ::listen(descriptor_, 1) == -1) {
            ::close(descriptor_);
            throw std::runtime_error{"failed to start listener"};
        }

        socklen_t address_size = sizeof(address);
        if (::getsockname(
                descriptor_, reinterpret_cast<sockaddr*>(&address), &address_size)
            == -1) {
            ::close(descriptor_);
            throw std::runtime_error{"failed to query listener port"};
        }

        port_ = ntohs(address.sin_port);
    }

    ~LoopbackListener()
    {
        if (descriptor_ != -1) {
            ::close(descriptor_);
        }
    }

    LoopbackListener(const LoopbackListener&) = delete;
    LoopbackListener& operator=(const LoopbackListener&) = delete;

    std::uint16_t port() const noexcept
    {
        return port_;
    }

    std::string receive_line() const
    {
        int client = -1;
        do {
            client = ::accept(descriptor_, nullptr, nullptr);
        } while (client == -1 && errno == EINTR);

        if (client == -1) {
            throw std::runtime_error{"failed to accept client"};
        }

        std::string line;
        char character = '\0';
        while (true) {
            const auto received = ::recv(client, &character, 1, 0);
            if (received == 1) {
                line.push_back(character);
                if (character == '\n') {
                    break;
                }
                continue;
            }
            if (received == -1 && errno == EINTR) {
                continue;
            }

            ::close(client);
            throw std::runtime_error{"failed to receive client line"};
        }

        pollfd poll_descriptor{};
        poll_descriptor.fd = client;
        poll_descriptor.events = POLLIN;
        if (::poll(&poll_descriptor, 1, 1000) != 1) {
            ::close(client);
            throw std::runtime_error{"client did not disconnect"};
        }

        char extra_character = '\0';
        if (::recv(client, &extra_character, 1, 0) != 0) {
            ::close(client);
            throw std::runtime_error{"client connection remained open"};
        }

        ::close(client);
        return line;
    }

private:
    int descriptor_;
    std::uint16_t port_{0};
};

void test_sends_sum_to_loopback_server()
{
    LoopbackListener listener;
    std::istringstream input{"123456\nexit\n"};
    std::ostringstream output;
    std::ostringstream error_output;
    program1::Application application{
        input, output, error_output, "127.0.0.1", listener.port()};

    test_utils::expect_equal(
        application.run(), 0, "application sends result successfully");
    test_utils::expect_equal(
        listener.receive_line(), std::string{"9\n"}, "application sends exact sum line");
    test_utils::expect_true(
        output.str().find("Sum sent: 9") != std::string::npos,
        "application reports successful send");
}

void test_processes_input_and_exits()
{
    LoopbackListener listener;
    std::istringstream input{"123456\nexit\n"};
    std::ostringstream output;
    std::ostringstream error_output;
    program1::Application application{
        input, output, error_output, "127.0.0.1", listener.port()};

    test_utils::expect_equal(
        application.run(), 0, "application exits successfully");
    test_utils::expect_true(
        output.str().find("Processed: KB5KB3KB1") != std::string::npos,
        "application prints transformed value");
    test_utils::expect_true(
        output.str().find("Sum: 9") != std::string::npos,
        "application prints calculated sum");
    test_utils::expect_equal(
        listener.receive_line(), std::string{"9\n"}, "application sends calculated sum");
    test_utils::expect_true(
        error_output.str().empty(), "valid input does not produce errors");
}

void test_rejects_invalid_input_and_continues()
{
    LoopbackListener listener;
    std::istringstream input{"12a3\n13579\nexit\n"};
    std::ostringstream output;
    std::ostringstream error_output;
    program1::Application application{
        input, output, error_output, "127.0.0.1", listener.port()};

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
    test_utils::expect_equal(
        listener.receive_line(), std::string{"25\n"}, "application sends the next sum");
}

void test_stops_on_end_of_input()
{
    std::istringstream input;
    std::ostringstream output;
    std::ostringstream error_output;
    program1::Application application{
        input, output, error_output, "127.0.0.1", 1};

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
    program1::Application application{
        input, output, error_output, "127.0.0.1", 1};

    bool exception_received = false;

    try {
        application.run();
    } catch (const std::exception&) {
        exception_received = true;
    }

    test_utils::expect_true(
        exception_received, "worker output error reaches the main thread");
}

void test_reports_connection_failure_without_crashing()
{
    std::istringstream input{"123456\nexit\n"};
    std::ostringstream output;
    std::ostringstream error_output;
    program1::Application application{
        input, output, error_output, "invalid-host", 1};

    test_utils::expect_equal(
        application.run(), 0, "connection failure does not crash application");
    test_utils::expect_true(
        error_output.str().find("Error: failed to connect to TCP server")
            != std::string::npos,
        "application reports connection failure in English");
}

}  // namespace

int main()
{
    test_sends_sum_to_loopback_server();
    test_processes_input_and_exits();
    test_rejects_invalid_input_and_continues();
    test_stops_on_end_of_input();
    test_propagates_worker_error_without_terminating();
    test_reports_connection_failure_without_crashing();

    return test_utils::finish();
}
