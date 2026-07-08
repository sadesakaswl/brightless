# Qt/cxx-qt Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the GTK4/libadwaita Brightless UI with a Qt 6 QML UI using `cxx-qt`, while preserving the existing DDC backend, settings file, and all current behavior.

**Architecture:** Keep `ddc_manager.rs` and `settings.rs` as the backend. Add a small UI-facing state/controller layer that can be unit-tested without Qt, then expose that state to QML through `cxx-qt`. QML renders the same monitor list and settings panel structure as the current GTK UI.

**Tech Stack:** Rust 2021, Qt 6, QML, `cxx-qt = 0.9.1`, `cxx-qt-build = 0.9.1`, existing `ddc`, `ddc-i2c`, `serde`, and `serde_json` backend dependencies.

## Global Constraints

- Preserve settings path: `~/.config/brightless/settings.json`.
- Preserve settings fields and defaults from `src/settings.rs`.
- Preserve DDC VCP behavior and codes from `src/ddc_manager.rs`.
- Preserve all current UI features and Dynamic Contrast behavior.
- Use Qt 6 with `cxx-qt`; do not use GTK4 or libadwaita in the final build.
- Runtime DDC and settings save failures must not crash the app.
- Keep backend behavior changes minimal and focused on UI exposure.

---

## File Structure

- Modify `Cargo.toml`: remove GTK/libadwaita dependencies; add `cxx`, `cxx-qt`, `cxx-qt-lib`, `cxx-qt-build`, and Qt build metadata.
- Create `build.rs`: configure `cxx-qt-build` for the bridge module and QML resources.
- Keep `src/ddc_manager.rs`: DDC discovery/control backend.
- Keep `src/settings.rs`: settings load/save backend.
- Create `src/app_state.rs`: testable pure-Rust monitor/settings state helpers, including Dynamic Contrast visibility and ratio calculations.
- Create `src/qt_bridge.rs`: `cxx-qt` QObject/controller bridge exposing monitor/settings state and invokable actions to QML.
- Replace `src/main.rs`: Qt application startup and QML engine loading.
- Retire `src/window.rs` and `src/monitor_row.rs`: remove these modules from `main.rs`; delete after the Qt replacement compiles.
- Create `qml/Main.qml`: main window, header/settings button, monitor list, settings popup.
- Create `qml/MonitorCard.qml`: reusable monitor control entry.
- Modify `README.md`: replace GTK/libadwaita dependency instructions with Qt 6 dependency instructions.

---

### Task 1: Add testable UI state helpers

**Files:**
- Create: `src/app_state.rs`
- Modify: `src/main.rs`

**Interfaces:**
- Consumes: `crate::settings::AppSettings`
- Produces:
  - `pub struct MonitorUiState`
  - `pub struct MonitorCapabilities`
  - `pub fn contrast_for_dynamic_brightness(brightness: u8, ratio: f32) -> u8`
  - `pub fn dynamic_contrast_enabled_for_monitor(settings: &AppSettings, monitor_name: &str) -> bool`
  - `pub fn ratio_for_monitor(settings: &AppSettings, monitor_name: &str) -> f32`
  - `pub fn clamp_percent(value: i32) -> u8`

- [ ] **Step 1: Create failing unit tests for Dynamic Contrast state**

Create `src/app_state.rs` with this initial test-only content:

```rust
use crate::settings::AppSettings;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn contrast_is_rounded_and_clamped() {
        assert_eq!(contrast_for_dynamic_brightness(50, 0.7), 35);
        assert_eq!(contrast_for_dynamic_brightness(33, 1.5), 50);
        assert_eq!(contrast_for_dynamic_brightness(80, 2.0), 100);
    }

    #[test]
    fn monitor_dynamic_contrast_respects_master_global_and_per_monitor_flags() {
        let mut settings = AppSettings::default();
        settings.dynamic_contrast_enabled = false;
        settings.dynamic_contrast_global = true;
        assert!(!dynamic_contrast_enabled_for_monitor(&settings, "Dell"));

        settings.dynamic_contrast_enabled = true;
        settings.dynamic_contrast_global = true;
        assert!(dynamic_contrast_enabled_for_monitor(&settings, "Dell"));

        settings.dynamic_contrast_global = false;
        assert!(dynamic_contrast_enabled_for_monitor(&settings, "Dell"));

        settings.monitor_dynamic_contrast.insert("Dell".to_string(), false);
        assert!(!dynamic_contrast_enabled_for_monitor(&settings, "Dell"));

        settings.monitor_dynamic_contrast.insert("Dell".to_string(), true);
        assert!(dynamic_contrast_enabled_for_monitor(&settings, "Dell"));
    }

    #[test]
    fn ratio_uses_global_or_per_monitor_value() {
        let mut settings = AppSettings::default();
        settings.dynamic_contrast_ratio = 0.8;
        settings.dynamic_contrast_per_monitor_ratio = false;
        settings.monitor_ratios.insert("Dell".to_string(), 1.4);
        assert_eq!(ratio_for_monitor(&settings, "Dell"), 0.8);

        settings.dynamic_contrast_per_monitor_ratio = true;
        assert_eq!(ratio_for_monitor(&settings, "Dell"), 1.4);
        assert_eq!(ratio_for_monitor(&settings, "LG"), 0.8);
    }

    #[test]
    fn percent_values_are_clamped_to_slider_range() {
        assert_eq!(clamp_percent(-10), 0);
        assert_eq!(clamp_percent(42), 42);
        assert_eq!(clamp_percent(120), 100);
    }
}
```

Modify the top of `src/main.rs` so the module exists during tests:

```rust
mod app_state;
mod ddc_manager;
mod monitor_row;
mod settings;
mod window;
```

- [ ] **Step 2: Run tests to verify they fail**

Run:

```bash
cargo test app_state --lib
```

Expected: compilation fails with missing functions such as `contrast_for_dynamic_brightness`, because only tests exist.

- [ ] **Step 3: Implement the state helpers**

Replace `src/app_state.rs` with:

