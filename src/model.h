#pragma once

#include <algorithm>
#include <array>
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

constexpr int stepPercent(int value, int step, int direction)
{
    return clampPercent(value + (direction > 0 ? step : direction < 0 ? -step : 0));
}

inline int contrastForDynamicBrightness(int brightness, double ratio)
{
    return clampPercent(static_cast<int>(std::lround(brightness * ratio)));
}

// ponytail: fixed DDC input list matches the UI; parse monitor capabilities if needed.
inline constexpr std::array inputSourceCodes{1, 3, 15, 16, 17, 18, 19, 20, 27};

constexpr int nextInputSource(int current)
{
    for (std::size_t index = 0; index < inputSourceCodes.size(); ++index) {
        if (inputSourceCodes[index] == current) {
            return inputSourceCodes[(index + 1) % inputSourceCodes.size()];
        }
    }
    return inputSourceCodes.front();
}

constexpr bool validIndex(int index, std::size_t size)
{
    return index >= 0 && static_cast<std::size_t>(index) < size;
}

} // namespace brightless
