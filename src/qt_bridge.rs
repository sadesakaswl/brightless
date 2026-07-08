use crate::app_state::{
    clamp_percent, clamp_ratio, contrast_for_dynamic_brightness,
    dynamic_contrast_enabled_for_monitor, ratio_for_monitor, valid_index, MonitorCapabilities,
    MonitorUiState,
};
use crate::ddc_manager::{DdcManager, InputSource, PowerMode};
use crate::settings::AppSettings;
use cxx_qt::CxxQtType;
use std::cell::RefCell;
use std::pin::Pin;

#[cxx_qt::bridge]
mod ffi {
    unsafe extern "C++" {
        type QString = cxx_qt_lib::QString;
        type QStringList = cxx_qt_lib::QStringList;
    }

    extern "RustQt" {
        #[qobject]
        #[qml_element]
        #[qproperty(QString, startup_error, READ, NOTIFY)]
        #[qproperty(QStringList, monitor_names, READ, NOTIFY)]
        #[qproperty(i32, monitor_count, READ, NOTIFY)]
        #[qproperty(i32, revision)]
        type BrightlessController = super::BrightlessControllerRust;

        #[qinvokable]
        fn initialize(self: Pin<&mut BrightlessController>);
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
    monitor_names: cxx_qt_lib::QStringList,
    monitor_count: i32,
    revision: i32,
    ddc: RefCell<Option<DdcManager>>,
    settings: RefCell<AppSettings>,
    monitors: RefCell<Vec<MonitorUiState>>,
}

impl Default for BrightlessControllerRust {
    fn default() -> Self {
        Self {
            startup_error: cxx_qt_lib::QString::default(),
            monitor_names: cxx_qt_lib::QStringList::default(),
            monitor_count: 0,
            revision: 0,
            ddc: RefCell::new(None),
            settings: RefCell::new(AppSettings::load()),
            monitors: RefCell::new(Vec::new()),
        }
    }
}

impl ffi::BrightlessController {
    pub fn initialize(mut self: Pin<&mut Self>) {
        let settings = self.as_ref().rust().settings.borrow().clone();
        match DdcManager::new() {
            Ok(mut ddc) => {
                let mut states = Vec::new();
                let mut monitor_names = cxx_qt_lib::QStringList::default();
                for i in 0..ddc.monitors.len() {
                    let (
                        name,
                        supports_contrast,
                        supports_volume,
                        supports_input_source,
                        supports_power_mode,
                    ) = {
                        let monitor = &ddc.monitors[i];
                        (
                            monitor.name.clone(),
                            monitor.max_contrast > 0,
                            monitor.max_volume > 0,
                            monitor.supports_input_source,
                            monitor.supports_power_mode,
                        )
                    };
                    let brightness = ddc.get_brightness_percentage(i).unwrap_or(50);
                    let contrast = ddc.get_contrast_percentage(i).unwrap_or(50);
                    let volume = ddc.get_volume_percentage(i).unwrap_or(50);
                    let input_source_code = ddc.get_input_source(i).map(|s| s.code()).unwrap_or(0);
                    let power_mode_code = ddc.get_power_mode(i).map(|m| m.code()).unwrap_or(0);
                    let dc_enabled = dynamic_contrast_enabled_for_monitor(&settings, &name);
                    let ratio = ratio_for_monitor(&settings, &name);
                    monitor_names.append(cxx_qt_lib::QString::from(name.as_str()));
                    states.push(MonitorUiState {
                        name,
                        brightness,
                        contrast,
                        volume,
                        input_source_code,
                        power_mode_code,
                        dynamic_contrast_brightness: brightness,
                        dynamic_contrast_enabled: dc_enabled,
                        dynamic_contrast_toggle_visible: settings.dynamic_contrast_enabled
                            && !settings.dynamic_contrast_global,
                        dynamic_contrast_ratio: ratio,
                        capabilities: MonitorCapabilities {
                            supports_contrast,
                            supports_volume,
                            supports_input_source,
                            supports_power_mode,
                        },
                    });
                }
                let monitor_count = states.len() as i32;
                *self.as_ref().rust().monitors.borrow_mut() = states;
                self.as_mut().rust_mut().monitor_names = monitor_names;
                self.as_mut().monitor_names_changed();
                self.as_mut().rust_mut().monitor_count = monitor_count;
                self.as_mut().monitor_count_changed();
                *self.as_ref().rust().ddc.borrow_mut() = Some(ddc);
                self.as_mut().rust_mut().startup_error = cxx_qt_lib::QString::default();
                self.as_mut().bump_revision();
            }
            Err(error) => {
                self.as_mut().rust_mut().startup_error =
                    cxx_qt_lib::QString::from(format!("{}", error).as_str());
                self.as_mut().startup_error_changed();
                self.as_mut().rust_mut().monitor_count = 0;
                self.as_mut().monitor_count_changed();
                self.as_mut().bump_revision();
            }
        }
    }

