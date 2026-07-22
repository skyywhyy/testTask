#include <program1/tcp_client.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <utility>

namespace program1 {

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

    if (::connect(
            descriptor,
            reinterpret_cast<const sockaddr*>(&address),
            sizeof(address)) == -1) {
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

void TcpClient::disconnect()
{
    if (descriptor_ != -1) {
        ::close(descriptor_);
        descriptor_ = -1;
    }
}

bool TcpClient::is_connected() const noexcept
{
    return descriptor_ != -1;
}

}  // namespace program1
