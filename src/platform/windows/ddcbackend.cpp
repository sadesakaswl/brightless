#include "platform/ddcbackend.h"

#include <qt_windows.h>
#include <physicalmonitorenumerationapi.h>
#include <lowlevelmonitorconfigurationapi.h>
#include <limits>

namespace brightless::ddc {
namespace {
QString windowsError(DWORD code)
{
    wchar_t *message = nullptr;
    const auto size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                                        | FORMAT_MESSAGE_IGNORE_INSERTS,
                                    nullptr, code, 0, reinterpret_cast<wchar_t *>(&message), 0, nullptr);
    const auto text = size ? QString::fromWCharArray(message, int(size)).trimmed()
                           : QString::number(code);
    LocalFree(message);
    return text;
}

struct Enumeration {
    std::vector<Device> devices;
    QString error;
};

BOOL CALLBACK collectMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM context)
{
    auto &result = *reinterpret_cast<Enumeration *>(context);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    DWORD count = 0;
    if (!GetMonitorInfoW(monitor, &info)
        || !GetNumberOfPhysicalMonitorsFromHMONITOR(monitor, &count)) {
        result.error = windowsError(GetLastError());
        return TRUE; // One unsupported display must not hide other displays.
    }
    std::vector<PHYSICAL_MONITOR> physical(count);
    if (count && !GetPhysicalMonitorsFromHMONITOR(monitor, count, physical.data())) {
        result.error = windowsError(GetLastError());
        return TRUE;
    }
    for (DWORD i = 0; i < count; ++i) {
        DISPLAY_DEVICEW device{};
        device.cb = sizeof(device);
        QString id;
        if (EnumDisplayDevicesW(info.szDevice, i, &device, EDD_GET_DEVICE_INTERFACE_NAME)) {
            id = QString::fromWCharArray(device.DeviceID);
        }
        // ponytail: unusual multi-panel drivers may lack interface IDs; GDI name/index is the fallback.
        if (id.isEmpty()) {
            id = QStringLiteral("%1:%2").arg(QString::fromWCharArray(info.szDevice)).arg(i);
        }
        result.devices.push_back({QStringLiteral("windows:") + id,
            QString::fromWCharArray(physical[i].szPhysicalMonitorDescription),
            {physical[i].hPhysicalMonitor, [](void *handle) { DestroyPhysicalMonitor(handle); }},
            QStringLiteral("%1:%2").arg(QString::fromWCharArray(info.szDevice)).arg(i + 1)});
    }
    return TRUE;
}
}

std::vector<Device> enumerate(QString &error)
{
    Enumeration result;
    if (!EnumDisplayMonitors(nullptr, nullptr, collectMonitor, reinterpret_cast<LPARAM>(&result))) {
        result.error = windowsError(GetLastError());
    }
    error = result.error;
    return std::move(result.devices);
}

Values read(const Device &device, const Codes &codes)
{
    Values values;
    for (std::size_t i = 0; i < codes.size(); ++i) {
        DWORD current = 0, maximum = 0;
        if (GetVCPFeatureAndVCPFeatureReply(device.native.get(), codes[i], nullptr, &current, &maximum)
            && current <= std::numeric_limits<std::uint16_t>::max()
            && maximum <= std::numeric_limits<std::uint16_t>::max()) {
            values[i] = Value{static_cast<std::uint16_t>(current), static_cast<std::uint16_t>(maximum)};
        }
        if (i == 0 && (!values[0] || values[0]->maximum == 0)) break;
    }
    return values;
}

QString write(const Device &device, const Writes &values)
{
    QString error;
    for (const auto &[code, value] : values) {
        if (!SetVCPFeature(device.native.get(), code, value) && error.isEmpty()) {
            const auto status = GetLastError();
            error = QStringLiteral("VCP 0x%1: %2").arg(code, 2, 16, QLatin1Char('0'))
                        .arg(windowsError(status));
        }
    }
    return error;
}

} // namespace brightless::ddc
