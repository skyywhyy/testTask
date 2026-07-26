#include <program1/tcp_client.hpp>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <utility>

namespace program1 {

namespace {

constexpr std::chrono::milliseconds kConnectTimeout{500};

bool set_non_blocking(int descriptor, bool enabled)
{
    const int flags = ::fcntl(descriptor, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }

    const int updated_flags =
        enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);

    return ::fcntl(descriptor, F_SETFL, updated_flags) != -1;
}

bool connect_within_timeout(
    int descriptor,
    const sockaddr_in& address,
    std::chrono::milliseconds timeout)
{
    if (::connect(
            descriptor,
            reinterpret_cast<const sockaddr*>(&address),
            sizeof(address)) == 0) {
        return true;
    }
    if (errno != EINPROGRESS && errno != EINTR) {
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (true) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining <= std::chrono::milliseconds::zero()) {
            return false;
        }

        pollfd descriptor_event{};
        descriptor_event.fd = descriptor;
        descriptor_event.events = POLLOUT;

        const int poll_result = ::poll(
            &descriptor_event,
            1,
            static_cast<int>(remaining.count()));
        if (poll_result == -1 && errno == EINTR) {
            continue;
        }
        if (poll_result != 1) {
            return false;
        }

        int socket_error = 0;
        socklen_t socket_error_size = sizeof(socket_error);
        if (::getsockopt(
                descriptor,
                SOL_SOCKET,
                SO_ERROR,
                &socket_error,
                &socket_error_size) == -1) {
            return false;
        }

        return socket_error == 0;
    }
}

}  // namespace

TcpClient::TcpClient(std::string host, std::uint16_t port)
    : host_{std::move(host)}
    , port_{port}
{
}

TcpClient::~TcpClient()
{
    disconnect();
}

bool TcpClient::connect()
{
    if (descriptor_ != -1) {
        return true;
    }

    const int descriptor = ::socket(AF_INET, SOCK_STREAM, 0);
    if (descriptor == -1) {
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);
    if (::inet_pton(AF_INET, host_.c_str(), &address.sin_addr) != 1) {
        ::close(descriptor);
        return false;
    }

    if (!set_non_blocking(descriptor, true)
        || !connect_within_timeout(descriptor, address, kConnectTimeout)
        || !set_non_blocking(descriptor, false)) {
        ::close(descriptor);
        return false;
    }

    descriptor_ = descriptor;
    return true;
}

bool TcpClient::send_line(const std::string& message)
{
    if (descriptor_ == -1) {
        return false;
    }

    std::string line = message;
    line.push_back('\n');

    std::size_t sent_total = 0;
    while (sent_total < line.size()) {
        const auto sent = ::send(
            descriptor_,
            line.data() + sent_total,
            line.size() - sent_total,
            MSG_NOSIGNAL);
        if (sent > 0) {
            sent_total += static_cast<std::size_t>(sent);
            continue;
        }
        if (sent == -1 && errno == EINTR) {
            continue;
        }

        disconnect();
        return false;
    }

    return true;
}

ReceiveStatus TcpClient::receive_line(
    std::string& message,
    std::chrono::milliseconds timeout)
{
    if (descriptor_ == -1) {
        return ReceiveStatus::disconnected;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (true) {
        // Bytes left over from an earlier call may already hold a full line.
        const auto newline = receive_buffer_.find('\n');
        if (newline != std::string::npos) {
            message.assign(receive_buffer_, 0, newline);
            receive_buffer_.erase(0, newline + 1);
            return ReceiveStatus::ready;
        }

        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining <= std::chrono::milliseconds::zero()) {
            // The connection stays open: an acknowledgement that is merely late
            // must not look like a lost message and trigger a resend.
            return ReceiveStatus::timeout;
        }

        pollfd descriptor_event{};
        descriptor_event.fd = descriptor_;
        descriptor_event.events = POLLIN;

        const int poll_result = ::poll(
            &descriptor_event,
            1,
            static_cast<int>(remaining.count()));
        if (poll_result == -1 && errno == EINTR) {
            continue;
        }
        if (poll_result == 0) {
            return ReceiveStatus::timeout;
        }
        if (poll_result < 0) {
            disconnect();
            return ReceiveStatus::disconnected;
        }

        std::array<char, 256> chunk{};
        const auto received = ::recv(descriptor_, chunk.data(), chunk.size(), 0);
        if (received == -1 && errno == EINTR) {
            continue;
        }
        if (received <= 0) {
            disconnect();
            return ReceiveStatus::disconnected;
        }

        receive_buffer_.append(
            chunk.data(), static_cast<std::size_t>(received));
    }
}

void TcpClient::disconnect()
{
    if (descriptor_ != -1) {
        ::close(descriptor_);
        descriptor_ = -1;
    }

    receive_buffer_.clear();
}

bool TcpClient::is_connected() const noexcept
{
    return descriptor_ != -1;
}

}  // namespace program1