```rust
use crate::settings::AppSettings;

#[derive(Debug, Clone, PartialEq)]
pub struct MonitorCapabilities {
    pub supports_contrast: bool,
    pub supports_volume: bool,
    pub supports_input_source: bool,
    pub supports_power_mode: bool,
}

#[derive(Debug, Clone, PartialEq)]
pub struct MonitorUiState {
    pub name: String,
    pub brightness: u8,
    pub contrast: u8,
    pub volume: u8,
    pub input_source_code: u8,
    pub power_mode_code: u8,
    pub dynamic_contrast_brightness: u8,
    pub dynamic_contrast_enabled: bool,
    pub dynamic_contrast_toggle_visible: bool,
    pub dynamic_contrast_ratio: f32,
    pub capabilities: MonitorCapabilities,
}

pub fn contrast_for_dynamic_brightness(brightness: u8, ratio: f32) -> u8 {
    ((brightness as f32 * ratio).round() as u8).min(100)
}

pub fn dynamic_contrast_enabled_for_monitor(settings: &AppSettings, monitor_name: &str) -> bool {
    if !settings.dynamic_contrast_enabled {
        return false;
    }

    if settings.dynamic_contrast_global {
        return true;
    }

    *settings
        .monitor_dynamic_contrast
        .get(monitor_name)
        .unwrap_or(&true)
}

pub fn ratio_for_monitor(settings: &AppSettings, monitor_name: &str) -> f32 {
    if settings.dynamic_contrast_per_monitor_ratio {
        *settings
            .monitor_ratios
            .get(monitor_name)
            .unwrap_or(&settings.dynamic_contrast_ratio)
    } else {
        settings.dynamic_contrast_ratio
    }
}

pub fn clamp_percent(value: i32) -> u8 {
    value.clamp(0, 100) as u8
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn contrast_is_rounded_and_clamped() {
        assert_eq!(contrast_for_dynamic_brightness(50, 0.7), 35);
        assert_eq!(contrast_for_dynamic_brightness(33, 1.5), 50);
        assert_eq!(contrast_for_dynamic_brightness(80, 2.0), 100);
    }

    #[test]
    fn monitor_dynamic_contrast_respects_master_global_and_per_monitor_flags() {
        let mut settings = AppSettings::default();
        settings.dynamic_contrast_enabled = false;
        settings.dynamic_contrast_global = true;
        assert!(!dynamic_contrast_enabled_for_monitor(&settings, "Dell"));

        settings.dynamic_contrast_enabled = true;
        settings.dynamic_contrast_global = true;
        assert!(dynamic_contrast_enabled_for_monitor(&settings, "Dell"));

        settings.dynamic_contrast_global = false;
        assert!(dynamic_contrast_enabled_for_monitor(&settings, "Dell"));

        settings.monitor_dynamic_contrast.insert("Dell".to_string(), false);
        assert!(!dynamic_contrast_enabled_for_monitor(&settings, "Dell"));

        settings.monitor_dynamic_contrast.insert("Dell".to_string(), true);
        assert!(dynamic_contrast_enabled_for_monitor(&settings, "Dell"));
    }

    #[test]
    fn ratio_uses_global_or_per_monitor_value() {
        let mut settings = AppSettings::default();
        settings.dynamic_contrast_ratio = 0.8;
        settings.dynamic_contrast_per_monitor_ratio = false;
        settings.monitor_ratios.insert("Dell".to_string(), 1.4);
        assert_eq!(ratio_for_monitor(&settings, "Dell"), 0.8);

        settings.dynamic_contrast_per_monitor_ratio = true;
        assert_eq!(ratio_for_monitor(&settings, "Dell"), 1.4);
        assert_eq!(ratio_for_monitor(&settings, "LG"), 0.8);
    }

    #[test]
    fn percent_values_are_clamped_to_slider_range() {
        assert_eq!(clamp_percent(-10), 0);
        assert_eq!(clamp_percent(42), 42);
        assert_eq!(clamp_percent(120), 100);
    }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run:

```bash
cargo test app_state --lib
```

Expected: all four `app_state` tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/app_state.rs src/main.rs
git commit -m "test: add Qt migration state helpers"
```

---

### Task 2: Add Qt/cxx-qt build scaffold

**Files:**
- Modify: `Cargo.toml`
- Create: `build.rs`
- Create: `src/qt_bridge.rs`
- Modify: `src/main.rs`

**Interfaces:**
- Consumes: `src/app_state.rs`, `src/settings.rs`, `src/ddc_manager.rs`
- Produces:
  - `pub mod qt_bridge`
  - A `build.rs` that runs `cxx_qt_build::CxxQtBuilder`
  - A minimal `BrightlessController` QObject name for QML registration/loading work in later tasks

- [ ] **Step 1: Replace Cargo dependencies with Qt dependencies**

Edit `Cargo.toml` so dependency sections are:

```toml
[dependencies]
cxx = "1"
cxx-qt = "0.9.1"
cxx-qt-lib = "0.9.1"
ddc = "0.2"
ddc-i2c = { version = "0.2", features = ["with-linux", "with-linux-enumerate"] }
i2c-linux = "0.1"
libc = "0.2"
thiserror = "2"
dirs = "5"
serde = { version = "1", features = ["derive"] }
serde_json = "1"

[build-dependencies]
cxx-qt-build = "0.9.1"
```

- [ ] **Step 2: Add the cxx-qt build script**

Create `build.rs`:

```rust
fn main() {
    cxx_qt_build::CxxQtBuilder::new()
        .file("src/qt_bridge.rs")
        .build();
}
```

- [ ] **Step 3: Add a minimal bridge module**

Create `src/qt_bridge.rs`:

```rust
#[cxx_qt::bridge]
mod ffi {
    unsafe extern "C++" {
        include!(<QtCore/QObject>);
        type QObject;
    }

    extern "RustQt" {
        #[qobject]
        #[qml_element]
        #[qproperty(QString, startup_error)]
        type BrightlessController = super::BrightlessControllerRust;
    }
}

#[derive(Default)]
pub struct BrightlessControllerRust {
    startup_error: cxx_qt_lib::QString,
}
```

- [ ] **Step 4: Make `main.rs` compile without GTK modules**

Replace `src/main.rs` with:

