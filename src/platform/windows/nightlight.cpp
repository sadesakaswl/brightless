#include "platform/nightlight.h"

#include <windows.h>
#include <magnification.h>
#include <cstring>

namespace brightless::nightlight {
namespace {
unsigned long lastError = ERROR_SUCCESS;
bool checked(BOOL result)
{
    lastError = result ? ERROR_SUCCESS : GetLastError();
    return result != FALSE;
}
}

bool initialize() { return checked(MagInitialize()); }
void uninitialize() { MagUninitialize(); }
unsigned long errorCode() { return lastError; }

bool getColorEffect(ColorEffect &effect)
{
    MAGCOLOREFFECT native{};
    static_assert(sizeof(native) == sizeof(effect));
    if (!checked(MagGetFullscreenColorEffect(&native))) return false;
    std::memcpy(effect.data(), &native, sizeof(native));
    return true;
}

bool setColorEffect(const ColorEffect &effect)
{
    MAGCOLOREFFECT native{};
    static_assert(sizeof(native) == sizeof(effect));
    std::memcpy(&native, effect.data(), sizeof(native));
    return checked(MagSetFullscreenColorEffect(&native));
}
}
