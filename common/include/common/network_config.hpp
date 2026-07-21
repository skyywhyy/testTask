#pragma once

#include <cstdint>
#include <string_view>

namespace network_config {

inline constexpr std::string_view kDefaultHost{"127.0.0.1"};
inline constexpr std::uint16_t kDefaultPort{5050};

}  // namespace network_config