```rust
mod app_state;
mod ddc_manager;
mod qt_bridge;
mod settings;

fn main() {
    println!("Brightless Qt bootstrap is available; QML loading is added in the next task.");
}
```

- [ ] **Step 5: Run check to validate scaffold**

Run:

```bash
cargo check
```

Expected: the command compiles Rust and the generated `cxx-qt` bridge. If it fails because Qt development tools are missing, install Qt 6 development packages for the distribution and rerun.

- [ ] **Step 6: Commit**

```bash
git add Cargo.toml Cargo.lock build.rs src/main.rs src/qt_bridge.rs
git commit -m "build: add Qt cxx-qt scaffold"
```

---

### Task 3: Expose monitor and settings state through the Qt bridge

**Files:**
- Modify: `src/qt_bridge.rs`
- Modify: `src/app_state.rs`

**Interfaces:**
- Consumes:
  - `AppSettings::load()` / `save()`
  - `DdcManager::new()` and DDC getters/setters
  - `contrast_for_dynamic_brightness`, `dynamic_contrast_enabled_for_monitor`, `ratio_for_monitor`, `clamp_percent`
- Produces QML-facing controller methods:
  - `monitor_count() -> i32`
  - `monitor_name(index: i32) -> QString`
  - `brightness(index: i32) -> i32`
  - `set_brightness(index: i32, value: i32)`
  - `contrast(index: i32) -> i32`
  - `set_contrast(index: i32, value: i32)`
  - `volume(index: i32) -> i32`
  - `set_volume(index: i32, value: i32)`
  - `input_source_code(index: i32) -> i32`
  - `set_input_source(index: i32, code: i32)`
  - `power_mode_code(index: i32) -> i32`
  - `set_power_mode(index: i32, code: i32)`
  - `supports_contrast(index: i32) -> bool`
  - `supports_volume(index: i32) -> bool`
  - `supports_input_source(index: i32) -> bool`
  - `supports_power_mode(index: i32) -> bool`
  - `scroll_step() -> i32`
  - `set_scroll_step(value: i32)`
  - `dynamic_contrast_enabled() -> bool`
  - `set_dynamic_contrast_enabled(value: bool)`
  - `dynamic_contrast_global() -> bool`
  - `set_dynamic_contrast_global(value: bool)`
  - `dynamic_contrast_ratio() -> f32`
  - `set_dynamic_contrast_ratio(value: f32)`
  - `dynamic_contrast_per_monitor_ratio() -> bool`
  - `set_dynamic_contrast_per_monitor_ratio(value: bool)`
  - `monitor_dynamic_contrast_enabled(index: i32) -> bool`
  - `set_monitor_dynamic_contrast_enabled(index: i32, value: bool)`
  - `monitor_ratio(index: i32) -> f32`
  - `set_monitor_ratio(index: i32, value: f32)`
  - `set_dynamic_contrast_brightness(index: i32, value: i32)`

- [ ] **Step 1: Add index and ratio helper tests**

Append these tests inside `src/app_state.rs` test module:

```rust
    #[test]
    fn ratio_values_are_clamped_to_current_ui_range() {
        assert_eq!(clamp_ratio(0.0), 0.1);
        assert_eq!(clamp_ratio(1.2), 1.2);
        assert_eq!(clamp_ratio(5.0), 2.0);
    }

    #[test]
    fn index_conversion_rejects_negative_and_out_of_range_values() {
        assert_eq!(valid_index(-1, 2), None);
        assert_eq!(valid_index(0, 2), Some(0));
        assert_eq!(valid_index(1, 2), Some(1));
        assert_eq!(valid_index(2, 2), None);
    }
```

- [ ] **Step 2: Run tests to verify they fail**

Run:

```bash
cargo test app_state --lib
```

Expected: compilation fails because `clamp_ratio` and `valid_index` are not defined.

- [ ] **Step 3: Add helper functions**

Add to `src/app_state.rs` before the test module:

```rust
pub fn clamp_ratio(value: f32) -> f32 {
    value.clamp(0.1, 2.0)
}

pub fn valid_index(index: i32, len: usize) -> Option<usize> {
    if index < 0 {
        return None;
    }

    let index = index as usize;
    if index < len {
        Some(index)
    } else {
        None
    }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run:

```bash
cargo test app_state --lib
```

Expected: all `app_state` tests pass.

- [ ] **Step 5: Replace bridge storage with backend-backed controller skeleton**

Replace `src/qt_bridge.rs` with a controller that stores `DdcManager`, `AppSettings`, and `Vec<MonitorUiState>`. The implementation should use this structure exactly so QML-facing methods have stable storage:

```rust
use crate::app_state::{
    clamp_percent, clamp_ratio, contrast_for_dynamic_brightness,
    dynamic_contrast_enabled_for_monitor, ratio_for_monitor, valid_index, MonitorCapabilities,
    MonitorUiState,
};
use crate::ddc_manager::{DdcManager, InputSource, PowerMode};
use crate::settings::AppSettings;
use std::cell::RefCell;

#[cxx_qt::bridge]
mod ffi {
    unsafe extern "C++" {
        include!(<QtCore/QObject>);
        type QObject;
    }

