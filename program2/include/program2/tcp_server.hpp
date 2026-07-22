#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace program2 {

enum class WaitResult {
    ready,
    timeout,
    error,
};

class TcpServer {
public:
    explicit TcpServer(std::uint16_t port);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    bool start();
    WaitResult wait_for_client(std::chrono::milliseconds timeout) const;
    bool accept_client();
    bool last_accept_would_block() const noexcept;
    WaitResult wait_for_message(std::chrono::milliseconds timeout);
    std::optional<std::string> receive_line();
    void disconnect_client();
    void stop();
    std::uint16_t bound_port() const noexcept;

private:
    std::uint16_t requested_port_;
    std::uint16_t bound_port_{0};
    int server_descriptor_{-1};
    int client_descriptor_{-1};
    bool last_accept_would_block_{false};
    std::string receive_buffer_;
};

}  // namespace program2
