#pragma once

#include <QString>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace brightless::ddc {

// Implemented by exactly one platform source selected in CMake. All calls are serialized.
struct Device {
    QString id;
    QString name;
    std::shared_ptr<void> native;
    QString connection;
};
struct Value {
    std::uint16_t current;
    std::uint16_t maximum;
};
using Codes = std::array<std::uint8_t, 5>;
using Values = std::array<std::optional<Value>, 5>;
using Writes = std::map<std::uint8_t, std::uint16_t>;

std::vector<Device> enumerate(QString &error);
Values read(const Device &device, const Codes &codes);
QString write(const Device &device, const Writes &values);

} // namespace brightless::ddc