    extern "RustQt" {
        #[qobject]
        #[qml_element]
        #[qproperty(QString, startup_error)]
        type BrightlessController = super::BrightlessControllerRust;

        #[qinvokable]
        fn initialize(self: Pin<&mut BrightlessController>);
        #[qinvokable]
        fn monitor_count(self: &BrightlessController) -> i32;
        #[qinvokable]
        fn monitor_name(self: &BrightlessController, index: i32) -> cxx_qt_lib::QString;
        #[qinvokable]
        fn brightness(self: &BrightlessController, index: i32) -> i32;
        #[qinvokable]
        fn set_brightness(self: Pin<&mut BrightlessController>, index: i32, value: i32);
        #[qinvokable]
        fn contrast(self: &BrightlessController, index: i32) -> i32;
        #[qinvokable]
        fn set_contrast(self: Pin<&mut BrightlessController>, index: i32, value: i32);
        #[qinvokable]
        fn volume(self: &BrightlessController, index: i32) -> i32;
        #[qinvokable]
        fn set_volume(self: Pin<&mut BrightlessController>, index: i32, value: i32);
        #[qinvokable]
        fn input_source_code(self: &BrightlessController, index: i32) -> i32;
        #[qinvokable]
        fn set_input_source(self: Pin<&mut BrightlessController>, index: i32, code: i32);
        #[qinvokable]
        fn power_mode_code(self: &BrightlessController, index: i32) -> i32;
        #[qinvokable]
        fn set_power_mode(self: Pin<&mut BrightlessController>, index: i32, code: i32);
        #[qinvokable]
        fn supports_contrast(self: &BrightlessController, index: i32) -> bool;
        #[qinvokable]
        fn supports_volume(self: &BrightlessController, index: i32) -> bool;
        #[qinvokable]
        fn supports_input_source(self: &BrightlessController, index: i32) -> bool;
        #[qinvokable]
        fn supports_power_mode(self: &BrightlessController, index: i32) -> bool;
        #[qinvokable]
        fn scroll_step(self: &BrightlessController) -> i32;
        #[qinvokable]
        fn set_scroll_step(self: Pin<&mut BrightlessController>, value: i32);
        #[qinvokable]
        fn dynamic_contrast_enabled(self: &BrightlessController) -> bool;
        #[qinvokable]
        fn set_dynamic_contrast_enabled(self: Pin<&mut BrightlessController>, value: bool);
        #[qinvokable]
        fn dynamic_contrast_global(self: &BrightlessController) -> bool;
        #[qinvokable]
        fn set_dynamic_contrast_global(self: Pin<&mut BrightlessController>, value: bool);
        #[qinvokable]
        fn dynamic_contrast_ratio(self: &BrightlessController) -> f32;
        #[qinvokable]
        fn set_dynamic_contrast_ratio(self: Pin<&mut BrightlessController>, value: f32);
        #[qinvokable]
        fn dynamic_contrast_per_monitor_ratio(self: &BrightlessController) -> bool;
        #[qinvokable]
        fn set_dynamic_contrast_per_monitor_ratio(self: Pin<&mut BrightlessController>, value: bool);
        #[qinvokable]
        fn monitor_dynamic_contrast_enabled(self: &BrightlessController, index: i32) -> bool;
        #[qinvokable]
        fn set_monitor_dynamic_contrast_enabled(self: Pin<&mut BrightlessController>, index: i32, value: bool);
        #[qinvokable]
        fn monitor_ratio(self: &BrightlessController, index: i32) -> f32;
        #[qinvokable]
        fn set_monitor_ratio(self: Pin<&mut BrightlessController>, index: i32, value: f32);
        #[qinvokable]
        fn set_dynamic_contrast_brightness(self: Pin<&mut BrightlessController>, index: i32, value: i32);
    }
}

pub struct BrightlessControllerRust {
    startup_error: cxx_qt_lib::QString,
    ddc: RefCell<Option<DdcManager>>,
    settings: RefCell<AppSettings>,
    monitors: RefCell<Vec<MonitorUiState>>,
}

impl Default for BrightlessControllerRust {
    fn default() -> Self {
        Self {
            startup_error: cxx_qt_lib::QString::default(),
            ddc: RefCell::new(None),
            settings: RefCell::new(AppSettings::load()),
            monitors: RefCell::new(Vec::new()),
        }
    }
}
```

- [ ] **Step 6: Implement the bridge methods**

Add an `impl ffi::BrightlessController` block in `src/qt_bridge.rs`. The implementation must:

```rust
impl ffi::BrightlessController {
    pub fn initialize(mut self: std::pin::Pin<&mut Self>) {
        let settings = self.as_ref().rust().settings.borrow().clone();
        match DdcManager::new() {
            Ok(mut ddc) => {
                let mut states = Vec::new();
                for i in 0..ddc.monitors.len() {
                    let monitor = &ddc.monitors[i];
                    let name = monitor.name.clone();
                    let brightness = ddc.get_brightness_percentage(i).unwrap_or(50);
                    let contrast = ddc.get_contrast_percentage(i).unwrap_or(50);
                    let volume = ddc.get_volume_percentage(i).unwrap_or(50);
                    let input_source_code = ddc.get_input_source(i).map(|s| s.code()).unwrap_or(0);
                    let power_mode_code = ddc.get_power_mode(i).map(|m| m.code()).unwrap_or(0);
                    let dc_enabled = dynamic_contrast_enabled_for_monitor(&settings, &name);
                    let ratio = ratio_for_monitor(&settings, &name);
                    states.push(MonitorUiState {
                        name,
                        brightness,
                        contrast,
                        volume,
                        input_source_code,
                        power_mode_code,
                        dynamic_contrast_brightness: brightness,
                        dynamic_contrast_enabled: dc_enabled,
                        dynamic_contrast_toggle_visible: settings.dynamic_contrast_enabled && !settings.dynamic_contrast_global,
                        dynamic_contrast_ratio: ratio,
                        capabilities: MonitorCapabilities {
                            supports_contrast: monitor.max_contrast > 0,
                            supports_volume: monitor.max_volume > 0,
                            supports_input_source: monitor.supports_input_source,
                            supports_power_mode: monitor.supports_power_mode,
                        },
                    });
                }
                *self.as_ref().rust().monitors.borrow_mut() = states;
                *self.as_ref().rust().ddc.borrow_mut() = Some(ddc);
                self.as_mut().rust_mut().startup_error = cxx_qt_lib::QString::default();
            }
            Err(error) => {
                self.as_mut().rust_mut().startup_error = cxx_qt_lib::QString::from(&format!("{}", error));
            }
        }
    }

    pub fn monitor_count(&self) -> i32 {
        self.rust().monitors.borrow().len() as i32
    }

