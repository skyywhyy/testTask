#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>

namespace program1 {

class SharedBuffer {
public:
    bool put(std::string value);
    std::optional<std::string> take();
    void stop();

private:
    std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::optional<std::string> value_;
    bool stopped_{false};
};

}  // namespace program1