    pub fn brightness(&self, index: i32) -> i32 {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len())
            .map(|i| monitors[i].brightness as i32)
            .unwrap_or(0)
    }

    pub fn set_brightness(mut self: Pin<&mut Self>, index: i32, value: i32) {
        let value = clamp_percent(value);
        let monitor_len = { self.as_ref().rust().monitors.borrow().len() };
        if let Some(i) = valid_index(index, monitor_len) {
            self.as_ref().rust().monitors.borrow_mut()[i].brightness = value;
            if let Some(ddc) = self.as_ref().rust().ddc.borrow_mut().as_mut() {
                let _ = ddc.set_brightness_percentage(i, value);
            }
            self.as_mut().bump_revision();
        }
    }

    pub fn contrast(&self, index: i32) -> i32 {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len())
            .map(|i| monitors[i].contrast as i32)
            .unwrap_or(0)
    }

    pub fn set_contrast(mut self: Pin<&mut Self>, index: i32, value: i32) {
        let value = clamp_percent(value);
        let monitor_len = { self.as_ref().rust().monitors.borrow().len() };
        if let Some(i) = valid_index(index, monitor_len) {
            self.as_ref().rust().monitors.borrow_mut()[i].contrast = value;
            if let Some(ddc) = self.as_ref().rust().ddc.borrow_mut().as_mut() {
                let _ = ddc.set_contrast_percentage(i, value);
            }
            self.as_mut().bump_revision();
        }
    }

    pub fn volume(&self, index: i32) -> i32 {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len())
            .map(|i| monitors[i].volume as i32)
            .unwrap_or(0)
    }

    pub fn set_volume(mut self: Pin<&mut Self>, index: i32, value: i32) {
        let value = clamp_percent(value);
        let monitor_len = { self.as_ref().rust().monitors.borrow().len() };
        if let Some(i) = valid_index(index, monitor_len) {
            self.as_ref().rust().monitors.borrow_mut()[i].volume = value;
            if let Some(ddc) = self.as_ref().rust().ddc.borrow_mut().as_mut() {
                let _ = ddc.set_volume_percentage(i, value);
            }
            self.as_mut().bump_revision();
        }
    }

    pub fn input_source_code(&self, index: i32) -> i32 {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len())
            .map(|i| monitors[i].input_source_code as i32)
            .unwrap_or(0)
    }

    pub fn set_input_source(mut self: Pin<&mut Self>, index: i32, code: i32) {
        let monitor_len = { self.as_ref().rust().monitors.borrow().len() };
        if let Some(i) = valid_index(index, monitor_len) {
            let code = code.clamp(0, u8::MAX as i32) as u8;
            self.as_ref().rust().monitors.borrow_mut()[i].input_source_code = code;
            if let Some(ddc) = self.as_ref().rust().ddc.borrow_mut().as_mut() {
                let _ = ddc.set_input_source(i, InputSource::from_code(code));
            }
            self.as_mut().bump_revision();
        }
    }

    pub fn power_mode_code(&self, index: i32) -> i32 {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len())
            .map(|i| monitors[i].power_mode_code as i32)
            .unwrap_or(0)
    }

    pub fn set_power_mode(mut self: Pin<&mut Self>, index: i32, code: i32) {
        let monitor_len = { self.as_ref().rust().monitors.borrow().len() };
        if let Some(i) = valid_index(index, monitor_len) {
            let code = code.clamp(0, u8::MAX as i32) as u8;
            self.as_ref().rust().monitors.borrow_mut()[i].power_mode_code = code;
            if let Some(ddc) = self.as_ref().rust().ddc.borrow_mut().as_mut() {
                let _ = ddc.set_power_mode(i, PowerMode::from_code(code));
            }
            self.as_mut().bump_revision();
        }
    }

    pub fn supports_contrast(&self, index: i32) -> bool {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len())
            .map(|i| monitors[i].capabilities.supports_contrast)
            .unwrap_or(false)
    }

    pub fn supports_volume(&self, index: i32) -> bool {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len())
            .map(|i| monitors[i].capabilities.supports_volume)
            .unwrap_or(false)
    }

    pub fn supports_input_source(&self, index: i32) -> bool {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len())
            .map(|i| monitors[i].capabilities.supports_input_source)
            .unwrap_or(false)
    }

    pub fn supports_power_mode(&self, index: i32) -> bool {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len())
            .map(|i| monitors[i].capabilities.supports_power_mode)
            .unwrap_or(false)
    }

    pub fn scroll_step(&self) -> i32 {
        self.rust().settings.borrow().scroll_step as i32
    }

    pub fn set_scroll_step(mut self: Pin<&mut Self>, value: i32) {
        let value = value.clamp(1, 10) as u8;
        self.as_ref().rust().settings.borrow_mut().scroll_step = value;
        let _ = self.as_ref().rust().settings.borrow().save();
        self.as_mut().bump_revision();
    }

    pub fn dynamic_contrast_enabled(&self) -> bool {
        self.rust().settings.borrow().dynamic_contrast_enabled
    }

    pub fn set_dynamic_contrast_enabled(mut self: Pin<&mut Self>, value: bool) {
        self.as_ref().rust().settings.borrow_mut().dynamic_contrast_enabled = value;
        let _ = self.as_ref().rust().settings.borrow().save();
        self.as_mut().refresh_dynamic_contrast_state();
        self.as_mut().bump_revision();
    }

    pub fn dynamic_contrast_global(&self) -> bool {
        self.rust().settings.borrow().dynamic_contrast_global
    }

    pub fn set_dynamic_contrast_global(mut self: Pin<&mut Self>, value: bool) {
        self.as_ref().rust().settings.borrow_mut().dynamic_contrast_global = value;
        let _ = self.as_ref().rust().settings.borrow().save();
        self.as_mut().refresh_dynamic_contrast_state();
        self.as_mut().bump_revision();
    }

    pub fn dynamic_contrast_ratio(&self) -> f32 {
        self.rust().settings.borrow().dynamic_contrast_ratio
    }

    pub fn set_dynamic_contrast_ratio(mut self: Pin<&mut Self>, value: f32) {
        self.as_ref().rust().settings.borrow_mut().dynamic_contrast_ratio = clamp_ratio(value);
        let _ = self.as_ref().rust().settings.borrow().save();
        self.as_mut().refresh_dynamic_contrast_state();
        self.as_mut().bump_revision();
    }

    pub fn dynamic_contrast_per_monitor_ratio(&self) -> bool {
        self.rust().settings.borrow().dynamic_contrast_per_monitor_ratio
    }

    pub fn set_dynamic_contrast_per_monitor_ratio(mut self: Pin<&mut Self>, value: bool) {
        self.as_ref()
            .rust()
            .settings
            .borrow_mut()
            .dynamic_contrast_per_monitor_ratio = value;
        let _ = self.as_ref().rust().settings.borrow().save();
        self.as_mut().refresh_dynamic_contrast_state();
        self.as_mut().bump_revision();
    }

    pub fn monitor_dynamic_contrast_enabled(&self, index: i32) -> bool {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len())
            .map(|i| monitors[i].dynamic_contrast_enabled)
            .unwrap_or(false)
    }

    pub fn set_monitor_dynamic_contrast_enabled(
        mut self: Pin<&mut Self>,
        index: i32,
        value: bool,
    ) {
        let monitor_len = { self.as_ref().rust().monitors.borrow().len() };
        if let Some(i) = valid_index(index, monitor_len) {
            let name = self.as_ref().rust().monitors.borrow()[i].name.clone();
            self.as_ref()
                .rust()
                .settings
                .borrow_mut()
                .monitor_dynamic_contrast
                .insert(name, value);
            let _ = self.as_ref().rust().settings.borrow().save();
            self.as_mut().refresh_dynamic_contrast_state();
            self.as_mut().bump_revision();
        }
    }

    pub fn monitor_ratio(&self, index: i32) -> f32 {
        let monitors = self.rust().monitors.borrow();
        valid_index(index, monitors.len())
            .map(|i| monitors[i].dynamic_contrast_ratio)
            .unwrap_or(0.7)
    }

    pub fn set_monitor_ratio(mut self: Pin<&mut Self>, index: i32, value: f32) {
        let monitor_len = { self.as_ref().rust().monitors.borrow().len() };
        if let Some(i) = valid_index(index, monitor_len) {
            let value = clamp_ratio(value);
            let name = self.as_ref().rust().monitors.borrow()[i].name.clone();
            self.as_ref()
                .rust()
                .settings
                .borrow_mut()
                .monitor_ratios
                .insert(name, value);
            let _ = self.as_ref().rust().settings.borrow().save();
            self.as_mut().refresh_dynamic_contrast_state();
            self.as_mut().bump_revision();
        }
    }

    pub fn set_dynamic_contrast_brightness(mut self: Pin<&mut Self>, index: i32, value: i32) {
        let brightness = clamp_percent(value);
        let monitor_len = { self.as_ref().rust().monitors.borrow().len() };
        if let Some(i) = valid_index(index, monitor_len) {
            let ratio = self.as_ref().rust().monitors.borrow()[i].dynamic_contrast_ratio;
            let contrast = contrast_for_dynamic_brightness(brightness, ratio);
            {
                let this = self.as_ref();
                let mut monitors = this.rust().monitors.borrow_mut();
                monitors[i].dynamic_contrast_brightness = brightness;
                monitors[i].brightness = brightness;
                monitors[i].contrast = contrast;
            }
            if let Some(ddc) = self.as_ref().rust().ddc.borrow_mut().as_mut() {
                let _ = ddc.set_brightness_percentage(i, brightness);
                let _ = ddc.set_contrast_percentage(i, contrast);
            }
            self.as_mut().bump_revision();
        }
    }

    fn refresh_dynamic_contrast_state(self: Pin<&mut Self>) {
        let settings = self.as_ref().rust().settings.borrow().clone();
        let this = self.as_ref();
        let mut monitors = this.rust().monitors.borrow_mut();
        for monitor in monitors.iter_mut() {
            monitor.dynamic_contrast_enabled =
                dynamic_contrast_enabled_for_monitor(&settings, &monitor.name);
            monitor.dynamic_contrast_toggle_visible =
                settings.dynamic_contrast_enabled && !settings.dynamic_contrast_global;
            monitor.dynamic_contrast_ratio = ratio_for_monitor(&settings, &monitor.name);
        }
    }

    fn bump_revision(mut self: Pin<&mut Self>) {
        let revision = self.as_ref().rust().revision.saturating_add(1);
        self.as_mut().rust_mut().revision = revision;
        self.revision_changed();
    }
}
