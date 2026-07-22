#include <program2/message_processor.hpp>

#include <processing/processing.hpp>

#include <algorithm>
#include <ostream>
#include <string>

namespace {

bool is_valid_message_format(const std::string& value) noexcept
{
    return !value.empty() && value.size() <= 32
        && std::all_of(value.begin(), value.end(), [](char symbol) {
               return symbol >= '0' && symbol <= '9';
           });
}

}  // namespace

namespace program2 {

MessageProcessor::MessageProcessor(std::ostream& output) : output_{output}
{
}

void MessageProcessor::process(const std::string& value)
{
    if (!is_valid_message_format(value)) {
        output_ << "Error: invalid message\n";
        return;
    }

    if (!processing::is_valid_sum(value)) {
        output_ << "Error: sum does not satisfy the condition\n";
        return;
    }

    output_ << "Received valid sum: " << value << '\n';
}

}  // namespace program2
