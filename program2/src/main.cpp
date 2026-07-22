#include <program2/application.hpp>

#include <common/network_config.hpp>

#include <csignal>
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

    program2::Application application{
        network_config::kDefaultPort, std::cout, std::cerr};
    return application.run(stop_requested);
}
