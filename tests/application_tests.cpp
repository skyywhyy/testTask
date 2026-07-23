#include <program1/application.hpp>

#include "test_utils.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <sstream>
#include <string>
#include <thread>

namespace {

using namespace std::chrono_literals;

class BlockingInputBuffer : public std::streambuf {
public:
    explicit BlockingInputBuffer(std::string input)
        : input_{std::move(input)}
    {
    }

    void release()
    {
        {
            std::lock_guard lock{mutex_};
            released_ = true;
        }
        ready_.notify_all();
    }

protected:
    int_type underflow() override
    {
        std::unique_lock lock{mutex_};
        ready_.wait(lock, [this] {
            return position_ < input_.size() || released_;
        });

        if (position_ == input_.size()) {
            return traits_type::eof();
        }

        character_ = input_[position_++];
        setg(&character_, &character_, &character_ + 1);
        return traits_type::to_int_type(character_);
    }

private:
    std::mutex mutex_;
    std::condition_variable ready_;
    std::string input_;
    std::size_t position_{0};
    bool released_{false};
    char character_{'\0'};
};

class WaitableStringBuffer : public std::stringbuf {
public:
    bool wait_for_output(std::chrono::milliseconds timeout)
    {
        std::unique_lock lock{mutex_};
        return written_.wait_for(lock, timeout, [this] {
            return !str().empty();
        });
    }

    bool wait_for_text(const std::string& text, std::chrono::milliseconds timeout)
    {
        std::unique_lock lock{mutex_};
        return written_.wait_for(lock, timeout, [this, &text] {
            return str().find(text) != std::string::npos;
        });
    }

    std::string snapshot()
    {
        std::lock_guard lock{mutex_};
        return str();
    }

protected:
    int_type overflow(int_type character = traits_type::eof()) override
    {
        std::lock_guard lock{mutex_};
        const auto result = std::stringbuf::overflow(character);
        written_.notify_all();
        return result;
    }

private:
    std::mutex mutex_;
    std::condition_variable written_;
};

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

class DelayedLoopbackListener {
public:
    explicit DelayedLoopbackListener(std::uint16_t port)
        : descriptor_{::socket(AF_INET, SOCK_STREAM, 0)}
    {
        if (descriptor_ == -1) {
            throw std::runtime_error{"failed to create delayed listener socket"};
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (::bind(
                descriptor_,
                reinterpret_cast<const sockaddr*>(&address),
                sizeof(address)) == -1
            || ::listen(descriptor_, 1) == -1) {
            ::close(descriptor_);
            throw std::runtime_error{"failed to start delayed listener"};
        }
    }

    ~DelayedLoopbackListener()
    {
        if (descriptor_ != -1) {
            ::close(descriptor_);
        }
    }

    DelayedLoopbackListener(const DelayedLoopbackListener&) = delete;
    DelayedLoopbackListener& operator=(const DelayedLoopbackListener&) = delete;

    std::optional<std::string> receive_lines(
        std::size_t count,
        std::chrono::milliseconds timeout) const
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        pollfd listener_descriptor{};
        listener_descriptor.fd = descriptor_;
        listener_descriptor.events = POLLIN;
        if (::poll(&listener_descriptor, 1, static_cast<int>(timeout.count())) != 1) {
            return std::nullopt;
        }

        const int client = ::accept(descriptor_, nullptr, nullptr);
        if (client == -1) {
            throw std::runtime_error{"failed to accept delayed listener client"};
        }

        std::string received;
        while (static_cast<std::size_t>(std::count(received.begin(), received.end(), '\n')) < count) {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remaining <= 0ms) {
                ::close(client);
                return received;
            }

            pollfd client_descriptor{};
            client_descriptor.fd = client;
            client_descriptor.events = POLLIN;
            if (::poll(&client_descriptor, 1, static_cast<int>(remaining.count())) != 1) {
                ::close(client);
                return received;
            }

            char buffer[64];
            const auto size = ::recv(client, buffer, sizeof(buffer), 0);
            if (size > 0) {
                received.append(buffer, static_cast<std::size_t>(size));
                continue;
            }
            if (size == -1 && errno == EINTR) {
                continue;
            }

            ::close(client);
            throw std::runtime_error{"failed to receive delayed listener lines"};
        }

        ::close(client);
        return received;
    }

private:
    int descriptor_;
};

