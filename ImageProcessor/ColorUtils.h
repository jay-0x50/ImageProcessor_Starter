#pragma once

#include <cstdint>

namespace ip {

// Y = 0.299R + 0.587G + 0.114B. 정수 연산으로 소수점 이하를 버린다.
inline std::uint8_t luminance(const std::uint8_t* bgr) noexcept {
    return static_cast<std::uint8_t>((299u * bgr[2] + 587u * bgr[1] + 114u * bgr[0]) / 1000u);
}

} // namespace ip
