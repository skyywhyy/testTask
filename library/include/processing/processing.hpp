#pragma once

#include <string>
#include <string_view>

namespace processing {

void transform(std::string& value);

int calculate_sum(std::string_view value) noexcept;

bool is_valid_sum(std::string_view value) noexcept;

}  // namespace processing
