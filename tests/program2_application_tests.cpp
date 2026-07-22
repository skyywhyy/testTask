#include <program2/application.hpp>

#include "test_utils.hpp"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>
#include <thread>

namespace {

using namespace std::chrono_literals;

volatile std::sig_atomic_t child_stop_requested = 0;

void handle_sigint(int)
{
    child_stop_requested = 1;
}

bool install_sigint_handler()
{
    struct sigaction action {};
    action.sa_handler = handle_sigint;
    action.sa_flags = 0;

    return ::sigemptyset(&action.sa_mask) == 0
        && ::sigaction(SIGINT, &action, nullptr) == 0;
}

class FileDescriptorBuffer : public std::streambuf {
public:
    explicit FileDescriptorBuffer(int descriptor) : descriptor_{descriptor}
    {
    }

protected:
    std::streamsize xsputn(const char* text, std::streamsize count) override
    {
        std::streamsize written = 0;
        while (written < count) {
            const auto result = ::write(
                descriptor_, text + written, static_cast<std::size_t>(count - written));
            if (result > 0) {
                written += result;
                continue;
            }
            if (result == -1 && errno == EINTR) {
                continue;
            }
            break;
        }
        return written;
    }

    int_type overflow(int_type character) override
    {
        if (traits_type::eq_int_type(character, traits_type::eof())) {
            return traits_type::not_eof(character);
        }

        const char value = traits_type::to_char_type(character);
        return xsputn(&value, 1) == 1 ? character : traits_type::eof();
    }

private:
    int descriptor_;
};

class RawClient {
public:
    explicit RawClient(std::uint16_t port)
    {
        for (int attempt = 0; attempt != 100; ++attempt) {
            descriptor_ = ::socket(AF_INET, SOCK_STREAM, 0);
            if (descriptor_ == -1) {
                throw std::runtime_error{"failed to create client socket"};
            }

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_port = htons(port);
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

            if (::connect(
                    descriptor_,
                    reinterpret_cast<const sockaddr*>(&address),
                    sizeof(address)) == 0) {
                return;
            }

            close();
            std::this_thread::sleep_for(10ms);
        }

        throw std::runtime_error{"failed to connect client socket"};
    }

    ~RawClient()
    {
        close();
    }

    RawClient(const RawClient&) = delete;
    RawClient& operator=(const RawClient&) = delete;

    void close()
    {
        if (descriptor_ != -1) {
            ::close(descriptor_);
            descriptor_ = -1;
        }
    }

private:
    int descriptor_{-1};
};

std::uint16_t reserve_unused_port()
{
    const int descriptor = ::socket(AF_INET, SOCK_STREAM, 0);
    if (descriptor == -1) {
        throw std::runtime_error{"failed to create reservation socket"};
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(0);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(
            descriptor,
            reinterpret_cast<const sockaddr*>(&address),
            sizeof(address)) == -1) {
        ::close(descriptor);
        throw std::runtime_error{"failed to reserve loopback port"};
    }

    socklen_t address_size = sizeof(address);
    if (::getsockname(
            descriptor,
            reinterpret_cast<sockaddr*>(&address),
            &address_size) == -1) {
        ::close(descriptor);
        throw std::runtime_error{"failed to read reserved port"};
    }

    const auto port = ntohs(address.sin_port);
    ::close(descriptor);
    return port;
}

class ChildApplication {
public:
    explicit ChildApplication(std::uint16_t port)
    {
        int output_pipe[2]{};
        if (::pipe(output_pipe) == -1) {
            throw std::runtime_error{"failed to create child output pipe"};
        }

        child_stop_requested = 0;
        process_ = ::fork();
        if (process_ == -1) {
            ::close(output_pipe[0]);
            ::close(output_pipe[1]);
            throw std::runtime_error{"failed to fork application process"};
        }

        if (process_ == 0) {
            ::close(output_pipe[0]);
            run_child(port, output_pipe[1]);
        }

        ::close(output_pipe[1]);
        output_descriptor_ = output_pipe[0];
    }

