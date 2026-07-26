#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>

namespace program1 {

class SharedBuffer {
public:
    bool put(std::string value);
    std::optional<std::string> take();
    std::optional<std::string> take_for(std::chrono::milliseconds timeout);
    std::optional<std::string> try_take();
    void stop();
    bool stopped() const;

private:
    std::optional<std::string> extract(std::unique_lock<std::mutex>& lock);

    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::optional<std::string> value_;
    bool stopped_{false};
};

}  // namespace program1
