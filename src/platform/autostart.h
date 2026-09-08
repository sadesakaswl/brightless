#pragma once
#include <QString>

namespace brightless {
bool autostartEnabled();
// Empty result means success.
QString setAutostartEnabled(bool enabled);
}
