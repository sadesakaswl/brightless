use crate::common::{valid_index, CommonController};
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
        fn set_dynamic_contrast_per_monitor_ratio(
            self: Pin<&mut BrightlessController>,
            value: bool,
        );
        #[qinvokable]
        fn monitor_dynamic_contrast_enabled(self: &BrightlessController, index: i32) -> bool;
        #[qinvokable]
        fn set_monitor_dynamic_contrast_enabled(
            self: Pin<&mut BrightlessController>,
            index: i32,
            value: bool,
        );
        #[qinvokable]
        fn monitor_ratio(self: &BrightlessController, index: i32) -> f32;
        #[qinvokable]
        fn set_monitor_ratio(self: Pin<&mut BrightlessController>, index: i32, value: f32);
        #[qinvokable]
        fn set_dynamic_contrast_brightness(
            self: Pin<&mut BrightlessController>,
            index: i32,
            value: i32,
        );
    }
}

pub struct BrightlessControllerRust {
    startup_error: cxx_qt_lib::QString,
    monitor_names: cxx_qt_lib::QStringList,
    monitor_count: i32,
    revision: i32,
    controller: RefCell<CommonController>,
}

impl Default for BrightlessControllerRust {
    fn default() -> Self {
        Self {
            startup_error: cxx_qt_lib::QString::default(),
            monitor_names: cxx_qt_lib::QStringList::default(),
            monitor_count: 0,
            revision: 0,
            controller: RefCell::new(CommonController::new()),
        }
    }
}

