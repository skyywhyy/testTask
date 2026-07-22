#include <program1/application.hpp>

#include <iostream>

int main()
{
    program1::Application application{std::cin, std::cout, std::cerr};
    return application.run();
}
