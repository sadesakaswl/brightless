#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace brightless {

constexpr int clampPercent(int value)
{
    return std::clamp(value, 0, 100);
}

constexpr double clampRatio(double value)
{
    return std::clamp(value, 0.1, 2.0);
}

inline int contrastForDynamicBrightness(int brightness, double ratio)
{
    return clampPercent(static_cast<int>(std::lround(brightness * ratio)));
}

constexpr bool validIndex(int index, std::size_t size)
{
    return index >= 0 && static_cast<std::size_t>(index) < size;
}

} // namespace brightless
