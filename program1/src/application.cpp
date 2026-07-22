#include <program1/application.hpp>

#include <processing/processing.hpp>

#include <algorithm>
#include <istream>
#include <ostream>
#include <string>
#include <thread>
#include <utility>

namespace program1 {

Application::Application(
    std::istream& input,
    std::ostream& output,
    std::ostream& error_output)
    : input_{input}, output_{output}, error_output_{error_output}
{
}

int Application::run()
{
    std::thread worker{&Application::worker_loop, this};

    try {
        input_loop();
    } catch (...) {
        buffer_.stop();
        worker.join();
        throw;
    }

    buffer_.stop();
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

    while (true) {
        print_prompt();

        if (!std::getline(input_, input) || input == "exit") {
            return;
        }

        if (!is_valid_input(input)) {
            error_output_ << "Invalid input: enter 1 to 64 digits\n";
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
            std::optional<std::string> value = buffer_.take();

            if (!value.has_value()) {
                return;
            }

            const int sum = processing::calculate_sum(*value);
            print_result(*value, sum);
        }
    } catch (...) {
        worker_exception_ = std::current_exception();
        buffer_.stop();
    }
}

void Application::print_prompt()
{
    std::lock_guard lock{output_mutex_};
    output_ << "Enter 1 to 64 digits or 'exit': " << std::flush;
}

void Application::print_result(const std::string& value, int sum)
{
    std::lock_guard lock{output_mutex_};
    output_ << "Processed: " << value << '\n';
    output_ << "Sum: " << sum << '\n';
}

}  // namespace program1
