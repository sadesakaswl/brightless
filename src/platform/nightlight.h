#pragma once

#include <array>

namespace brightless::nightlight {
using ColorEffect = std::array<float, 25>;

bool initialize();
void uninitialize();
bool getColorEffect(ColorEffect &effect);
bool setColorEffect(const ColorEffect &effect);
unsigned long errorCode();
}