    pub fn monitor_name(&self, index: i32) -> cxx_qt_lib::QString {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len())
            .map(|i| cxx_qt_lib::QString::from(&monitors[i].name))
            .unwrap_or_default()
    }

    pub fn brightness(&self, index: i32) -> i32 {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len()).map(|i| monitors[i].brightness as i32).unwrap_or(0)
    }

    pub fn set_brightness(mut self: std::pin::Pin<&mut Self>, index: i32, value: i32) {
        let value = clamp_percent(value);
        if let Some(i) = valid_index(index, self.as_ref().rust().monitors.borrow().len()) {
            self.as_ref().rust().monitors.borrow_mut()[i].brightness = value;
            if let Some(ddc) = self.as_ref().rust().ddc.borrow_mut().as_mut() {
                let _ = ddc.set_brightness_percentage(i, value);
            }
        }
    }

    pub fn contrast(&self, index: i32) -> i32 {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len()).map(|i| monitors[i].contrast as i32).unwrap_or(0)
    }

    pub fn set_contrast(mut self: std::pin::Pin<&mut Self>, index: i32, value: i32) {
        let value = clamp_percent(value);
        if let Some(i) = valid_index(index, self.as_ref().rust().monitors.borrow().len()) {
            self.as_ref().rust().monitors.borrow_mut()[i].contrast = value;
            if let Some(ddc) = self.as_ref().rust().ddc.borrow_mut().as_mut() {
                let _ = ddc.set_contrast_percentage(i, value);
            }
        }
    }

    pub fn volume(&self, index: i32) -> i32 {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len()).map(|i| monitors[i].volume as i32).unwrap_or(0)
    }

    pub fn set_volume(mut self: std::pin::Pin<&mut Self>, index: i32, value: i32) {
        let value = clamp_percent(value);
        if let Some(i) = valid_index(index, self.as_ref().rust().monitors.borrow().len()) {
            self.as_ref().rust().monitors.borrow_mut()[i].volume = value;
            if let Some(ddc) = self.as_ref().rust().ddc.borrow_mut().as_mut() {
                let _ = ddc.set_volume_percentage(i, value);
            }
        }
    }
}
```

Add these additional methods in the same `impl ffi::BrightlessController` block:

```rust
    pub fn input_source_code(&self, index: i32) -> i32 {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len()).map(|i| monitors[i].input_source_code as i32).unwrap_or(0)
    }

    pub fn set_input_source(mut self: std::pin::Pin<&mut Self>, index: i32, code: i32) {
        if let Some(i) = valid_index(index, self.as_ref().rust().monitors.borrow().len()) {
            let code = code.clamp(0, u8::MAX as i32) as u8;
            self.as_ref().rust().monitors.borrow_mut()[i].input_source_code = code;
            if let Some(ddc) = self.as_ref().rust().ddc.borrow_mut().as_mut() {
                let _ = ddc.set_input_source(i, InputSource::from_code(code));
            }
        }
    }

    pub fn power_mode_code(&self, index: i32) -> i32 {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len()).map(|i| monitors[i].power_mode_code as i32).unwrap_or(0)
    }

    pub fn set_power_mode(mut self: std::pin::Pin<&mut Self>, index: i32, code: i32) {
        if let Some(i) = valid_index(index, self.as_ref().rust().monitors.borrow().len()) {
            let code = code.clamp(0, u8::MAX as i32) as u8;
            self.as_ref().rust().monitors.borrow_mut()[i].power_mode_code = code;
            if let Some(ddc) = self.as_ref().rust().ddc.borrow_mut().as_mut() {
                let _ = ddc.set_power_mode(i, PowerMode::from_code(code));
            }
        }
    }

    pub fn supports_contrast(&self, index: i32) -> bool {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len()).map(|i| monitors[i].capabilities.supports_contrast).unwrap_or(false)
    }

    pub fn supports_volume(&self, index: i32) -> bool {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len()).map(|i| monitors[i].capabilities.supports_volume).unwrap_or(false)
    }

    pub fn supports_input_source(&self, index: i32) -> bool {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len()).map(|i| monitors[i].capabilities.supports_input_source).unwrap_or(false)
    }

    pub fn supports_power_mode(&self, index: i32) -> bool {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len()).map(|i| monitors[i].capabilities.supports_power_mode).unwrap_or(false)
    }

    pub fn scroll_step(&self) -> i32 {
        self.rust().settings.borrow().scroll_step as i32
    }

    pub fn set_scroll_step(mut self: std::pin::Pin<&mut Self>, value: i32) {
        let value = value.clamp(1, 10) as u8;
        self.as_ref().rust().settings.borrow_mut().scroll_step = value;
        let _ = self.as_ref().rust().settings.borrow().save();
    }

    pub fn dynamic_contrast_enabled(&self) -> bool {
        self.rust().settings.borrow().dynamic_contrast_enabled
    }

    pub fn set_dynamic_contrast_enabled(mut self: std::pin::Pin<&mut Self>, value: bool) {
        self.as_ref().rust().settings.borrow_mut().dynamic_contrast_enabled = value;
        let _ = self.as_ref().rust().settings.borrow().save();
        self.as_mut().refresh_dynamic_contrast_state();
    }

    pub fn dynamic_contrast_global(&self) -> bool {
        self.rust().settings.borrow().dynamic_contrast_global
    }

    pub fn set_dynamic_contrast_global(mut self: std::pin::Pin<&mut Self>, value: bool) {
        self.as_ref().rust().settings.borrow_mut().dynamic_contrast_global = value;
        let _ = self.as_ref().rust().settings.borrow().save();
        self.as_mut().refresh_dynamic_contrast_state();
    }

    pub fn dynamic_contrast_ratio(&self) -> f32 {
        self.rust().settings.borrow().dynamic_contrast_ratio
    }

    pub fn set_dynamic_contrast_ratio(mut self: std::pin::Pin<&mut Self>, value: f32) {
        self.as_ref().rust().settings.borrow_mut().dynamic_contrast_ratio = clamp_ratio(value);
        let _ = self.as_ref().rust().settings.borrow().save();
        self.as_mut().refresh_dynamic_contrast_state();
    }

    pub fn dynamic_contrast_per_monitor_ratio(&self) -> bool {
        self.rust().settings.borrow().dynamic_contrast_per_monitor_ratio
    }

    pub fn set_dynamic_contrast_per_monitor_ratio(mut self: std::pin::Pin<&mut Self>, value: bool) {
        self.as_ref().rust().settings.borrow_mut().dynamic_contrast_per_monitor_ratio = value;
        let _ = self.as_ref().rust().settings.borrow().save();
        self.as_mut().refresh_dynamic_contrast_state();
    }

    pub fn monitor_dynamic_contrast_enabled(&self, index: i32) -> bool {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len()).map(|i| monitors[i].dynamic_contrast_enabled).unwrap_or(false)
    }

    pub fn set_monitor_dynamic_contrast_enabled(mut self: std::pin::Pin<&mut Self>, index: i32, value: bool) {
        if let Some(i) = valid_index(index, self.as_ref().rust().monitors.borrow().len()) {
            let name = self.as_ref().rust().monitors.borrow()[i].name.clone();
            self.as_ref().rust().settings.borrow_mut().monitor_dynamic_contrast.insert(name, value);
            let _ = self.as_ref().rust().settings.borrow().save();
            self.as_mut().refresh_dynamic_contrast_state();
        }
    }

    pub fn monitor_ratio(&self, index: i32) -> f32 {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len()).map(|i| monitors[i].dynamic_contrast_ratio).unwrap_or(0.7)
    }

    pub fn set_monitor_ratio(mut self: std::pin::Pin<&mut Self>, index: i32, value: f32) {
        if let Some(i) = valid_index(index, self.as_ref().rust().monitors.borrow().len()) {
            let value = clamp_ratio(value);
            let name = self.as_ref().rust().monitors.borrow()[i].name.clone();
            self.as_ref().rust().settings.borrow_mut().monitor_ratios.insert(name, value);
            let _ = self.as_ref().rust().settings.borrow().save();
            self.as_mut().refresh_dynamic_contrast_state();
        }
    }

    pub fn set_dynamic_contrast_brightness(mut self: std::pin::Pin<&mut Self>, index: i32, value: i32) {
        let brightness = clamp_percent(value);
        if let Some(i) = valid_index(index, self.as_ref().rust().monitors.borrow().len()) {
            let ratio = self.as_ref().rust().monitors.borrow()[i].dynamic_contrast_ratio;
            let contrast = contrast_for_dynamic_brightness(brightness, ratio);
            {
                let mut monitors = self.as_ref().rust().monitors.borrow_mut();
                monitors[i].dynamic_contrast_brightness = brightness;
                monitors[i].brightness = brightness;
                monitors[i].contrast = contrast;
            }
            if let Some(ddc) = self.as_ref().rust().ddc.borrow_mut().as_mut() {
                let _ = ddc.set_brightness_percentage(i, brightness);
                let _ = ddc.set_contrast_percentage(i, contrast);
            }
        }
    }

    fn refresh_dynamic_contrast_state(mut self: std::pin::Pin<&mut Self>) {
        let settings = self.as_ref().rust().settings.borrow().clone();
        let mut monitors = self.as_ref().rust().monitors.borrow_mut();
        for monitor in monitors.iter_mut() {
            monitor.dynamic_contrast_enabled = dynamic_contrast_enabled_for_monitor(&settings, &monitor.name);
            monitor.dynamic_contrast_toggle_visible = settings.dynamic_contrast_enabled && !settings.dynamic_contrast_global;
            monitor.dynamic_contrast_ratio = ratio_for_monitor(&settings, &monitor.name);
        }
    }
