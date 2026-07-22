#pragma once

#include <program1/shared_buffer.hpp>
#include <program1/tcp_client.hpp>

#include <cstdint>
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

    int run();

private:
    static bool is_valid_input(const std::string& value) noexcept;

    void input_loop();
    void worker_loop();
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
    std::exception_ptr worker_exception_;
};

}  // namespace program1
