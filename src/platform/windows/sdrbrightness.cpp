#include "platform/sdrbrightness.h"

#include <QCoreApplication>
#ifdef BRIGHTLESS_SDR_TRANSPORT_TEST
#include "windows_sdr_test_api.h"
#else
#include <qt_windows.h>
#include <dxgi1_6.h>
#endif
#include <algorithm>
#include <cstddef>
#include <memory>

namespace brightless::sdr {
namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("SdrBrightnessController", text);
}

QString windowsError(LONG status)
{
    wchar_t *message = nullptr;
    const auto count = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, DWORD(status), 0,
        reinterpret_cast<wchar_t *>(&message), 0, nullptr);
    const auto detail = count ? QString::fromWCharArray(message, int(count)).trimmed() : QString();
    LocalFree(message);
    return tr("Windows display error %1: %2").arg(status).arg(detail);
}

// Local ABI declarations also compile with SDKs predating the public type-15 query.
struct AdvancedColorInfo {
    DISPLAYCONFIG_DEVICE_INFO_HEADER header;
    UINT32 value;
    UINT32 colorEncoding;
    UINT32 bitsPerColorChannel;
    UINT32 activeColorMode;
};

// Public units: https://learn.microsoft.com/windows/win32/api/wingdi/ns-wingdi-displayconfig_sdr_white_level
struct WhiteLevel {
    DISPLAYCONFIG_DEVICE_INFO_HEADER header;
    ULONG value;
};

// Private ABI independently described by ledoge/set_maxtml (GPL3), main_sdrwhite.c:
// https://github.com/ledoge/set_maxtml/blob/master/main_sdrwhite.c
// shortcut: undocumented setter may change across Windows versions; fail on errors, never use DPI type -4.
struct SetWhiteLevel {
    DISPLAYCONFIG_DEVICE_INFO_HEADER header;
    ULONG value;
    BYTE finalValue;
    BYTE reserved[3];
};

static_assert(sizeof(DISPLAYCONFIG_DEVICE_INFO_HEADER) == 20);
static_assert(sizeof(AdvancedColorInfo) == 36);
static_assert(sizeof(WhiteLevel) == 24);
static_assert(sizeof(SetWhiteLevel) == 28 && offsetof(SetWhiteLevel, finalValue) == 24);

DISPLAYCONFIG_DEVICE_INFO_HEADER header(UINT32 type, UINT32 size, const DISPLAYCONFIG_PATH_INFO &path)
{
    return {static_cast<DISPLAYCONFIG_DEVICE_INFO_TYPE>(type), size,
            path.targetInfo.adapterId, path.targetInfo.id};
}

struct ReleaseCom {
    void operator()(IUnknown *object) const { if (object) object->Release(); }
};
template<class T> using Com = std::unique_ptr<T, ReleaseCom>;

bool dxgiHdr(const DISPLAYCONFIG_PATH_INFO &path)
{
    DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
    source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
    source.header.size = sizeof(source);
    source.header.adapterId = path.sourceInfo.adapterId;
    source.header.id = path.sourceInfo.id;
    if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS) return false;

    IDXGIFactory1 *factoryRaw = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factoryRaw)))) return false;
    Com<IDXGIFactory1> factory(factoryRaw);
    for (UINT adapterIndex = 0; ; ++adapterIndex) {
        IDXGIAdapter1 *adapterRaw = nullptr;
        const auto status = factory->EnumAdapters1(adapterIndex, &adapterRaw);
        if (status == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(status)) return false;
        Com<IDXGIAdapter1> adapter(adapterRaw);
        DXGI_ADAPTER_DESC1 adapterInfo{};
        if (FAILED(adapter->GetDesc1(&adapterInfo))) return false;
        if (adapterInfo.AdapterLuid.LowPart != path.targetInfo.adapterId.LowPart
            || adapterInfo.AdapterLuid.HighPart != path.targetInfo.adapterId.HighPart) continue;

        bool matched = false;
        for (UINT outputIndex = 0; ; ++outputIndex) {
            IDXGIOutput *outputRaw = nullptr;
            const auto outputStatus = adapter->EnumOutputs(outputIndex, &outputRaw);
            if (outputStatus == DXGI_ERROR_NOT_FOUND) break;
            if (FAILED(outputStatus)) return false;
            Com<IDXGIOutput> output(outputRaw);
            IDXGIOutput6 *output6Raw = nullptr;
            if (FAILED(output->QueryInterface(__uuidof(IDXGIOutput6), reinterpret_cast<void **>(&output6Raw)))) return false;
            Com<IDXGIOutput6> output6(output6Raw);
            DXGI_OUTPUT_DESC1 info{};
            if (FAILED(output6->GetDesc1(&info))) return false;
            if (QString::fromWCharArray(info.DeviceName).compare(
                    QString::fromWCharArray(source.viewGdiDeviceName), Qt::CaseInsensitive) != 0) continue;
            if (!info.AttachedToDesktop || info.ColorSpace != DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) return false;
            matched = true;
        }
        // Cloned outputs sharing a GDI name must all confirm HDR, or we fail closed.
        return matched;
    }
    return false;
}