```

- [ ] **Step 7: Run check**

Run:

```bash
cargo check
```

Expected: bridge compiles. If cxx-qt requires small signature adjustments for generated type names, keep the public QML method names and behavior from the Interfaces section unchanged.

- [ ] **Step 8: Commit**

```bash
git add src/app_state.rs src/qt_bridge.rs
git commit -m "feat: expose Brightless state to Qt"
```

---

### Task 4: Add QML UI matching the current app

**Files:**
- Create: `qml/Main.qml`
- Create: `qml/MonitorCard.qml`
- Modify: `build.rs`
- Modify: `src/main.rs`

**Interfaces:**
- Consumes: QML invokables from Task 3
- Produces: Qt/QML application UI loaded by `src/main.rs`

- [ ] **Step 1: Create monitor card QML**

Create `qml/MonitorCard.qml`:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    required property var controller
    required property int monitorIndex

    function sliderWheel(slider, wheel) {
        var step = controller.scroll_step()
        if (wheel.angleDelta.y > 0) {
            slider.value = Math.min(slider.to, slider.value + step)
        } else if (wheel.angleDelta.y < 0) {
            slider.value = Math.max(slider.from, slider.value - step)
        }
        wheel.accepted = true
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label {
            text: controller.monitor_name(root.monitorIndex)
            font.bold: true
            Layout.fillWidth: true
        }

        RowLayout {
            visible: controller.dynamic_contrast_enabled()
                && !controller.dynamic_contrast_global()
                && controller.supports_contrast(root.monitorIndex)
            Label { text: "Dynamic Contrast:"; Layout.preferredWidth: 120 }
            Switch {
                checked: controller.monitor_dynamic_contrast_enabled(root.monitorIndex)
                onToggled: controller.set_monitor_dynamic_contrast_enabled(root.monitorIndex, checked)
            }
        }

        RowLayout {
            visible: !controller.monitor_dynamic_contrast_enabled(root.monitorIndex)
            Label { text: "Brightness:"; Layout.preferredWidth: 120 }
            Slider {
                id: brightnessSlider
                from: 0
                to: 100
                stepSize: 1
                value: controller.brightness(root.monitorIndex)
                Layout.fillWidth: true
                onMoved: controller.set_brightness(root.monitorIndex, Math.round(value))
                WheelHandler { onWheel: root.sliderWheel(brightnessSlider, wheel) }
            }
            Label { text: Math.round(brightnessSlider.value) + "%"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight }
        }

        RowLayout {
            visible: controller.supports_contrast(root.monitorIndex)
                && !controller.monitor_dynamic_contrast_enabled(root.monitorIndex)
            Label { text: "Contrast:"; Layout.preferredWidth: 120 }
            Slider {
                id: contrastSlider
                from: 0
                to: 100
                stepSize: 1
                value: controller.contrast(root.monitorIndex)
                Layout.fillWidth: true
                onMoved: controller.set_contrast(root.monitorIndex, Math.round(value))
                WheelHandler { onWheel: root.sliderWheel(contrastSlider, wheel) }
            }
            Label { text: Math.round(contrastSlider.value) + "%"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight }
        }

        RowLayout {
            visible: controller.supports_contrast(root.monitorIndex)
                && controller.monitor_dynamic_contrast_enabled(root.monitorIndex)
            Label { text: "Dynamic Contrast:"; Layout.preferredWidth: 120 }
            Slider {
                id: dynamicContrastSlider
                from: 0
                to: 100
                stepSize: 1
                value: controller.brightness(root.monitorIndex)
                Layout.fillWidth: true
                onMoved: controller.set_dynamic_contrast_brightness(root.monitorIndex, Math.round(value))
                WheelHandler { onWheel: root.sliderWheel(dynamicContrastSlider, wheel) }
            }
            Label { text: Math.round(dynamicContrastSlider.value) + "%"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight }
        }

        RowLayout {
            visible: controller.supports_volume(root.monitorIndex)
            Label { text: "Volume:"; Layout.preferredWidth: 120 }
            Slider {
                id: volumeSlider
                from: 0
                to: 100
                stepSize: 1
                value: controller.volume(root.monitorIndex)
                Layout.fillWidth: true
                onMoved: controller.set_volume(root.monitorIndex, Math.round(value))
                WheelHandler { onWheel: root.sliderWheel(volumeSlider, wheel) }
            }
            Label { text: Math.round(volumeSlider.value) + "%"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight }
        }

        RowLayout {
            visible: controller.supports_input_source(root.monitorIndex) || controller.supports_power_mode(root.monitorIndex)
            Label { text: "Input:"; Layout.preferredWidth: 120; visible: controller.supports_input_source(root.monitorIndex) }
            ComboBox {
                visible: controller.supports_input_source(root.monitorIndex)
                textRole: "text"
                valueRole: "code"
                model: [
                    { text: "VGA", code: 1 },
                    { text: "DVI", code: 3 },
                    { text: "DisplayPort 1", code: 15 },
                    { text: "DisplayPort 2", code: 16 },
                    { text: "HDMI 1", code: 17 },
                    { text: "HDMI 2", code: 18 },
                    { text: "HDMI 3", code: 19 },
                    { text: "HDMI 4", code: 20 },
                    { text: "USB-C", code: 27 }
                ]
                onActivated: controller.set_input_source(root.monitorIndex, currentValue)
            }

            Label { text: "Power:"; visible: controller.supports_power_mode(root.monitorIndex) }
            ComboBox {
                visible: controller.supports_power_mode(root.monitorIndex)
                textRole: "text"
                valueRole: "code"
                model: [
                    { text: "On", code: 1 },
                    { text: "Standby", code: 2 },
                    { text: "Suspend", code: 3 },
                    { text: "Off", code: 4 },
                    { text: "Normal", code: 5 }
                ]
                onActivated: controller.set_power_mode(root.monitorIndex, currentValue)
            }
        }
    }
}
```

