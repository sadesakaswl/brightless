#include "model.h"

int main()
{
    static_assert(brightless::clampPercent(-10) == 0);
    static_assert(brightless::clampPercent(42) == 42);
    static_assert(brightless::clampPercent(120) == 100);
    static_assert(brightless::clampRatio(0.0) == 0.1);
    static_assert(brightless::clampRatio(1.2) == 1.2);
    static_assert(brightless::clampRatio(5.0) == 2.0);
    static_assert(!brightless::validIndex(-1, 2));
    static_assert(brightless::validIndex(1, 2));
    static_assert(!brightless::validIndex(2, 2));

    return brightless::contrastForDynamicBrightness(50, 0.7) != 35
            || brightless::contrastForDynamicBrightness(33, 1.5) != 50
            || brightless::contrastForDynamicBrightness(80, 2.0) != 100;
}
