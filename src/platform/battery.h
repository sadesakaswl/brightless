#pragma once

#include <functional>
#include <optional>

class QObject;

namespace brightless::battery {
// Completion runs on the context's thread; unavailable readings are nullopt.
void readPercentage(QObject *context, std::function<void(std::optional<int>)> completion);
}
