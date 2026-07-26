#include <program1/tcp_client.hpp>

#include "test_utils.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<program1::TcpClient>);
static_assert(!std::is_copy_assignable_v<program1::TcpClient>);
static_assert(noexcept(std::declval<const program1::TcpClient&>().is_connected()));

namespace {

using namespace std::chrono_literals;

class SocketDescriptor {
public:
    explicit SocketDescriptor(int descriptor = -1) noexcept
        : descriptor_{descriptor}
    {
    }

    ~SocketDescriptor()
    {
        reset();
    }

    SocketDescriptor(const SocketDescriptor&) = delete;
    SocketDescriptor& operator=(const SocketDescriptor&) = delete;

    int get() const noexcept
    {
        return descriptor_;
    }

    int release() noexcept
    {
        const int descriptor = descriptor_;
        descriptor_ = -1;
        return descriptor;
    }

    void reset(int descriptor = -1) noexcept
    {
        if (descriptor_ != -1) {
            ::close(descriptor_);
        }
        descriptor_ = descriptor;
    }

private:
    int descriptor_;
};

class LoopbackServer {
public:
    LoopbackServer()
        : listener_{::socket(AF_INET, SOCK_STREAM, 0)}
    {
        if (listener_.get() == -1) {
            throw std::runtime_error{"failed to create listener socket"};
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(0);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (::bind(
                listener_.get(),
                reinterpret_cast<const sockaddr*>(&address),
                sizeof(address)) == -1) {
            throw std::runtime_error{"failed to bind listener socket"};
        }

        if (::listen(listener_.get(), 1) == -1) {
            throw std::runtime_error{"failed to listen on socket"};
        }

        socklen_t address_size = sizeof(address);
        if (::getsockname(
                listener_.get(),
                reinterpret_cast<sockaddr*>(&address),
                &address_size) == -1) {
            throw std::runtime_error{"failed to query listener port"};
        }

        port_ = ntohs(address.sin_port);
    }

    std::uint16_t port() const noexcept
    {
        return port_;
    }

    SocketDescriptor accept_client() const
    {
        int descriptor = -1;
        do {
            descriptor = ::accept(listener_.get(), nullptr, nullptr);
        } while (descriptor == -1 && errno == EINTR);

        if (descriptor == -1) {
            throw std::runtime_error{"failed to accept client"};
        }

        timeval timeout{};
        timeout.tv_sec = 2;
        if (::setsockopt(
                descriptor,
                SOL_SOCKET,
                SO_RCVTIMEO,
                &timeout,
                sizeof(timeout)) == -1) {
            ::close(descriptor);
            throw std::runtime_error{"failed to set receive timeout"};
        }

        return SocketDescriptor{descriptor};
    }

private:
    SocketDescriptor listener_;
    std::uint16_t port_{0};
};

std::uint16_t reserve_unused_port()
{
    LoopbackServer server;
    return server.port();
}

std::string receive_exact(int descriptor, std::size_t size)
{
    std::string result(size, '\0');
    std::size_t received = 0;

    while (received < size) {
        const auto count = ::recv(
            descriptor, result.data() + received, size - received, 0);
        if (count > 0) {
            received += static_cast<std::size_t>(count);
            continue;
        }
        if (count == -1 && errno == EINTR) {
            continue;
        }
        throw std::runtime_error{"failed to receive expected client data"};
    }

    return result;
}

void test_connects_to_loopback_server()
{
    LoopbackServer server;
    program1::TcpClient client{"127.0.0.1", server.port()};

    test_utils::expect_false(
        client.is_connected(), "client starts disconnected");
    test_utils::expect_true(client.connect(), "client connects to loopback server");
    test_utils::expect_true(
        client.is_connected(), "successful connect updates client state");

    const SocketDescriptor peer = server.accept_client();
    test_utils::expect_true(peer.get() != -1, "server accepts connected client");
}

void test_sends_message_with_exact_newline_framing()
{
    LoopbackServer server;
    program1::TcpClient client{"127.0.0.1", server.port()};
    test_utils::expect_true(client.connect(), "client connects for framed send");
    const SocketDescriptor peer = server.accept_client();

    test_utils::expect_true(client.send_line("128"), "client sends one line");
    test_utils::expect_equal(
        receive_exact(peer.get(), 4),
        std::string{"128\n"},
        "send_line appends exactly one newline");
}

void test_sends_sequential_messages_in_order()
{
    LoopbackServer server;
    program1::TcpClient client{"127.0.0.1", server.port()};
    test_utils::expect_true(client.connect(), "client connects for sequential send");
    const SocketDescriptor peer = server.accept_client();

    test_utils::expect_true(client.send_line("first"), "client sends first line");
    test_utils::expect_true(client.send_line("second"), "client sends second line");
    test_utils::expect_equal(
        receive_exact(peer.get(), 13),
        std::string{"first\nsecond\n"},
        "sequential lines arrive in order");
}

void test_unavailable_server_reports_connection_failure()
{
    const std::uint16_t port = reserve_unused_port();
    program1::TcpClient client{"127.0.0.1", port};

    test_utils::expect_false(
        client.connect(), "connection to unavailable server fails");
    test_utils::expect_false(
        client.is_connected(), "failed connect leaves client disconnected");
    test_utils::expect_false(
        client.send_line("message"), "disconnected client cannot send");
}

void test_disconnect_is_idempotent_and_updates_state()
{
    LoopbackServer server;
    program1::TcpClient client{"127.0.0.1", server.port()};
    test_utils::expect_true(client.connect(), "client connects before disconnect");
    const SocketDescriptor peer = server.accept_client();

    client.disconnect();
    client.disconnect();

    test_utils::expect_false(
        client.is_connected(), "disconnect clears connected state");
    test_utils::expect_false(
        client.send_line("message"), "send after disconnect fails");
    test_utils::expect_true(peer.get() != -1, "peer remains valid for teardown");
}

void test_closed_peer_does_not_raise_sigpipe()
{
    LoopbackServer server;
    program1::TcpClient client{"127.0.0.1", server.port()};
    test_utils::expect_true(client.connect(), "client connects before peer closes");
    SocketDescriptor peer = server.accept_client();

    linger reset_on_close{};
    reset_on_close.l_onoff = 1;
    reset_on_close.l_linger = 0;
    if (::setsockopt(
            peer.get(),
            SOL_SOCKET,
            SO_LINGER,
            &reset_on_close,
            sizeof(reset_on_close)) == -1) {
        throw std::runtime_error{"failed to configure reset-on-close"};
    }
    peer.reset();
    std::this_thread::sleep_for(20ms);

    test_utils::expect_false(
        client.send_line("after-close"),
        "send to closed peer fails without terminating process");
    test_utils::expect_false(
        client.is_connected(), "send failure disconnects client");
}

}  // namespace

int main()
{
    test_connects_to_loopback_server();
    test_sends_message_with_exact_newline_framing();
    test_sends_sequential_messages_in_order();
    test_unavailable_server_reports_connection_failure();
    test_disconnect_is_idempotent_and_updates_state();
    test_closed_peer_does_not_raise_sigpipe();

    return test_utils::finish();
}
