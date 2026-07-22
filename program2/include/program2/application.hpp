#pragma once

#include <program2/message_processor.hpp>
#include <program2/tcp_server.hpp>

#include <csignal>
#include <cstdint>
#include <iosfwd>

namespace program2 {

class Application {
public:
    Application(
        std::uint16_t port,
        std::ostream& output,
        std::ostream& error_output);

    int run(const volatile std::sig_atomic_t& stop_requested);

private:
    TcpServer server_;
    std::ostream& output_;
    std::ostream& error_output_;
    MessageProcessor processor_;
};

}  // namespace program2