bool hdrActive(const DISPLAYCONFIG_PATH_INFO &path)
{
    AdvancedColorInfo color{};
    color.header = header(15, sizeof(color), path);
    if (DisplayConfigGetDeviceInfo(&color.header) == ERROR_SUCCESS) return color.activeColorMode == 2;
    // Legacy advancedColorEnabled also covers SDR/WCG; DXGI distinguishes actual HDR.
    return dxgiHdr(path);
}

std::vector<DISPLAYCONFIG_PATH_INFO> activePaths(QString &error)
{
    for (int attempt = 0; attempt < 4; ++attempt) {
        UINT32 pathCount = 0, modeCount = 0;
        auto status = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount);
        if (status != ERROR_SUCCESS) {
            error = windowsError(status);
            return {};
        }
        if (!pathCount) return {};
        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
        status = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr);
        if (status == ERROR_INSUFFICIENT_BUFFER) continue;
        if (status != ERROR_SUCCESS) {
            error = windowsError(status);
            return {};
        }
        paths.resize(pathCount);
        return paths;
    }
    error = tr("The display configuration kept changing. Try again.");
    return {};
}

} // namespace

std::vector<Target> enumerate(QString &error)
{
    error.clear();
    std::vector<Target> targets;
    for (const auto &path : activePaths(error)) {
        if (!path.targetInfo.targetAvailable || !hdrActive(path)) continue;
        const auto duplicate = std::any_of(targets.begin(), targets.end(), [&](const auto &target) {
            return target.adapterLow == path.targetInfo.adapterId.LowPart
                && target.adapterHigh == path.targetInfo.adapterId.HighPart && target.targetId == path.targetInfo.id;
        });
        if (duplicate) continue;

        // Skip unreadable targets without hiding other usable HDR displays.
        DISPLAYCONFIG_TARGET_DEVICE_NAME name{};
        name.header = header(DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME, sizeof(name), path);
        if (DisplayConfigGetDeviceInfo(&name.header) != ERROR_SUCCESS) continue;
        const auto physicalPath = QString::fromWCharArray(name.monitorDevicePath);
        if (physicalPath.isEmpty()) continue;
        WhiteLevel white{};
        white.header = header(11, sizeof(white), path); // GET_SDR_WHITE_LEVEL
        if (DisplayConfigGetDeviceInfo(&white.header) != ERROR_SUCCESS) continue;
        const auto key = QStringLiteral("%1:%2:%3:%4").arg(path.targetInfo.adapterId.HighPart)
            .arg(path.targetInfo.adapterId.LowPart).arg(path.targetInfo.id).arg(physicalPath);
        auto displayName = QString::fromWCharArray(name.monitorFriendlyDeviceName);
        if (displayName.isEmpty()) displayName = physicalPath;
        targets.push_back({key, displayName, path.targetInfo.adapterId.LowPart,
            path.targetInfo.adapterId.HighPart, path.targetInfo.id, whiteLevelToNits(white.value)});
    }
    return targets;
}

QString setBrightness(const Target &target, int nits)
{
    QString error;
    const auto targets = enumerate(error);
    const auto current = std::find_if(targets.begin(), targets.end(), [&](const auto &candidate) {
        return candidate.key == target.key && candidate.adapterLow == target.adapterLow
            && candidate.adapterHigh == target.adapterHigh && candidate.targetId == target.targetId;
    });
    if (current == targets.end())
        return error.isEmpty() ? tr("The HDR display is no longer available. Refresh the display list.") : error;

    SetWhiteLevel packet{};
    // Private packet type -18 for the SDR white level, not the DPI-scale type -4.
    packet.header.type = static_cast<DISPLAYCONFIG_DEVICE_INFO_TYPE>(-18);
    packet.header.size = sizeof(packet);
    packet.header.adapterId = {target.adapterLow, target.adapterHigh};
    packet.header.id = target.targetId;
    packet.value = nitsToWhiteLevel(nits);
    packet.finalValue = 1;
    const auto status = DisplayConfigSetDeviceInfo(&packet.header);
    return status == ERROR_SUCCESS ? QString() : windowsError(status);
}

} // namespace brightless::sdr
