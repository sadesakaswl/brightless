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
- **Selectable Modern UI** — Choose either Qt 6/QML or GTK4/libadwaita

## Frontends

Brightless supports two interfaces: Qt 6 with QML, and GTK4 with libadwaita.
There is no default frontend; select exactly one Cargo feature.

## Requirements

- Linux with DRM support
- I2C dev permissions (`/dev/i2c-*`)
- DDC/CI support:
  ```bash
  # Debian/Ubuntu
  sudo apt install libddc-dev
  ```
- Qt frontend dependencies:
  ```bash
  # Debian/Ubuntu
  sudo apt install qt6-base-dev qt6-declarative-dev

  # Fedora
  sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel

  # Arch Linux
  sudo pacman -S qt6-base qt6-declarative
  ```
- GTK frontend dependencies:
  ```bash
  # Debian/Ubuntu (supplies/requires the GTK4 development stack)
  sudo apt install libadwaita-1-dev

  # Fedora
  sudo dnf install gtk4-devel libadwaita-devel

  # Arch Linux
  sudo pacman -S gtk4 libadwaita
  ```

## Building

Build the Qt 6/QML frontend:

```bash
cargo build --release --features qt
```

Build the GTK4/libadwaita frontend:

```bash
cargo build --release --features gtk
```

## Usage

Launch the Qt frontend directly:

```bash
cargo run --features qt
```

Launch the GTK frontend directly:

```bash
cargo run --features gtk
```

For an already-built release binary:

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
