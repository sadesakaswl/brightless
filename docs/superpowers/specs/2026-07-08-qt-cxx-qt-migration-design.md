# Brightless Qt/cxx-qt Migration Design

## Goal

Convert Brightless from GTK4/libadwaita to Qt 6 using `cxx-qt`, while keeping the existing application behavior, features, settings format, and overall UI structure the same as the current version.

## Scope

In scope:

- Remove GTK4/libadwaita UI dependencies.
- Add Qt 6 and `cxx-qt` build/runtime dependencies.
- Preserve the existing Rust backend for DDC/CI monitor control.
- Preserve settings persistence at `~/.config/brightless/settings.json` with the same fields and defaults.
- Recreate the current UI structure in QML:
  - Brightless main window.
  - Header area with settings button.
  - Scrollable list of monitor controls.
  - Settings popup/panel.
- Preserve full feature parity:
  - Brightness control.
  - Contrast control when supported.
  - Volume control when supported.
  - Input source selection when supported.
  - Power mode selection when supported.
  - Mouse wheel/trackpad slider adjustment using configured scroll step.
  - Dynamic Contrast master setting.
  - Global vs per-monitor Dynamic Contrast.
  - Global and per-monitor Dynamic Contrast ratios.

Out of scope:

- Redesigning the application beyond what Qt requires.
- Changing the settings file schema.
- Changing DDC discovery/control behavior.
- Adding new features.

## Architecture

The migration keeps the project split between backend logic and UI logic.

Existing backend modules remain the foundation:

- `src/ddc_manager.rs` continues to own monitor discovery and DDC/CI operations.
- `src/settings.rs` continues to load and save app settings.

GTK-specific UI modules are replaced by a Qt-facing layer:

- `AppController` owns `DdcManager`, `AppSettings`, and the exposed monitor state.
- Monitor state is exposed to QML through `cxx-qt` as Qt properties and invokable methods.
- QML owns rendering and binds to the Rust-exposed state.
- User actions in QML call Rust invokable methods, which update DDC and settings immediately.

The Qt layer should not duplicate DDC logic. It should translate UI actions into calls on existing backend methods.

## UI Design

The Qt UI should match the current GTK/libadwaita behavior and structure as closely as practical.

Main window:

- Title: `Brightless`.
- Default size roughly matching the current app: 400x300.
- Header area with a settings button on the right.
- Scrollable monitor list as the main content.

Each monitor entry exposes the same controls as today:

- Monitor name as the row/card title.
- Brightness slider with percent label.
- Contrast slider with percent label when contrast is supported.
- Volume slider with percent label when volume is supported.
- Input source dropdown when input source control is supported.
- Power mode dropdown when power mode control is supported.
- Dynamic Contrast slider when Dynamic Contrast is active for that monitor.
- Per-monitor Dynamic Contrast toggle when Dynamic Contrast is enabled and global mode is disabled.

Settings panel:

- Opens from the gear/settings button.
- Contains scroll step slider from 1% to 10%.
- Contains Dynamic Contrast master switch.
- Contains apply-to-all-monitors switch.
- Contains global ratio slider from 0.1 to 2.0 when per-monitor ratios are disabled.
- Contains per-monitor ratio switch.
- Contains per-monitor ratio sliders when per-monitor ratios are enabled.
- Saves changes immediately, matching current behavior.

Mouse wheel/trackpad behavior:

- Scrolling over sliders adjusts values by the configured scroll step.
- Values remain clamped to their existing ranges.

## Data Flow

Startup:

1. Qt application starts.
2. Rust `AppController` loads settings using `AppSettings::load()`.
3. Rust `AppController` creates `DdcManager` and discovers monitors.
4. Monitor capabilities and settings-derived Dynamic Contrast state are exposed to QML.
5. Current DDC values are read and pushed into exposed properties.
6. QML renders the monitor list and settings panel.

User control changes:

1. QML updates a slider/dropdown/switch.
2. QML calls the corresponding Rust invokable method.
3. Rust updates DDC and/or settings.
4. Rust updates exposed properties where needed.
5. QML labels and visibility update through bindings.

Dynamic Contrast:

- When inactive, brightness and contrast controls are shown separately.
- When active for a monitor, brightness and contrast controls are hidden and replaced with the Dynamic Contrast slider.
- Moving the Dynamic Contrast slider sets brightness to the slider value and contrast to `round(brightness * ratio).min(100)`.
- Ratio comes from the global ratio unless per-monitor ratios are enabled.
- Per-monitor Dynamic Contrast enablement is only visible when Dynamic Contrast is enabled and global mode is disabled.

## Error Handling

Startup errors:

- If monitor discovery or initialization fails, show a small Qt error window/dialog equivalent to the current GTK error window.
- The error should include the backend error message.

Runtime DDC errors:

- Failed DDC writes should not crash the app.
- Initial behavior may log errors to stderr, matching the current implementation style.
- The UI may keep its attempted value, matching the current best-effort behavior.

Settings errors:

- Failed saves should not crash the app.
- Errors may be ignored or logged, matching current behavior.

## Testing and Verification

Automated checks:

- `cargo check` should pass after dependency/build-system changes.
- `cargo build` should validate generated Qt bridge code when Qt development packages are installed.

Manual verification:

- Launch app successfully.
- Verify monitor discovery and displayed names.
- Verify brightness, contrast, and volume sliders update values and labels.
- Verify mouse wheel/trackpad slider adjustment uses configured scroll step.
- Verify input source and power mode dropdowns call the backend.
- Verify settings save and reload from the existing config path.
- Verify Dynamic Contrast master, global/per-monitor modes, and ratios behave as before.
- Verify startup error dialog/window appears when initialization fails.

## Implementation Notes

The current files most likely affected are:

- `Cargo.toml`: replace GTK/libadwaita dependencies with Qt/cxx-qt dependencies and build dependencies.
- `src/main.rs`: replace GTK application startup with Qt application startup.
- `src/window.rs`: replace with Qt controller or remove after migration.
- `src/monitor_row.rs`: replace with Qt-exposed monitor state/model or remove after migration.
- New QML files under a resources/UI directory.
- Optional `build.rs` for `cxx-qt` generation.

The migration should keep changes focused on UI replacement. Backend behavior should only change when needed to expose data cleanly to Qt.
