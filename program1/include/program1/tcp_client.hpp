#pragma once

#include <cstdint>
#include <string>

namespace program1 {

class TcpClient {
public:
    TcpClient(std::string host, std::uint16_t port);
    ~TcpClient();

    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    bool connect();
    bool send_line(const std::string& message);
    void disconnect();
    bool is_connected() const noexcept;

private:
    std::string host_;
    std::uint16_t port_;
    int descriptor_{-1};
};

}  // namespace program1
