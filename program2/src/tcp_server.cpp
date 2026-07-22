#include <program2/tcp_server.hpp>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstddef>

namespace program2 {

namespace {

void close_descriptor(int& descriptor)
{
    if (descriptor != -1) {
        ::close(descriptor);
        descriptor = -1;
    }
}

}  // namespace

TcpServer::TcpServer(std::uint16_t port)
    : requested_port_{port}
{
}

TcpServer::~TcpServer()
{
    stop();
}

bool TcpServer::start()
{
    if (server_descriptor_ != -1) {
        return true;
    }

    server_descriptor_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_descriptor_ == -1) {
        return false;
    }

    const int reuse_address = 1;
    if (::setsockopt(
            server_descriptor_,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse_address,
            sizeof(reuse_address)) == -1) {
        stop();
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(requested_port_);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(
            server_descriptor_,
            reinterpret_cast<const sockaddr*>(&address),
            sizeof(address)) == -1) {
        stop();
        return false;
    }

    if (::listen(server_descriptor_, 1) == -1) {
        stop();
        return false;
    }

    sockaddr_in bound_address{};
    socklen_t bound_address_size = sizeof(bound_address);
    if (::getsockname(
            server_descriptor_,
            reinterpret_cast<sockaddr*>(&bound_address),
            &bound_address_size) == -1) {
        stop();
        return false;
    }

    bound_port_ = ntohs(bound_address.sin_port);
    return true;
}

bool TcpServer::accept_client()
{
    if (server_descriptor_ == -1) {
        return false;
    }

    disconnect_client();
    client_descriptor_ = ::accept(server_descriptor_, nullptr, nullptr);
    return client_descriptor_ != -1;
}

std::optional<std::string> TcpServer::receive_line()
{
    if (client_descriptor_ == -1) {
        receive_buffer_.clear();
        return std::nullopt;
    }

    while (true) {
        const auto newline = receive_buffer_.find('\n');
        if (newline != std::string::npos) {
            std::string line = receive_buffer_.substr(0, newline);
            receive_buffer_.erase(0, newline + 1);
            return line;
        }

        std::array<char, 4096> chunk{};
        const auto received =
            ::recv(client_descriptor_, chunk.data(), chunk.size(), 0);
        if (received <= 0) {
            disconnect_client();
            return std::nullopt;
        }

        receive_buffer_.append(
            chunk.data(), static_cast<std::size_t>(received));
    }
}

void TcpServer::disconnect_client()
{
    close_descriptor(client_descriptor_);
    receive_buffer_.clear();
}

void TcpServer::stop()
{
    disconnect_client();
    close_descriptor(server_descriptor_);
    bound_port_ = 0;
}

std::uint16_t TcpServer::bound_port() const noexcept
{
    return bound_port_;
}

}  // namespace program2