impl ffi::BrightlessController {
    pub fn initialize(mut self: Pin<&mut Self>) {
        let result = self.as_ref().rust().controller.borrow_mut().initialize();
        match result {
            Ok(()) => {
                let mut monitor_names = cxx_qt_lib::QStringList::default();
                let monitor_count = {
                    let this = self.as_ref();
                    let controller = this.rust().controller.borrow();
                    for monitor in controller.monitors() {
                        monitor_names.append(cxx_qt_lib::QString::from(monitor.name.as_str()));
                    }
                    controller.monitors().len() as i32
                };
                self.as_mut().rust_mut().monitor_names = monitor_names;
                self.as_mut().monitor_names_changed();
                self.as_mut().rust_mut().monitor_count = monitor_count;
                self.as_mut().monitor_count_changed();
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
        let controller = self.rust().controller.borrow();
        valid_index(index, controller.monitors().len())
            .and_then(|index| controller.monitor(index))
            .map(|monitor| monitor.brightness as i32)
            .unwrap_or(0)
    }

    pub fn set_brightness(mut self: Pin<&mut Self>, index: i32, value: i32) {
        let updated = {
            let this = self.as_ref();
            let mut controller = this.rust().controller.borrow_mut();
            valid_index(index, controller.monitors().len())
                .is_some_and(|index| controller.set_brightness(index, value))
        };
        if updated {
            self.as_mut().bump_revision();
        }
    }

    pub fn contrast(&self, index: i32) -> i32 {
        let controller = self.rust().controller.borrow();
        valid_index(index, controller.monitors().len())
            .and_then(|index| controller.monitor(index))
            .map(|monitor| monitor.contrast as i32)
            .unwrap_or(0)
    }

    pub fn set_contrast(mut self: Pin<&mut Self>, index: i32, value: i32) {
        let updated = {
            let this = self.as_ref();
            let mut controller = this.rust().controller.borrow_mut();
            valid_index(index, controller.monitors().len())
                .is_some_and(|index| controller.set_contrast(index, value))
        };
        if updated {
            self.as_mut().bump_revision();
        }
    }

    pub fn volume(&self, index: i32) -> i32 {
        let controller = self.rust().controller.borrow();
        valid_index(index, controller.monitors().len())
            .and_then(|index| controller.monitor(index))
            .map(|monitor| monitor.volume as i32)
            .unwrap_or(0)
    }

    pub fn set_volume(mut self: Pin<&mut Self>, index: i32, value: i32) {
        let updated = {
            let this = self.as_ref();
            let mut controller = this.rust().controller.borrow_mut();
            valid_index(index, controller.monitors().len())
                .is_some_and(|index| controller.set_volume(index, value))
        };
        if updated {
            self.as_mut().bump_revision();
        }
    }

    pub fn input_source_code(&self, index: i32) -> i32 {
        let controller = self.rust().controller.borrow();
        valid_index(index, controller.monitors().len())
            .and_then(|index| controller.monitor(index))
            .map(|monitor| monitor.input_source_code as i32)
            .unwrap_or(0)
    }

    pub fn set_input_source(mut self: Pin<&mut Self>, index: i32, code: i32) {
        let updated = {
            let this = self.as_ref();
            let mut controller = this.rust().controller.borrow_mut();
            valid_index(index, controller.monitors().len())
                .is_some_and(|index| controller.set_input_source(index, code))
        };
        if updated {
            self.as_mut().bump_revision();
        }
    }

    pub fn power_mode_code(&self, index: i32) -> i32 {
        let controller = self.rust().controller.borrow();
        valid_index(index, controller.monitors().len())
            .and_then(|index| controller.monitor(index))
            .map(|monitor| monitor.power_mode_code as i32)
            .unwrap_or(0)
    }

    pub fn set_power_mode(mut self: Pin<&mut Self>, index: i32, code: i32) {
        let updated = {
            let this = self.as_ref();
            let mut controller = this.rust().controller.borrow_mut();
            valid_index(index, controller.monitors().len())
                .is_some_and(|index| controller.set_power_mode(index, code))
        };
        if updated {
            self.as_mut().bump_revision();
        }
    }

    pub fn supports_contrast(&self, index: i32) -> bool {
        let controller = self.rust().controller.borrow();
        valid_index(index, controller.monitors().len())
            .and_then(|index| controller.monitor(index))
            .map(|monitor| monitor.capabilities.supports_contrast)
            .unwrap_or(false)
    }

    pub fn supports_volume(&self, index: i32) -> bool {
        let controller = self.rust().controller.borrow();
        valid_index(index, controller.monitors().len())
            .and_then(|index| controller.monitor(index))
            .map(|monitor| monitor.capabilities.supports_volume)
            .unwrap_or(false)
    }

    pub fn supports_input_source(&self, index: i32) -> bool {
        let controller = self.rust().controller.borrow();
        valid_index(index, controller.monitors().len())
            .and_then(|index| controller.monitor(index))
            .map(|monitor| monitor.capabilities.supports_input_source)
            .unwrap_or(false)
    }

    pub fn supports_power_mode(&self, index: i32) -> bool {
        let controller = self.rust().controller.borrow();
        valid_index(index, controller.monitors().len())
            .and_then(|index| controller.monitor(index))
            .map(|monitor| monitor.capabilities.supports_power_mode)
            .unwrap_or(false)
    }

    pub fn scroll_step(&self) -> i32 {
        self.rust().controller.borrow().scroll_step() as i32
    }

    pub fn set_scroll_step(mut self: Pin<&mut Self>, value: i32) {
        self.as_ref()
            .rust()
            .controller
            .borrow_mut()
            .set_scroll_step(value);
        self.as_mut().bump_revision();
    }

    pub fn dynamic_contrast_enabled(&self) -> bool {
        self.rust().controller.borrow().dynamic_contrast_enabled()
    }

    pub fn set_dynamic_contrast_enabled(mut self: Pin<&mut Self>, value: bool) {
        self.as_ref()
            .rust()
            .controller
            .borrow_mut()
            .set_dynamic_contrast_enabled(value);
        self.as_mut().bump_revision();
    }

    pub fn dynamic_contrast_global(&self) -> bool {
        self.rust().controller.borrow().dynamic_contrast_global()
    }

    pub fn set_dynamic_contrast_global(mut self: Pin<&mut Self>, value: bool) {
        self.as_ref()
            .rust()
            .controller
            .borrow_mut()
            .set_dynamic_contrast_global(value);
        self.as_mut().bump_revision();
    }

    pub fn dynamic_contrast_ratio(&self) -> f32 {
        self.rust().controller.borrow().dynamic_contrast_ratio()
    }

    pub fn set_dynamic_contrast_ratio(mut self: Pin<&mut Self>, value: f32) {
        self.as_ref()
            .rust()
            .controller
            .borrow_mut()
            .set_dynamic_contrast_ratio(value);
        self.as_mut().bump_revision();
    }

    pub fn dynamic_contrast_per_monitor_ratio(&self) -> bool {
        self.rust()
            .controller
            .borrow()
            .dynamic_contrast_per_monitor_ratio()
    }

    pub fn set_dynamic_contrast_per_monitor_ratio(mut self: Pin<&mut Self>, value: bool) {
        self.as_ref()
            .rust()
            .controller
            .borrow_mut()
            .set_dynamic_contrast_per_monitor_ratio(value);
        self.as_mut().bump_revision();
    }

    pub fn monitor_dynamic_contrast_enabled(&self, index: i32) -> bool {
        let controller = self.rust().controller.borrow();
        valid_index(index, controller.monitors().len())
            .and_then(|index| controller.monitor(index))
            .map(|monitor| monitor.dynamic_contrast_enabled)
            .unwrap_or(false)
    }

    pub fn set_monitor_dynamic_contrast_enabled(mut self: Pin<&mut Self>, index: i32, value: bool) {
        let updated = {
            let this = self.as_ref();
            let mut controller = this.rust().controller.borrow_mut();
            valid_index(index, controller.monitors().len())
                .is_some_and(|index| controller.set_monitor_dynamic_contrast_enabled(index, value))
        };
        if updated {
            self.as_mut().bump_revision();
        }
    }

    pub fn monitor_ratio(&self, index: i32) -> f32 {
        let controller = self.rust().controller.borrow();
        valid_index(index, controller.monitors().len())
            .and_then(|index| controller.monitor(index))
            .map(|monitor| monitor.dynamic_contrast_ratio)
            .unwrap_or(0.7)
    }

    pub fn set_monitor_ratio(mut self: Pin<&mut Self>, index: i32, value: f32) {
        let updated = {
            let this = self.as_ref();
            let mut controller = this.rust().controller.borrow_mut();
            valid_index(index, controller.monitors().len())
                .is_some_and(|index| controller.set_monitor_ratio(index, value))
        };
        if updated {
            self.as_mut().bump_revision();
        }
    }

    pub fn set_dynamic_contrast_brightness(mut self: Pin<&mut Self>, index: i32, value: i32) {
        let updated = {
            let this = self.as_ref();
            let mut controller = this.rust().controller.borrow_mut();
            valid_index(index, controller.monitors().len())
                .is_some_and(|index| controller.set_dynamic_contrast_brightness(index, value))
        };
        if updated {
            self.as_mut().bump_revision();
        }
    }

    fn bump_revision(mut self: Pin<&mut Self>) {
        let revision = self.as_ref().rust().revision.saturating_add(1);
        self.as_mut().rust_mut().revision = revision;
        self.revision_changed();
    }
}
