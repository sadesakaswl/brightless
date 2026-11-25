#include "platform/battery.h"

#include <QCoreApplication>
#include <qt_windows.h>
#include <iostream>

namespace {
SYSTEM_POWER_STATUS report{};
BOOL apiResult = TRUE;
}

BOOL getSystemPowerStatusForTest(LPSYSTEM_POWER_STATUS status)
{
    *status = report;
    return apiResult;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const struct {
        const char *name;
        BYTE flags;
        BYTE percent;
        BOOL success;
        std::optional<int> expected;
    } cases[] = {
        {"empty battery", 4, 0, TRUE, 0},
        {"discharging", 0, 49, TRUE, 49},
        {"full and charging", 9, 100, TRUE, 100},
        {"no battery", 128, 49, TRUE, std::nullopt},
        {"unknown battery status", 255, 49, TRUE, std::nullopt},
        {"unknown percentage", 0, 255, TRUE, std::nullopt},
        {"out-of-range percentage", 0, 101, TRUE, std::nullopt},
        {"API failure", 0, 49, FALSE, std::nullopt},
    };
    for (const auto &test : cases) {
        report.BatteryFlag = test.flags;
        report.BatteryLifePercent = test.percent;
        apiResult = test.success;
        int calls = 0;
        std::optional<int> percentage;
        brightless::battery::readPercentage(&app, [&](std::optional<int> value) {
            ++calls;
            percentage = value;
        });
        if (calls != 1 || percentage != test.expected) {
            std::cerr << "Windows battery reader check failed: " << test.name << '\n';
            return 1;
        }
    }
}
