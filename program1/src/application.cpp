#include <program1/application.hpp>

#include <common/network_config.hpp>
#include <processing/processing.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <istream>
#include <optional>
#include <ostream>
#include <string>
#include <thread>
#include <utility>

namespace program1 {

namespace {

constexpr std::chrono::milliseconds kAcknowledgementTimeout{500};
constexpr std::size_t kMaxPendingSums{1024};

}  // namespace

Application::Application(
    std::istream& input,
    std::ostream& output,
    std::ostream& error_output)
    : Application{
          input,
          output,
          error_output,
          "127.0.0.1",
          network_config::kDefaultPort,
          std::chrono::seconds{1}}
{
}

Application::Application(
    std::istream& input,
    std::ostream& output,
    std::ostream& error_output,
    std::string host,
    std::uint16_t port)
    : Application{
          input,
          output,
          error_output,
          std::move(host),
          port,
          std::chrono::seconds{1}}
{
}

Application::Application(
    std::istream& input,
    std::ostream& output,
    std::ostream& error_output,
    std::string host,
    std::uint16_t port,
    std::chrono::milliseconds retry_interval)
    : input_{input}
    , output_{output}
    , error_output_{error_output}
    , client_{std::move(host), port}
    , retry_interval_{retry_interval}
{
}

int Application::run()
{
    std::thread worker{&Application::worker_loop, this};

    try {
        input_loop();
    } catch (...) {
        stop();
        worker.join();
        throw;
    }

    stop();
    worker.join();

    if (worker_exception_) {
        std::rethrow_exception(worker_exception_);
    }

    return 0;
}

bool Application::is_valid_input(const std::string& value) noexcept
{
    if (value.empty() || value.size() > 64) {
        return false;
    }

    return std::all_of(value.begin(), value.end(), [](char symbol) {
        return symbol >= '0' && symbol <= '9';
    });
}

void Application::input_loop()
{
    std::string input;
    print_prompt();

    while (true) {
        if (!std::getline(input_, input) || input == "exit") {
            return;
        }

        if (!is_valid_input(input)) {
            print_error("Invalid input: enter 1 to 64 digits\n");
            continue;
        }

        processing::transform(input);

        if (!buffer_.put(std::move(input))) {
            return;
        }
    }
}

void Application::worker_loop()
{
    try {
        while (true) {
            // Emptying the shared buffer always comes first: the input thread
            // must never wait for the second program to become reachable.
            drain_buffer();

            if (!pending_sums_.empty()) {
                if (deliver_sum(pending_sums_.front())) {
                    pending_sums_.pop_front();

                    if (pending_sums_.empty()) {
                        pending_overflow_reported_ = false;
                    }

                    continue;
                }

                report_unavailable();
            }

            if (buffer_.stopped()) {
                break;
            }

            // Waiting on the buffer instead of on a bare timer keeps the retry
            // interval interruptible by new user input and by shutdown.
            auto value = pending_sums_.empty()
                ? buffer_.take()
                : buffer_.take_for(retry_interval_);

            if (value.has_value()) {
                handle_value(std::move(*value));
            }
        }
    } catch (...) {
        worker_exception_ = std::current_exception();
        stop();
    }

    client_.disconnect();
}

void Application::drain_buffer()
{
    while (auto value = buffer_.try_take()) {
        handle_value(std::move(*value));
    }
}

void Application::handle_value(std::string value)
{
    const int sum = processing::calculate_sum(value);
    print_result(value, sum);
    queue_sum(sum);
}

void Application::queue_sum(int sum)
{
    if (pending_sums_.size() >= kMaxPendingSums) {
        pending_sums_.pop_front();

        if (!pending_overflow_reported_) {
            print_error("Undelivered sums overflow: oldest values dropped\n");
            pending_overflow_reported_ = true;
        }
    }

    pending_sums_.push_back(sum);
}

bool Application::deliver_sum(int sum)
{
    if (!client_.is_connected() && !client_.connect()) {
        return false;
    }

    if (!client_.send_line(std::to_string(sum))) {
        return false;
    }

    std::string acknowledgement;
    if (!client_.receive_line(acknowledgement, kAcknowledgementTimeout)) {
        return false;
    }

    if (acknowledgement != "OK") {
        client_.disconnect();
        return false;
    }

    if (server_unavailable_) {
        print_error("Connection restored.\n");
        server_unavailable_ = false;
    }

    print_sent_sum(sum);
    return true;
}

void Application::report_unavailable()
{
    if (server_unavailable_) {
        return;
    }

    print_error("Server is unavailable. Retrying...\n");
    server_unavailable_ = true;
}

void Application::stop()
{
    buffer_.stop();
}

void Application::print_prompt()
{
    std::lock_guard lock{output_mutex_};
    output_ << "Enter 1 to 64 digits or 'exit':\n" << std::flush;
}

void Application::print_result(const std::string& value, int sum)
{
    std::lock_guard lock{output_mutex_};
    output_ << "Processed: " << value << '\n';
    output_ << "Sum: " << sum << '\n';
}

void Application::print_sent_sum(int sum)
{
    std::lock_guard lock{output_mutex_};
    output_ << "Sum sent: " << sum << '\n';
}

void Application::print_error(const std::string& message)
{
    std::lock_guard lock{output_mutex_};
    error_output_ << message;
}

}  // namespace program1
