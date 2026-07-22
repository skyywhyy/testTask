#pragma once

#include <iosfwd>
#include <string>

namespace program2 {

class MessageProcessor {
public:
    explicit MessageProcessor(std::ostream& output);

    void process(const std::string& value);

private:
    std::ostream& output_;
};

}  // namespace program2
