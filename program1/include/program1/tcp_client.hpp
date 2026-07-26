#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace program1 {

enum class ReceiveStatus {
    // A complete line was extracted into the caller's buffer.
    ready,
    // No complete line arrived in time; the connection is still usable and any
    // partially received bytes are retained for the next call.
    timeout,
    // The peer closed the connection or the socket failed.
    disconnected,
};

class TcpClient {
public:
    TcpClient(std::string host, std::uint16_t port);
    ~TcpClient();

    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    bool connect();
    bool send_line(const std::string& message);
    ReceiveStatus receive_line(
        std::string& message,
        std::chrono::milliseconds timeout);
    void disconnect();
    bool is_connected() const noexcept;

private:
    std::string host_;
    std::uint16_t port_;
    int descriptor_{-1};
    std::string receive_buffer_;
};

}  // namespace program1
