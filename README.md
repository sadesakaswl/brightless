# Brightless

<p align="center">
  <img src="resources/icon.png" alt="Brightless icon" width="192">
</p>

A Qt DDC/CI control application for external monitors on Linux and Windows.

## Features

- **Brightness, Contrast & Volume Control** — Adjust external monitor settings via DDC/CI
- **SDR Brightness in HDR** — Adjust SDR content brightness per HDR-enabled screen on KDE Plasma Wayland, even without DDC/CI
- **Input Source Selection** — Switch between HDMI, DisplayPort, VGA, DVI, and USB-C
- **Power Mode Control** — Turn monitors on, off, or to standby/suspend
- **Monitor Detection** — Discover displays asynchronously, refresh on Qt screen changes, or rescan using the refresh button
- **Mouse Scroll Support** — Scroll over sliders with a configurable 1–10% step; Linux also supports tray scrolling
- **Global Shortcuts & OSD** — Configurable monitor shortcuts with brightness/volume feedback; native Plasma OSD on Linux, Qt OSD on Windows
- **Dynamic Contrast** — Link brightness and contrast globally or per monitor
- **Settings Persistence** — Atomically save preferences and window size; debounce frequent settings-slider updates
- **Optional Autostart** — XDG autostart on Linux, per-user Windows login startup; disabled by default
- **Optional System Tray** — Keep Brightless available after closing its window
- **Single Instance** — Launching Brightless again restores the existing window
- **System Language** — Automatically use Chinese, English, French, German, Italian, Japanese, Korean, Polish, Portuguese, Russian, Spanish, or Turkish
- **Qt 6 UI** — Native C++23 backend with QML and Qt Quick Controls

## Requirements

Both platforms need DDC/CI-capable external monitors. Enable DDC/CI in the monitor's menu. Dock, adapter, GPU-driver, and monitor support varies; built-in laptop backlight control is not implemented. Volume controls the **monitor's speakers**, not the system audio mixer.

### Common build requirements

- A C++23 compiler
- CMake 3.21+
- Qt 6.4+ (`LinguistTools`, `Network`, `Quick`, `QuickControls2`, and `Widgets`)
- Python 3 for tests (or configure with `-DBUILD_TESTING=OFF`)

### Linux

- Permission to access the system I²C devices
- KDE Plasma 6 Wayland with HDR enabled for SDR brightness controls
- Qt `DBus`
- KDE Frameworks 6 `GlobalAccel` and `StatusNotifierItem`
- libkscreen 6.0+
- libddcutil 1.2+
- pkg-config

For Debian/Ubuntu:

```bash
sudo apt install cmake g++ pkg-config qt6-base-dev qt6-declarative-dev qt6-tools-dev libkf6globalaccel-dev libkf6statusnotifieritem-dev libkscreen-dev libddcutil-dev
```

For Fedora:

```bash
sudo dnf install cmake gcc-c++ pkgconf-pkg-config qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qttools-devel kf6-kglobalaccel-devel kf6-kstatusnotifieritem-devel libkscreen-devel libddcutil-devel
```

For Arch Linux:

```bash
sudo pacman -S cmake gcc pkgconf qt6-declarative qt6-tools kglobalaccel kstatusnotifieritem libkscreen ddcutil
```

### Windows

- Windows 10/11 x64 and a graphics driver exposing physical-monitor DDC/CI
- Visual Studio 2022 C++ tools, Windows SDK, and matching Qt MSVC libraries
- CI targets Qt 6.8.3 / MSVC 2022; no KDE or libddcutil dependencies

Windows uses Qt for the UI, tray, shortcut editor, settings, and single-instance IPC. Monitor operations use Windows Monitor Configuration APIs (`Dxva2`); global shortcut registration uses `RegisterHotKey`. HDR SDR-white-level controls are currently Linux/Plasma-only and are hidden on Windows.

## Build

### Linux

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Optional: run tests.

```bash
ctest --test-dir build --output-on-failure
```

## Run

```bash
./build/brightless
```

Install it with:

```bash
cmake --install build --prefix ~/.local
```

### Windows build and deployment

From a Visual Studio developer terminal with Qt on `CMAKE_PREFIX_PATH`:

```powershell
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix stage
./stage/bin/brightless.exe
```

Installation deploys Qt libraries and QML plugins. Run `cpack --config build/CPackConfig.cmake -C Release` to create a ZIP. Windows artifacts are unsigned; Linux ZIPs require the distribution's Qt/KDE/libddcutil runtime packages and are not standalone bundles. CI builds, tests, installs, and smoke-tests both platform artifacts; physical-monitor compatibility still requires hardware testing.

Release builds retain LTO when the compiler supports it and use `-O3` on GCC/Clang or `/O2` on MSVC. Unsupported LTO does not prevent building. No machine-specific instruction flags are enabled for release artifacts.

### Preferences and platform behavior

- Linux preferences: `$XDG_CONFIG_HOME/brightless/settings.json` (normally `~/.config/brightless/settings.json`).
- Windows preferences: `%LOCALAPPDATA%/brightless/settings.json`; global shortcut assignments use Qt `QSettings` under the per-user `Brightless/Brightless` registry key.
- Shortcuts are unassigned by default. Linux: enable Plasma shortcuts, then assign keys in Plasma settings. Windows: use **Settings → System → Configure global shortcuts**; use a single key combination per action. Conflicts or unsupported combinations are reported in the main window.
- Autostart uses a per-user entry only; no administrator rights are needed.
- DDC errors are shown in the main window. Sliders and OSD display requested values optimistically, not hardware readback; use refresh to reread the monitor.
- Per-monitor dynamic-contrast preferences migrate from model names to device IDs. Linux IDs currently include the I²C bus; moving a monitor between connectors may require reapplying its settings.

### Code layout

- `src/brightlesscontroller.*`: shared monitor state, operations, and serialized asynchronous DDC work.
- `src/settings.cpp`: validated JSON persistence and atomic writes.
- `src/platform/{linux,windows}`: compile-time-selected DDC and autostart implementations.
- `src/desktopintegration.*`: shared actions plus Qt/Plasma/native desktop integration.
- `src/singleinstance.*`: per-user local IPC and process locking through Qt Network.
- `src/qt/qml`: main window, settings window, monitor cards, and fallback OSD.

The small DDC contract is declared in `src/platform/ddcbackend.h`; tests replace that boundary without accessing hardware. Discovery and writes share one worker to avoid races with handle replacement. No plugin system is needed.

### Controls

- **Sliders** — Drag to adjust brightness, contrast, or volume
- **SDR brightness (nits)** — HDR screen cards control SDR white level separately from DDC brightness. Use the slider or enter 50–10,000 nits; mouse-wheel steps use the scroll-step setting in nits. Plasma manages this value; tray scrolling and global shortcuts still control DDC brightness.
- **Dropdowns** — Select an input source or power mode
- **Mouse wheel** — Scroll over a slider to change its value; on Linux, tray scrolling changes brightness (and contrast when Dynamic Contrast is enabled)
- **Settings** — Use the gear button to configure autostart, tray, global shortcuts, scroll, and dynamic contrast
- **System tray** — Enable “Close to tray icon” in settings, then click the tray icon or choose “Show Brightless” to restore the window

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE).
