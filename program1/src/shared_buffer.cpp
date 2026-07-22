#include <program1/shared_buffer.hpp>

#include <utility>

namespace program1 {

bool SharedBuffer::put(std::string value)
{
    std::unique_lock lock{mutex_};

    not_full_.wait(lock, [this] {
        return !value_.has_value() || stopped_;
    });

    if (stopped_) {
        return false;
    }

    value_ = std::move(value);

    lock.unlock();
    not_empty_.notify_one();

    return true;
}

std::optional<std::string> SharedBuffer::take()
{
    std::unique_lock lock{mutex_};

    not_empty_.wait(lock, [this] {
        return value_.has_value() || stopped_;
    });

    if (!value_.has_value()) {
        return std::nullopt;
    }

    std::string result = std::move(*value_);
    value_.reset();

    lock.unlock();
    not_full_.notify_one();

    return result;
}

void SharedBuffer::stop()
{
    {
        std::lock_guard lock{mutex_};
        stopped_ = true;
    }

    not_empty_.notify_all();
    not_full_.notify_all();
}

}  // namespace program1
