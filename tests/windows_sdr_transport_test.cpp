#include "platform/sdrbrightness.h"
#include "windows_sdr_test_api.h"

#include <QCoreApplication>
#include <array>
#include <cstring>
#include <cwchar>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace {
void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "Windows SDR transport check failed: " << message << '\n';
        std::exit(1);
    }
}

struct Device {
    DISPLAYCONFIG_PATH_INFO path{};
    std::wstring physical = L"monitor-A", name = L"HDR display", gdi = L"\\\\.\\DISPLAY1";
    bool modern = true;
    UINT32 mode = 2, white = 3500;
    LONG nameStatus = ERROR_SUCCESS, whiteStatus = ERROR_SUCCESS, sourceStatus = ERROR_SUCCESS;
};
struct Output {
    std::wstring gdi = L"\\\\.\\DISPLAY1";
    DXGI_COLOR_SPACE_TYPE color = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
    bool attached = true, queryFails = false, descriptionFails = false;
    SdrTestOutput native;
    SdrTestOutput6 native6;
};
struct Adapter {
    LUID luid{};
    std::vector<Output> outputs{Output{}};
    SdrTestAdapter native;
};
struct State {
    std::vector<Device> devices;
    std::vector<Adapter> adapters;
    SdrTestFactory factory;
    LONG bufferStatus = ERROR_SUCCESS, queryStatus = ERROR_SUCCESS, setStatus = ERROR_SUCCESS;
    int insufficient = 0, buffers = 0, queries = 0, sets = 0, releases = 0, factories = 0;
    bool factoryFails = false;
    std::array<unsigned char, 28> lastPacket{};
} state;

bool same(LUID a, LUID b) { return a.LowPart == b.LowPart && a.HighPart == b.HighPart; }

void reset()
{
    state = State{};
    Device device;
    device.path.sourceInfo.adapterId = {0x11112222u, 9};
    device.path.sourceInfo.id = 3;
    device.path.targetInfo.adapterId = {0x12345678u, -7};
    device.path.targetInfo.id = 17;
    device.path.targetInfo.targetAvailable = 1;
    state.devices.push_back(device);
    Adapter adapter;
    adapter.luid = device.path.targetInfo.adapterId;
    state.adapters.push_back(adapter);
}

template<class T> T field(const void *packet, std::size_t offset)
{
    T value;
    std::memcpy(&value, static_cast<const unsigned char *>(packet) + offset, sizeof(value));
    return value;
}
void put(void *packet, std::size_t offset, UINT32 value)
{
    std::memcpy(static_cast<unsigned char *>(packet) + offset, &value, sizeof(value));
}

template<std::size_t N> void text(wchar_t (&destination)[N], const std::wstring &source)
{
    check(source.size() < N, "fixture string fits native field");
    std::wmemcpy(destination, source.c_str(), source.size() + 1);
}

std::vector<brightless::sdr::Target> scan()
{
    QString error = QStringLiteral("old error");
    const auto targets = brightless::sdr::enumerate(error);
    check(error.isEmpty(), "successful discovery clears errors");
    return targets;
}
brightless::sdr::Target only()
{
    const auto targets = scan();
    check(targets.size() == 1, "one eligible HDR target");
    return targets.front();
}
}

