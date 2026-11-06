#pragma once

#include <QString>
#include <algorithm>
#include <cstdint>
#include <vector>

namespace brightless::sdr {

struct Target {
    QString key;
    QString name;
    std::uint32_t adapterLow;
    std::int32_t adapterHigh;
    std::uint32_t targetId;
    int nits;
};

inline constexpr int whiteLevelToNits(std::uint32_t raw)
{
    return int((std::uint64_t(raw) * 80 + 500) / 1000);
}

inline constexpr std::uint32_t nitsToWhiteLevel(int nits)
{
    return std::uint32_t((std::uint64_t(std::clamp(nits, 80, 480)) * 1000 + 40) / 80);
}

// Discovery never writes. Error reports only fatal enumeration failures; unsupported targets are skipped.
std::vector<Target> enumerate(QString &error);
// Revalidates the active HDR target and physical path; empty string means success.
QString setBrightness(const Target &target, int nits);

} // namespace brightless::sdr
