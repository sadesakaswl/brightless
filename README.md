# Brightless

<p align="center">
  <img src="resources/icon.png" alt="Brightless icon" width="192">
</p>

A modern DDC control application for Linux external monitors.

## Features

- **Brightness, Contrast & Volume Control** — Adjust external monitor settings via DDC/CI
- **Input Source Selection** — Switch between HDMI, DisplayPort, VGA, DVI, and USB-C
- **Power Mode Control** — Turn monitors on, off, or to standby/suspend
- **Automatic Monitor Detection** — Discover DDC-capable displays through libddcutil
- **Mouse Scroll Support** — Scroll over sliders with a configurable 1–10% step
- **Dynamic Contrast** — Link brightness and contrast globally or per monitor
- **Settings Persistence** — Save preferences in `~/.config/brightless/settings.json`
- **Qt 6 UI** — Native C++23 backend with QML and Qt Quick Controls

## Requirements

- Linux and DDC/CI-capable external monitors
- Permission to access the system I²C devices
- A C++23 compiler
- CMake 3.21+
- Qt 6.4+ (`Quick` and `QuickControls2`)
- libddcutil 1.2+
- pkg-config

For Debian/Ubuntu:

```bash
sudo apt install cmake g++ pkg-config qt6-base-dev qt6-declarative-dev libddcutil-dev
```

For Fedora:

```bash
sudo dnf install cmake gcc-c++ pkgconf-pkg-config qt6-qtbase-devel qt6-qtdeclarative-devel libddcutil-devel
```

For Arch Linux:

```bash
sudo pacman -S cmake gcc pkgconf qt6-declarative ddcutil
```

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
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

### Controls

- **Sliders** — Drag to adjust brightness, contrast, or volume
- **Dropdowns** — Select an input source or power mode
- **Mouse wheel** — Scroll over a slider to change it
- **Settings** — Use the gear button to configure scroll and dynamic contrast

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE).
