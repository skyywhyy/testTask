#include <program1/application.hpp>

#include <csignal>
#include <exception>
#include <iostream>

namespace {

volatile std::sig_atomic_t stop_requested = 0;

void handle_sigint(int)
{
    stop_requested = 1;
}

bool install_sigint_handler()
{
    struct sigaction action {};
    action.sa_handler = handle_sigint;

    // No SA_RESTART: the pending read on standard input must fail with EINTR so
    // the input loop can unwind instead of waiting for another line.
    action.sa_flags = 0;

    if (::sigemptyset(&action.sa_mask) == -1) {
        return false;
    }

    return ::sigaction(SIGINT, &action, nullptr) != -1;
}

}  // namespace

int main()
{
    if (!install_sigint_handler()) {
        std::cerr << "Error: failed to install SIGINT handler\n";
        return 1;
    }

    try {
        program1::Application application{std::cin, std::cout, std::cerr};
        return application.run(stop_requested);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Error: unknown failure\n";
        return 1;
    }
}
