#pragma once

// Test-only native boundary. Windows uses real SDK packet types; Linux supplies only
// the members consumed by the transport, not a replacement Windows ABI for display paths.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <qt_windows.h>
#include <dxgi1_6.h>
#else
#include <cstdint>
using UINT = std::uint32_t;
using UINT32 = std::uint32_t;
using ULONG = std::uint32_t;
using DWORD = std::uint32_t;
using LONG = std::int32_t;
using BYTE = unsigned char;
using HRESULT = std::int32_t;
struct LUID { DWORD LowPart; LONG HighPart; };
enum DISPLAYCONFIG_DEVICE_INFO_TYPE : UINT32 {
    DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME = 1,
    DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME = 2,
    DISPLAYCONFIG_DEVICE_INFO_FORCE_UINT32 = 0xFFFFFFFFu
};
struct DISPLAYCONFIG_DEVICE_INFO_HEADER {
    DISPLAYCONFIG_DEVICE_INFO_TYPE type;
    UINT32 size;
    LUID adapterId;
    UINT32 id;
};
struct DISPLAYCONFIG_PATH_SOURCE_INFO { LUID adapterId; UINT32 id; };
struct DISPLAYCONFIG_PATH_TARGET_INFO { LUID adapterId; UINT32 id; int targetAvailable; };
struct DISPLAYCONFIG_PATH_INFO {
    DISPLAYCONFIG_PATH_SOURCE_INFO sourceInfo;
    DISPLAYCONFIG_PATH_TARGET_INFO targetInfo;
};
struct DISPLAYCONFIG_MODE_INFO { UINT32 unused; };
struct DISPLAYCONFIG_TARGET_DEVICE_NAME {
    DISPLAYCONFIG_DEVICE_INFO_HEADER header;
    wchar_t monitorFriendlyDeviceName[64];
    wchar_t monitorDevicePath[128];
};
struct DISPLAYCONFIG_SOURCE_DEVICE_NAME {
    DISPLAYCONFIG_DEVICE_INFO_HEADER header;
    wchar_t viewGdiDeviceName[32];
};
struct DXGI_ADAPTER_DESC1 { LUID AdapterLuid; };
enum DXGI_COLOR_SPACE_TYPE {
    DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 = 0,
    DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 = 12
};
struct DXGI_OUTPUT_DESC1 {
    wchar_t DeviceName[32];
    int AttachedToDesktop;
    DXGI_COLOR_SPACE_TYPE ColorSpace;
};
inline constexpr LONG ERROR_SUCCESS = 0, ERROR_ACCESS_DENIED = 5, ERROR_NOT_SUPPORTED = 50,
    ERROR_INVALID_PARAMETER = 87, ERROR_INSUFFICIENT_BUFFER = 122;
inline constexpr UINT32 QDC_ONLY_ACTIVE_PATHS = 2;
inline constexpr HRESULT DXGI_ERROR_NOT_FOUND = -2005270526;
inline constexpr DWORD FORMAT_MESSAGE_ALLOCATE_BUFFER = 0x100,
    FORMAT_MESSAGE_FROM_SYSTEM = 0x1000, FORMAT_MESSAGE_IGNORE_INSERTS = 0x200;
#define FAILED(status) ((status) < 0)
#endif

struct SdrTestUnknown { ULONG Release(); };
struct SdrTestFactory : SdrTestUnknown {
    HRESULT EnumAdapters1(UINT index, struct SdrTestAdapter **adapter);
};
struct SdrTestAdapter : SdrTestUnknown {
    UINT adapterIndex = 0;
    HRESULT GetDesc1(DXGI_ADAPTER_DESC1 *description);
    HRESULT EnumOutputs(UINT index, struct SdrTestOutput **output);
};
struct SdrTestOutput : SdrTestUnknown {
    UINT adapterIndex = 0, outputIndex = 0;
    HRESULT QueryInterface(int, void **output);
};
struct SdrTestOutput6 : SdrTestUnknown {
    UINT adapterIndex = 0, outputIndex = 0;
    HRESULT GetDesc1(DXGI_OUTPUT_DESC1 *description);
};

LONG sdrTestBufferSizes(UINT32 flags, UINT32 *paths, UINT32 *modes);
LONG sdrTestQuery(UINT32 flags, UINT32 *pathCount, DISPLAYCONFIG_PATH_INFO *paths,
                  UINT32 *modeCount, DISPLAYCONFIG_MODE_INFO *modes, void *topology);
LONG sdrTestGet(DISPLAYCONFIG_DEVICE_INFO_HEADER *packet);
LONG sdrTestSet(DISPLAYCONFIG_DEVICE_INFO_HEADER *packet);
HRESULT sdrTestFactory(int, void **factory);
DWORD sdrTestFormatMessage(DWORD, const void *, DWORD, DWORD, wchar_t *, DWORD, void *);
void sdrTestLocalFree(void *);

#define GetDisplayConfigBufferSizes sdrTestBufferSizes
#define QueryDisplayConfig sdrTestQuery
#define DisplayConfigGetDeviceInfo sdrTestGet
#define DisplayConfigSetDeviceInfo sdrTestSet
#define CreateDXGIFactory1 sdrTestFactory
#define FormatMessageW sdrTestFormatMessage
#define LocalFree sdrTestLocalFree
#define IUnknown SdrTestUnknown
#define IDXGIFactory1 SdrTestFactory
#define IDXGIAdapter1 SdrTestAdapter
#define IDXGIOutput SdrTestOutput
#define IDXGIOutput6 SdrTestOutput6
#define __uuidof(...) 0
