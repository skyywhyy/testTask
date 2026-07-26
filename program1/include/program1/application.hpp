#pragma once

#include <program1/shared_buffer.hpp>
#include <program1/tcp_client.hpp>

#include <cstdint>
#include <chrono>
#include <deque>
#include <exception>
#include <iosfwd>
#include <mutex>
#include <string>

namespace program1 {

class Application {
public:
    Application(
        std::istream& input,
        std::ostream& output,
        std::ostream& error_output);
    Application(
        std::istream& input,
        std::ostream& output,
        std::ostream& error_output,
        std::string host,
        std::uint16_t port);
    Application(
        std::istream& input,
        std::ostream& output,
        std::ostream& error_output,
        std::string host,
        std::uint16_t port,
        std::chrono::milliseconds retry_interval);

    int run();

private:
    static bool is_valid_input(const std::string& value) noexcept;

    void input_loop();
    void worker_loop();
    void drain_buffer();
    void handle_value(std::string value);
    void queue_sum(int sum);
    bool deliver_sum(int sum);
    void report_unavailable();
    void stop();
    void print_prompt();
    void print_result(const std::string& value, int sum);
    void print_sent_sum(int sum);
    void print_error(const std::string& message);

    std::istream& input_;
    std::ostream& output_;
    std::ostream& error_output_;
    TcpClient client_;
    SharedBuffer buffer_;
    std::mutex output_mutex_;
    std::chrono::milliseconds retry_interval_;

    // Owned by the worker thread only, no synchronisation required.
    std::deque<int> pending_sums_;
    bool server_unavailable_{false};
    bool pending_overflow_reported_{false};

    std::exception_ptr worker_exception_;
};

}  // namespace program1
