# Brightless

A modern DDC brightness control application for Linux external monitors.

## Features

- **Brightness & Contrast Control** — Adjust external monitor settings via DDC/CI protocol
- **Auto-detect Monitors** — Discovers connected monitors via DRM and reads names from EDID
- **Real-time Value Display** — Shows current brightness/contrast values on startup
- **DDC Range Info** — Displays supported DDC ranges in monitor subtitle
- **Mouse Scroll Support** — Scroll on sliders to adjust values (configurable step: 1-10%)
- **Settings Persistence** — Saves your preferences to `~/.config/brightless/settings.json`
- **Modern UI** — Built with GTK4 and libadwaita for a native GNOME experience

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

- **Sliders** — Drag to adjust brightness/contrast
- **Mouse Scroll** — Scroll on any slider to change values (default: 2% per tick)
- **Settings** — Click the gear icon in the titlebar to configure scroll step

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE) for details.
