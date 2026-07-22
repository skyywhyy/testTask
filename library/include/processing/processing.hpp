#pragma once

#include <string>

namespace processing {

void transform(std::string& value);

int calculate_sum(const std::string& value) noexcept;

bool is_valid_sum(const std::string& value) noexcept;

}  // namespace processing
