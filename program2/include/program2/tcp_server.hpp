#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace program2 {

class TcpServer {
public:
    explicit TcpServer(std::uint16_t port);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    bool start();
    bool accept_client();
    std::optional<std::string> receive_line();
    void disconnect_client();
    void stop();
    std::uint16_t bound_port() const noexcept;

private:
    std::uint16_t requested_port_;
    std::uint16_t bound_port_{0};
    int server_descriptor_{-1};
    int client_descriptor_{-1};
    std::string receive_buffer_;
};

}  // namespace program2