    ~ChildApplication()
    {
        if (process_ > 0) {
            ::kill(process_, SIGKILL);
            int status = 0;
            while (::waitpid(process_, &status, 0) == -1 && errno == EINTR) {
            }
        }
        if (output_descriptor_ != -1) {
            ::close(output_descriptor_);
        }
    }

    ChildApplication(const ChildApplication&) = delete;
    ChildApplication& operator=(const ChildApplication&) = delete;

    bool wait_for_output(std::string_view expected)
    {
        const auto deadline = std::chrono::steady_clock::now() + 2s;
        while (std::chrono::steady_clock::now() < deadline) {
            if (output_.find(expected) != std::string::npos) {
                return true;
            }

            pollfd output_event{};
            output_event.fd = output_descriptor_;
            output_event.events = POLLIN;
            const int result = ::poll(&output_event, 1, 100);
            if (result == -1) {
                if (errno == EINTR) {
                    continue;
                }
                return false;
            }
            if (result == 0) {
                continue;
            }

            char buffer[256]{};
            const auto received =
                ::read(output_descriptor_, buffer, sizeof(buffer));
            if (received > 0) {
                output_.append(buffer, static_cast<std::size_t>(received));
                continue;
            }
            if (received == -1 && errno == EINTR) {
                continue;
            }
            return output_.find(expected) != std::string::npos;
        }

        return output_.find(expected) != std::string::npos;
    }

    bool interrupt()
    {
        return process_ > 0 && ::kill(process_, SIGINT) == 0;
    }

    std::optional<int> wait_for_exit(std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            int status = 0;
            const auto result = ::waitpid(process_, &status, WNOHANG);
            if (result == process_) {
                process_ = -1;
                if (WIFEXITED(status)) {
                    return WEXITSTATUS(status);
                }
                return std::nullopt;
            }
            if (result == -1 && errno != EINTR) {
                process_ = -1;
                return std::nullopt;
            }

            std::this_thread::sleep_for(10ms);
        }
        return std::nullopt;
    }

private:
    [[noreturn]] static void run_child(
        std::uint16_t port,
        int output_descriptor)
    {
        if (!install_sigint_handler()) {
            ::close(output_descriptor);
            ::_exit(2);
        }

        int result = 1;
        {
            FileDescriptorBuffer output_buffer{output_descriptor};
            std::ostream output{&output_buffer};
            program2::Application application{port, output, output};
            result = application.run(child_stop_requested);
        }

        ::close(output_descriptor);
        ::_exit(result);
    }

    pid_t process_{-1};
    int output_descriptor_{-1};
    std::string output_;
};

void expect_successful_exit(ChildApplication& child, std::string_view test_name)
{
    const auto exit_code = child.wait_for_exit(2s);
    test_utils::expect_true(exit_code.has_value(), test_name);
    if (exit_code.has_value()) {
        test_utils::expect_equal(*exit_code, 0, test_name);
    }
}

void test_sigint_stops_wait_for_client()
{
    ChildApplication child{reserve_unused_port()};
    test_utils::expect_true(
        child.wait_for_output("Server is listening"),
        "child application starts listening");
    test_utils::expect_true(
        child.interrupt(), "parent sends SIGINT while child waits for client");
    expect_successful_exit(child, "SIGINT stops child waiting for client");
}

void test_sigint_stops_wait_for_message()
{
    const auto port = reserve_unused_port();
    ChildApplication child{port};
    test_utils::expect_true(
        child.wait_for_output("Server is listening"),
        "child application starts before client connection");

    RawClient client{port};
    test_utils::expect_true(
        child.wait_for_output("Client connected."),
        "child application accepts the test client");
    test_utils::expect_true(
        child.interrupt(), "parent sends SIGINT while child waits for message");
    expect_successful_exit(child, "SIGINT stops child waiting for message");
}

}  // namespace

int main()
{
    test_sigint_stops_wait_for_client();
    test_sigint_stops_wait_for_message();

    return test_utils::finish();
}
