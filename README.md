# Brightless

A modern DDC control application for Linux external monitors.

## Features

- **Brightness, Contrast & Volume Control** — Adjust external monitor settings via DDC/CI protocol
- **Input Source Selection** — Switch between HDMI, DisplayPort, VGA, DVI, USB-C
- **Power Mode Control** — Turn monitor on, off, or to standby/suspend
- **Auto-detect Monitors** — Discovers connected monitors via DRM and reads names from EDID
- **Real-time Value Display** — Shows current values on startup
- **Mouse Scroll Support** — Scroll on sliders to adjust values (configurable step: 1-10%)
- **Settings Persistence** — Saves your preferences to `~/.config/brightless/settings.json`
- **Modern UI** — Built with Qt 6 and QML

## Requirements

- Linux with DRM support
- I2C dev permissions (`/dev/i2c-*`)
- System dependencies:
  ```bash
  # Debian/Ubuntu
  sudo apt install qt6-base-dev qt6-declarative-dev libddc-dev

  # Fedora
  sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel

  # Arch Linux
  sudo pacman -S qt6-base qt6-declarative
  ```

## Building

```bash
cargo build --release
```

## Usage

```bash
./target/release/brightless
```

### Controls

- **Sliders** — Drag to adjust brightness/contrast/volume
- **Dropdowns** — Select input source and power mode
- **Mouse Scroll** — Scroll on any slider to change values (default: 2% per tick)
- **Settings** — Click the gear icon in the titlebar to configure scroll step

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE) for details.