- [ ] **Step 2: Create main QML window**

Create `qml/Main.qml`:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import com.brightless

ApplicationWindow {
    id: window
    width: 400
    height: 300
    visible: true
    title: "Brightless"

    BrightlessController {
        id: controller
        Component.onCompleted: initialize()
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            Label {
                text: "Brightless"
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
            ToolButton {
                text: "⚙"
                onClicked: settingsPopup.open()
            }
        }
    }

    Dialog {
        id: errorDialog
        title: "Error"
        modal: true
        standardButtons: Dialog.Ok
        visible: controller.startup_error.length > 0
        Label {
            text: "Error: " + controller.startup_error
            wrapMode: Text.WordWrap
            width: 260
        }
    }

    Popup {
        id: settingsPopup
        modal: false
        focus: true
        x: Math.max(0, window.width - width - 12)
        y: 12
        width: 340
        padding: 12

        ColumnLayout {
            spacing: 12
            anchors.fill: parent

            Label { text: "Scroll Step:" }
            Label { text: scrollStepSlider.value.toFixed(0) + "%"; Layout.alignment: Qt.AlignRight }
            Slider {
                id: scrollStepSlider
                from: 1
                to: 10
                stepSize: 1
                value: controller.scroll_step()
                Layout.fillWidth: true
                onMoved: controller.set_scroll_step(Math.round(value))
            }

            Label { text: "Dynamic Contrast"; font.bold: true }

            RowLayout {
                Label { text: "Enable Dynamic Contrast"; Layout.fillWidth: true }
                Switch {
                    checked: controller.dynamic_contrast_enabled()
                    onToggled: controller.set_dynamic_contrast_enabled(checked)
                }
            }

            ColumnLayout {
                visible: controller.dynamic_contrast_enabled()
                spacing: 8

                RowLayout {
                    Label { text: "Apply to all monitors"; Layout.fillWidth: true }
                    Switch {
                        checked: controller.dynamic_contrast_global()
                        onToggled: controller.set_dynamic_contrast_global(checked)
                    }
                }

                RowLayout {
                    visible: !controller.dynamic_contrast_per_monitor_ratio()
                    Label { text: "Contrast Ratio:"; Layout.preferredWidth: 120 }
                    Label { text: ratioSlider.value.toFixed(1); Layout.fillWidth: true; horizontalAlignment: Text.AlignRight }
                }
                Slider {
                    id: ratioSlider
                    visible: !controller.dynamic_contrast_per_monitor_ratio()
                    from: 0.1
                    to: 2.0
                    stepSize: 0.1
                    value: controller.dynamic_contrast_ratio()
                    Layout.fillWidth: true
                    onMoved: controller.set_dynamic_contrast_ratio(value)
                }

                RowLayout {
                    Label { text: "Per-monitor ratio"; Layout.fillWidth: true }
                    Switch {
                        checked: controller.dynamic_contrast_per_monitor_ratio()
                        onToggled: controller.set_dynamic_contrast_per_monitor_ratio(checked)
                    }
                }

                Repeater {
                    visible: controller.dynamic_contrast_per_monitor_ratio()
                    model: controller.monitor_count()
                    ColumnLayout {
                        visible: controller.dynamic_contrast_per_monitor_ratio() && controller.supports_contrast(index)
                        RowLayout {
                            Label { text: controller.monitor_name(index) + " Ratio:"; Layout.fillWidth: true }
                            Label { text: perMonitorRatio.value.toFixed(1) }
                        }
                        Slider {
                            id: perMonitorRatio
                            from: 0.1
                            to: 2.0
                            stepSize: 0.1
                            value: controller.monitor_ratio(index)
                            Layout.fillWidth: true
                            onMoved: controller.set_monitor_ratio(index, value)
                        }
                    }
                }
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 16
        ColumnLayout {
            width: parent.width
            spacing: 12
            Repeater {
                model: controller.monitor_count()
                MonitorCard {
                    controller: controller
                    monitorIndex: index
                    Layout.fillWidth: true
                }
            }
        }
    }
}
```

- [ ] **Step 3: Update build script for QML module**

Update `build.rs` to include QML files in the build if supported by the installed `cxx-qt-build` version. Keep bridge generation as the required behavior:

```rust
fn main() {
    cxx_qt_build::CxxQtBuilder::new()
        .file("src/qt_bridge.rs")
        .build();

    println!("cargo:rerun-if-changed=qml/Main.qml");
    println!("cargo:rerun-if-changed=qml/MonitorCard.qml");
}
```

- [ ] **Step 4: Replace main with Qt/QML loading**

Replace `src/main.rs` with:

```rust
mod app_state;
mod ddc_manager;
mod qt_bridge;
mod settings;

fn main() {
    cxx_qt_lib::QGuiApplication::init(|_| {
        let mut engine = cxx_qt_lib::QQmlApplicationEngine::new();
        engine.load(cxx_qt_lib::QUrl::from("qml/Main.qml"));
        cxx_qt_lib::QGuiApplication::exec()
    });
}
```

If `cxx_qt_lib` exposes slightly different constructor names in 0.9.1, adapt only the startup calls while preserving this behavior: initialize Qt application, create QML engine, load `qml/Main.qml`, execute the app loop.

- [ ] **Step 5: Run check/build**

Run:

```bash
cargo check
cargo build
```

Expected: project builds with Qt 6 development packages installed.

- [ ] **Step 6: Commit**

```bash
git add build.rs src/main.rs qml/Main.qml qml/MonitorCard.qml
git commit -m "feat: add Qt QML interface"
```

---

### Task 5: Remove GTK UI modules and update documentation

**Files:**
- Delete: `src/window.rs`
- Delete: `src/monitor_row.rs`
- Modify: `README.md`
- Modify: `Cargo.toml`

**Interfaces:**
- Consumes: completed Qt UI from Tasks 2-4
- Produces: GTK-free project documentation and source tree

- [ ] **Step 1: Delete retired GTK UI files**

Run:

```bash
git rm src/window.rs src/monitor_row.rs
```

Expected: files are staged for deletion.

- [ ] **Step 2: Confirm Cargo has no GTK dependencies**

Run:

```bash
grep -nE 'gtk|adw|libadwaita' Cargo.toml src/*.rs README.md || true
```

Expected: matches may remain only in README text before Step 3. No matches should remain in `Cargo.toml` or active Rust source files.

- [ ] **Step 3: Update README system dependency instructions**

Replace the README system dependency section with:

```markdown
- System dependencies:
  ```bash
  # Debian/Ubuntu
  sudo apt install qt6-base-dev qt6-declarative-dev libddc-dev

  # Fedora
  sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel

  # Arch Linux
  sudo pacman -S qt6-base qt6-declarative
  ```
```

Replace the feature bullet:

```markdown
- **Modern UI** — Built with GTK4 and libadwaita
```

with:

```markdown
- **Modern UI** — Built with Qt 6 and QML
```

- [ ] **Step 4: Run final checks**

Run:

```bash
cargo test app_state --lib
cargo check
```

Expected: tests and check pass with Qt 6 development packages installed.

- [ ] **Step 5: Commit**

```bash
git add README.md Cargo.toml Cargo.lock src/window.rs src/monitor_row.rs
git commit -m "docs: update project for Qt migration"
```

---

### Task 6: Manual verification pass

**Files:**
- No source changes expected unless verification finds a defect.

**Interfaces:**
- Consumes: completed Qt application
- Produces: verified feature parity notes in the final response or defect commits if problems are found

- [ ] **Step 1: Build release binary**

Run:

```bash
cargo build --release
```

Expected: release binary builds successfully.

- [ ] **Step 2: Launch the app**

Run:

```bash
./target/release/brightless
```

Expected: Qt window titled `Brightless` appears. If no DDC monitors are available or permissions are missing, an error dialog appears with the backend error message.

- [ ] **Step 3: Verify settings persistence**

Run the app, change scroll step and Dynamic Contrast settings, close the app, then inspect:

```bash
python3 -m json.tool ~/.config/brightless/settings.json
```

Expected: JSON contains the existing fields: `scroll_step`, `dynamic_contrast_enabled`, `dynamic_contrast_global`, `dynamic_contrast_ratio`, `dynamic_contrast_per_monitor_ratio`, `monitor_dynamic_contrast`, and `monitor_ratios`.

- [ ] **Step 4: Verify controls with available monitors**

Use the UI to test:

```text
Brightness slider changes brightness and label.
Contrast slider changes contrast and label when supported.
Volume slider changes volume and label when supported.
Input dropdown sends selected source code when supported.
Power dropdown sends selected power mode code when supported.
Mouse wheel over sliders changes values by the configured scroll step.
Dynamic Contrast slider sets brightness and computed contrast.
Global/per-monitor Dynamic Contrast switches change visible controls as in the GTK version.
```

Expected: behavior matches the previous GTK/libadwaita application.

- [ ] **Step 5: Commit verification fixes if needed**

If a defect is fixed during verification, commit it:

```bash
git add src/qt_bridge.rs qml/Main.qml qml/MonitorCard.qml README.md Cargo.toml Cargo.lock
git commit -m "fix: complete Qt migration verification"
```

If no defects are found, do not create an empty commit.
