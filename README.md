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
- **Modern UI** — Built with GTK4 and libadwaita

## Requirements

- Linux with DRM support
- I2C dev permissions (`/dev/i2c-*`)
- System dependencies:
  ```bash
  # Debian/Ubuntu
  sudo apt install libgtk-4-dev libadwaita-1-dev libddc-dev
  
  # Fedora
  sudo dnf install gtk4-devel libadwaita-devel
  
  # Arch Linux
  sudo pacman -S gtk4 libadwaita
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
