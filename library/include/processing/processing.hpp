#pragma once

#include <string>

namespace processing {

// Sorts the string in descending order and replaces every even digit with
// "KB". Any character that is not a digit is kept as is. Validating the input
// is the caller's responsibility, so this function never throws on content.
void transform(std::string& value);

// Returns the sum of every digit contained in the string.
int calculate_sum(const std::string& value) noexcept;

// Returns true when the string is longer than two characters and the number it
// spells is a multiple of 32.
bool is_valid_sum(const std::string& value) noexcept;

}  // namespace processing
