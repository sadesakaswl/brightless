# Building Brightless

[Back to README](README.md)

## Requirements

**C++23**, **CMake 3.21+**, and **Qt 6.4+** with LinguistTools, Network, Quick, QuickControls2, and Widgets. Tests require Python 3; disable them with `-DBUILD_TESTING=OFF` if needed.

- **Linux:** Qt DBus, KDE Frameworks 6 Config, GlobalAccel and StatusNotifierItem, libkscreen 6.0+, libddcutil 1.2+, pkg-config, and permission to access I²C devices. SDR brightness requires KDE Plasma 6 Wayland with HDR enabled; battery triggers require UPower.
- **Windows:** Windows 10/11 x64, Visual Studio 2022 C++ tools, Windows SDK, and matching Qt MSVC libraries. No KDE or libddcutil dependencies. SDR white level requires HDR to be enabled in Windows display settings; Brightless writes the per-display SDR white level with an undocumented `DisplayConfig` API. Night Light uses the Windows SDK's Magnification API (`Magnification.lib`), not Windows' built-in Night light settings.

## Build and run

### Linux

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/brightless
```

Optional installation:

```bash
cmake --install build --prefix ~/.local
```

### Windows

From a Visual Studio developer terminal, adjust the Qt path to your installation:

```powershell
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix stage
./stage/bin/brightless.exe
```

Windows installation deploys Qt libraries and QML plugins; artifacts are unsigned. The SDR transport test intercepts native API calls while compiling the real transport; Windows builds use the real SDK structures for its packet-layout checks. Hardware validation is separate: enable HDR, compare 80/280/480-nit settings with Windows' SDR-content brightness slider, verify refresh and HDR toggling, and quit immediately after a change to check shutdown flushing. Linux installations require the distribution's Qt/KDE/libddcutil runtime packages.

## Release packaging

GitHub Actions builds all seven download formats. Linux packages use Docker (`ci/`); Windows uses MSVC and Qt deployment. Push a `v<version>` tag matching `CMakeLists.txt`, or run **Build and package** manually with **publish_release** enabled. Add `releases/<version>.md` first. Publishing waits for all builds, package installation/smoke checks, and checksum generation; published releases are never overwritten.

To build one Linux format locally, archive the source to a directory, mount it read-only at `/src`, mount an output directory at `/out`, and run `bash /src/ci/build.sh debian` (or `fedora`/`arch`) in its corresponding Docker image. Flatpak uses the Fedora image and `ci/flatpak.sh`; nested Bubblewrap needs `SYS_ADMIN` for mounts, `NET_ADMIN` for its private loopback interface, and unconfined seccomp/AppArmor/system paths inside the disposable container. Do not mount credentials or host system directories into it.

The Flatpak manifest pins Jansson 2.15.1, ddcutil 2.2.0, and libkscreen 6.7.5 for the KDE 6.11 SDK. ddcutil's development headers and pkg-config file must remain installed until Brightless is built. The export sandbox is checked before downloading the SDK, so missing Docker capabilities fail early.
