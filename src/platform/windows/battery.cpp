#include "platform/battery.h"

#include <windows.h>

namespace brightless::battery {
void readPercentage(QObject *, std::function<void(std::optional<int>)> completion)
{
    SYSTEM_POWER_STATUS status{};
    std::optional<int> percentage;
    if (GetSystemPowerStatus(&status) && status.BatteryFlag != 255
        && !(status.BatteryFlag & 128) && status.BatteryLifePercent <= 100) {
        percentage = status.BatteryLifePercent;
    }
    completion(percentage);
}
}