std::uint16_t unavailable_loopback_port()
{
    const int descriptor = ::socket(AF_INET, SOCK_STREAM, 0);
    if (descriptor == -1) {
        throw std::runtime_error{"failed to create unavailable-port socket"};
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(0);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(descriptor, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == -1) {
        ::close(descriptor);
        throw std::runtime_error{"failed to reserve unavailable port"};
    }

    socklen_t address_size = sizeof(address);
    if (::getsockname(descriptor, reinterpret_cast<sockaddr*>(&address), &address_size) == -1) {
        ::close(descriptor);
        throw std::runtime_error{"failed to query unavailable port"};
    }

    ::close(descriptor);
    return ntohs(address.sin_port);
}

std::size_t count_occurrences(const std::string& text, const std::string& needle)
{
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = text.find(needle, position)) != std::string::npos) {
        ++count;
        position += needle.size();
    }
    return count;
}

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
        error_output.str().find("Server is unavailable. Retrying...")
            != std::string::npos,
        "application reports unavailable server in English");
}

void test_retries_pending_sums_after_server_becomes_available()
{
    const auto port = unavailable_loopback_port();
    BlockingInputBuffer input_buffer{"123456\n13579\n"};
    std::istream input{&input_buffer};
    WaitableStringBuffer output_buffer;
    WaitableStringBuffer error_buffer;
    std::ostream output{&output_buffer};
    std::ostream error_output{&error_buffer};
    program1::Application application{
        input, output, error_output, "127.0.0.1", port, 30ms};
    std::exception_ptr application_error;

    std::thread application_thread{[&] {
        try {
            application.run();
        } catch (...) {
            application_error = std::current_exception();
        }
    }};

    const bool connection_attempted = error_buffer.wait_for_output(1s);
    DelayedLoopbackListener listener{port};
    const auto received = listener.receive_lines(2, 2500ms);
    input_buffer.release();
    application_thread.join();

    test_utils::expect_true(
        !application_error, "application reconnect run does not throw");
    test_utils::expect_true(
        connection_attempted, "application attempts connection before server starts");
    test_utils::expect_true(received.has_value(), "application reconnects to delayed server");
    if (received.has_value()) {
        test_utils::expect_equal(
            *received, std::string{"9\n25\n"}, "application preserves pending sum wire order");
    }

    const std::string error_text = error_buffer.snapshot();
    test_utils::expect_equal(
        count_occurrences(error_text, "Server is unavailable. Retrying..."),
        std::size_t{1},
        "application reports one unavailable outage");
    test_utils::expect_equal(
        count_occurrences(error_text, "Connection restored."),
        std::size_t{1},
        "application reports one restored connection");

    const std::string output_text = output_buffer.snapshot();
    test_utils::expect_equal(
        count_occurrences(output_text, "Sum sent: 9"),
        std::size_t{1},
        "application reports first sent sum once");
    test_utils::expect_equal(
        count_occurrences(output_text, "Sum sent: 25"),
        std::size_t{1},
        "application reports second sent sum once");
}

void test_stops_retry_wait_promptly_when_input_ends()
{
    const auto port = unavailable_loopback_port();
    BlockingInputBuffer input_buffer{"123456\n"};
    std::istream input{&input_buffer};
    WaitableStringBuffer output_buffer;
    WaitableStringBuffer error_buffer;
    std::ostream output{&output_buffer};
    std::ostream error_output{&error_buffer};
    program1::Application application{
        input, output, error_output, "127.0.0.1", port};
    std::exception_ptr application_error;

    std::thread application_thread{[&] {
        try {
            application.run();
        } catch (...) {
            application_error = std::current_exception();
        }
    }};

    const bool retry_started = error_buffer.wait_for_text(
        "Server is unavailable. Retrying...", 2s);
    const auto shutdown_started = std::chrono::steady_clock::now();
    input_buffer.release();
    application_thread.join();
    const auto shutdown_duration = std::chrono::steady_clock::now() - shutdown_started;

    test_utils::expect_true(
        !application_error, "application stop-during-retry run does not throw");
    test_utils::expect_true(retry_started, "application enters retry wait before input ends");
    test_utils::expect_true(
        shutdown_duration < 500ms,
        "application interrupts the one-second retry wait promptly");
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
    test_retries_pending_sums_after_server_becomes_available();
    test_stops_retry_wait_promptly_when_input_ends();

    return test_utils::finish();
}
