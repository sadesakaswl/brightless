#pragma once
#include <QDir>
#include <QStandardPaths>

namespace brightless {
inline QString configRoot()
{
#ifdef BRIGHTLESS_TESTING
    // Tests must never change a user's preferences, autostart entry, or instance lock.
    return qEnvironmentVariable("BRIGHTLESS_TEST_CONFIG");
#else
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
#endif
}
}
