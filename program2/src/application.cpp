#include <program2/application.hpp>

#include <chrono>
#include <ostream>

namespace program2 {

Application::Application(
    std::uint16_t port,
    std::ostream& output,
    std::ostream& error_output)
    : server_{port},
      output_{output},
      error_output_{error_output},
      processor_{output}
{
}

int Application::run(const volatile std::sig_atomic_t& stop_requested)
{
    constexpr std::chrono::milliseconds wait_timeout{100};

    if (!server_.start()) {
        error_output_ << "Error: failed to start server\n";
        return 1;
    }

    output_ << "Server is listening on 127.0.0.1:"
            << server_.bound_port() << std::endl;

    while (stop_requested == 0) {
        const auto client_wait = server_.wait_for_client(wait_timeout);
        if (client_wait == WaitResult::timeout) {
            continue;
        }
        if (client_wait == WaitResult::error) {
            if (stop_requested != 0) {
                break;
            }

            error_output_ << "Error: failed to wait for client\n";
            return 1;
        }

        if (!server_.accept_client()) {
            if (stop_requested != 0) {
                break;
            }
            if (server_.last_accept_would_block()) {
                continue;
            }

            error_output_ << "Error: failed to accept client\n";
            return 1;
        }

        output_ << "Client connected." << std::endl;

        while (stop_requested == 0) {
            const auto message_wait = server_.wait_for_message(wait_timeout);
            if (message_wait == WaitResult::timeout) {
                continue;
            }
            if (message_wait == WaitResult::error) {
                if (stop_requested != 0) {
                    break;
                }

                server_.disconnect_client();
                break;
            }

            const auto message = server_.receive_line();
            if (!message.has_value()) {
                break;
            }

            processor_.process(*message);
        }

        server_.disconnect_client();

        if (stop_requested == 0) {
            output_ << "Client disconnected. Waiting for a new connection..."
                    << std::endl;
        }
    }

    server_.stop();
    return 0;
}

}  // namespace program2