LONG sdrTestBufferSizes(UINT32 flags, UINT32 *paths, UINT32 *modes)
{
    check(flags == QDC_ONLY_ACTIVE_PATHS, "only active paths requested");
    ++state.buffers;
    *paths = UINT32(state.devices.size());
    *modes = 1;
    return state.bufferStatus;
}
LONG sdrTestQuery(UINT32 flags, UINT32 *pathCount, DISPLAYCONFIG_PATH_INFO *paths,
                  UINT32 *modeCount, DISPLAYCONFIG_MODE_INFO *, void *topology)
{
    check(flags == QDC_ONLY_ACTIVE_PATHS && !topology, "correct topology query flags");
    ++state.queries;
    if (state.insufficient > 0) {
        --state.insufficient;
        return ERROR_INSUFFICIENT_BUFFER;
    }
    if (state.queryStatus != ERROR_SUCCESS) return state.queryStatus;
    check(*pathCount >= state.devices.size(), "allocated enough path records");
    for (std::size_t i = 0; i < state.devices.size(); ++i) paths[i] = state.devices[i].path;
    *pathCount = UINT32(state.devices.size());
    *modeCount = 0;
    return ERROR_SUCCESS;
}
LONG sdrTestGet(DISPLAYCONFIG_DEVICE_INFO_HEADER *packet)
{
    const auto type = UINT32(packet->type);
    if (type == DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME) {
        for (const auto &device : state.devices) {
            if (!same(packet->adapterId, device.path.sourceInfo.adapterId)
                || packet->id != device.path.sourceInfo.id) continue;
            check(packet->size == sizeof(DISPLAYCONFIG_SOURCE_DEVICE_NAME), "source name packet size");
            if (device.sourceStatus != ERROR_SUCCESS) return device.sourceStatus;
            text(reinterpret_cast<DISPLAYCONFIG_SOURCE_DEVICE_NAME *>(packet)->viewGdiDeviceName, device.gdi);
            return ERROR_SUCCESS;
        }
        check(false, "source query uses source identity");
    }
    for (const auto &device : state.devices) {
        if (!same(packet->adapterId, device.path.targetInfo.adapterId)
            || packet->id != device.path.targetInfo.id) continue;
        if (type == 15) {
            check(packet->size == 36, "modern advanced color ABI size");
            if (!device.modern) return ERROR_NOT_SUPPORTED;
            put(packet, 20, 0xFF); // Neither support/user bits nor active bit alone imply HDR.
            put(packet, 32, device.mode);
            return ERROR_SUCCESS;
        }
        if (type == DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME) {
            check(packet->size == sizeof(DISPLAYCONFIG_TARGET_DEVICE_NAME), "target name packet size");
            if (device.nameStatus != ERROR_SUCCESS) return device.nameStatus;
            auto *name = reinterpret_cast<DISPLAYCONFIG_TARGET_DEVICE_NAME *>(packet);
            text(name->monitorFriendlyDeviceName, device.name);
            text(name->monitorDevicePath, device.physical);
            return ERROR_SUCCESS;
        }
        if (type == 11) {
            check(packet->size == 24, "public white level ABI size");
            if (device.whiteStatus != ERROR_SUCCESS) return device.whiteStatus;
            put(packet, 20, device.white);
            return ERROR_SUCCESS;
        }
        check(false, "no legacy advanced color query or unknown query");
    }
    check(false, "target query uses target identity, not source identity");
    return ERROR_INVALID_PARAMETER;
}
LONG sdrTestSet(DISPLAYCONFIG_DEVICE_INFO_HEADER *packet)
{
    ++state.sets;
    check(UINT32(packet->type) == 0xFFFFFFEEu && packet->size == 28, "private setter is -18, size 28, never DPI -4");
    check(same(packet->adapterId, state.devices.front().path.targetInfo.adapterId)
          && packet->id == state.devices.front().path.targetInfo.id, "setter uses target adapter and target ID");
    std::memcpy(state.lastPacket.data(), packet, state.lastPacket.size());
    check(state.lastPacket[24] == 1 && state.lastPacket[25] == 0
          && state.lastPacket[26] == 0 && state.lastPacket[27] == 0, "apply byte and zero reserved padding");
    return state.setStatus;
}
DWORD sdrTestFormatMessage(DWORD, const void *, DWORD, DWORD, wchar_t *, DWORD, void *) { return 0; }
void sdrTestLocalFree(void *) {}
ULONG SdrTestUnknown::Release() { ++state.releases; return 1; }
HRESULT sdrTestFactory(int, void **factory)
{
    ++state.factories;
    if (state.factoryFails) return HRESULT(-1);
    *factory = &state.factory;
    return 0;
}
HRESULT SdrTestFactory::EnumAdapters1(UINT index, SdrTestAdapter **adapter)
{
    if (index >= state.adapters.size()) return DXGI_ERROR_NOT_FOUND;
    auto &native = state.adapters[index].native;
    native.adapterIndex = index;
    *adapter = &native;
    return 0;
}
HRESULT SdrTestAdapter::GetDesc1(DXGI_ADAPTER_DESC1 *description)
{
    description->AdapterLuid = state.adapters[adapterIndex].luid;
    return 0;
}
HRESULT SdrTestAdapter::EnumOutputs(UINT index, SdrTestOutput **output)
{
    auto &outputs = state.adapters[adapterIndex].outputs;
    if (index >= outputs.size()) return DXGI_ERROR_NOT_FOUND;
    auto &native = outputs[index].native;
    native.adapterIndex = adapterIndex;
    native.outputIndex = index;
    *output = &native;
    return 0;
}
HRESULT SdrTestOutput::QueryInterface(int, void **output)
{
    auto &model = state.adapters[adapterIndex].outputs[outputIndex];
    if (model.queryFails) return HRESULT(-1);
    model.native6.adapterIndex = adapterIndex;
    model.native6.outputIndex = outputIndex;
    *output = &model.native6;
    return 0;
}
HRESULT SdrTestOutput6::GetDesc1(DXGI_OUTPUT_DESC1 *description)
{
    const auto &model = state.adapters[adapterIndex].outputs[outputIndex];
    if (model.descriptionFails) return HRESULT(-1);
    text(description->DeviceName, model.gdi);
    description->AttachedToDesktop = model.attached;
    description->ColorSpace = model.color;
    return 0;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    reset();
    const auto target = only();
    check(target.nits == 280 && target.name == "HDR display" && target.key.contains("monitor-A"), "identity and white level exposed");
    check(state.sets == 0 && state.factories == 0, "discovery has no writes, modern query avoids DXGI");
    for (const auto &[nits, raw] : std::array<std::pair<int, UINT32>, 5>{{{80, 1000}, {280, 3500}, {480, 6000}, {-1, 1000}, {999, 6000}}}) {
        check(brightless::sdr::setBrightness(target, nits).isEmpty(), "native setter succeeds");
        check(field<UINT32>(state.lastPacket.data(), 20) == raw, "setter emits scaled and clamped white level");
    }
    state.setStatus = ERROR_ACCESS_DENIED;
    check(brightless::sdr::setBrightness(target, 80).contains("5"), "setter reports returned status without GetLastError");
    state.setStatus = ERROR_SUCCESS;
    state.devices.front().physical = L"monitor-B";
    const int before = state.sets;
    check(!brightless::sdr::setBrightness(target, 80).isEmpty() && state.sets == before, "stale physical monitor cannot be written");

    for (UINT32 mode : {0u, 1u}) {
        reset();
        state.devices.front().mode = mode;
        check(scan().empty() && state.factories == 0, "modern SDR/WCG does not fall back or expose controls");
        check(!brightless::sdr::setBrightness(target, 80).isEmpty() && state.sets == 0, "non-HDR target never written");
    }
    reset();
    state.devices.front().path.targetInfo.targetAvailable = 0;
    check(scan().empty(), "unavailable targets hidden");
    reset();
    state.devices.push_back(state.devices.front());
    check(scan().size() == 1, "duplicate adapter/target paths deduplicated");

    for (int failure = 0; failure < 3; ++failure) {
        reset();
        auto second = state.devices.front();
        second.path.targetInfo.id = 18;
        second.physical = L"monitor-B";
        state.devices.push_back(second);
        if (failure == 0) state.devices.front().nameStatus = ERROR_NOT_SUPPORTED;
        if (failure == 1) state.devices.front().physical.clear();
        if (failure == 2) state.devices.front().whiteStatus = ERROR_NOT_SUPPORTED;
        const auto targets = scan();
        check(targets.size() == 1 && targets.front().targetId == 18, "individual target failure does not hide valid monitor");
        check(!brightless::sdr::setBrightness(target, 80).isEmpty() && state.sets == 0, "unsupported target cannot be written");
    }
    reset();
    state.devices.front().white = std::numeric_limits<UINT32>::max();
    check(only().nits == 343597384, "white level arithmetic does not overflow");

    reset();
    state.insufficient = 2;
    check(scan().size() == 1 && state.buffers == 3 && state.queries == 3, "topology race retries with fresh buffer sizes");
    reset();
    state.insufficient = 100;
    QString error;
    check(brightless::sdr::enumerate(error).empty() && !error.isEmpty()
          && state.queries == 4, "topology retry limit is bounded");
    reset();
    state.bufferStatus = ERROR_ACCESS_DENIED;
    check(brightless::sdr::enumerate(error).empty() && error.contains("5") && state.queries == 0, "buffer-size failure is fatal");
    reset();
    state.queryStatus = ERROR_INVALID_PARAMETER;
    check(brightless::sdr::enumerate(error).empty() && error.contains("87"), "path query failure is fatal");
    reset();
    state.devices.clear();
    check(scan().empty() && state.queries == 0, "no active displays is not an error");
    check(!brightless::sdr::setBrightness(target, 80).isEmpty() && state.sets == 0, "removed target never written");

    reset();
    state.devices.front().modern = false;
    check(scan().size() == 1 && state.factories == 1 && state.releases == 4, "DXGI confirms HDR and releases every COM interface");
    state.adapters.front().outputs.front().gdi = L"\\\\.\\display1";
    check(scan().size() == 1, "DXGI GDI-name match ignores case");
    for (int failure = 0; failure < 10; ++failure) {
        reset();
        state.devices.front().modern = false;
        auto &output = state.adapters.front().outputs.front();
        if (failure == 0) state.adapters.front().luid.LowPart++;
        if (failure == 1) output.gdi = L"\\\\.\\DISPLAY9";
        if (failure == 2) output.color = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
        if (failure == 3) output.attached = false;
        if (failure == 4) state.factoryFails = true;
        if (failure == 5) output.queryFails = true;
        if (failure == 6) output.descriptionFails = true;
        if (failure == 7) state.devices.front().sourceStatus = ERROR_NOT_SUPPORTED;
        if (failure == 9) state.adapters.front().luid.HighPart++;
        if (failure == 8) {
            auto clone = output;
            clone.color = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
            state.adapters.front().outputs.push_back(clone);
        }
        check(scan().empty(), "DXGI mismatch, SDR clone or failed query fails closed");
        check(!brightless::sdr::setBrightness(target, 80).isEmpty() && state.sets == 0, "unconfirmed HDR never written");
    }
    std::cout << "Verified actual Windows SDR transport packets, HDR gating, DXGI matching, topology and errors.\n";
}
