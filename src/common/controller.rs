use super::{
    clamp_percent, clamp_ratio, contrast_for_dynamic_brightness,
    dynamic_contrast_enabled_for_monitor, ratio_for_monitor, AppSettings, DdcError, DdcManager,
    InputSource, MonitorCapabilities, MonitorState, PowerMode,
};

pub(crate) struct CommonController {
    ddc: Option<DdcManager>,
    settings: AppSettings,
    monitors: Vec<MonitorState>,
}

impl CommonController {
    pub(crate) fn new() -> Self {
        Self {
            ddc: None,
            settings: AppSettings::load(),
            monitors: Vec::new(),
        }
    }

    pub(crate) fn initialize(&mut self) -> Result<(), DdcError> {
        let mut ddc = DdcManager::new()?;
        let mut states = Vec::with_capacity(ddc.monitors.len());

        for index in 0..ddc.monitors.len() {
            let (
                name,
                min_brightness,
                max_brightness,
                min_contrast,
                max_contrast,
                min_volume,
                max_volume,
                supports_input_source,
                supports_power_mode,
            ) = {
                let monitor = &ddc.monitors[index];
                (
                    monitor.name.clone(),
                    monitor.min_brightness,
                    monitor.max_brightness,
                    monitor.min_contrast,
                    monitor.max_contrast,
                    monitor.min_volume,
                    monitor.max_volume,
                    monitor.supports_input_source,
                    monitor.supports_power_mode,
                )
            };

            let brightness = ddc.get_brightness_percentage(index).unwrap_or(50);
            let contrast = ddc.get_contrast_percentage(index).unwrap_or(50);
            let volume = ddc.get_volume_percentage(index).unwrap_or(50);
            let input_source_code = ddc
                .get_input_source(index)
                .map(|value| value.code())
                .unwrap_or(0);
            let power_mode_code = ddc
                .get_power_mode(index)
                .map(|value| value.code())
                .unwrap_or(0);
            let dynamic_contrast_enabled =
                dynamic_contrast_enabled_for_monitor(&self.settings, &name);
            let dynamic_contrast_ratio = ratio_for_monitor(&self.settings, &name);

            states.push(MonitorState {
                name,
                min_brightness,
                max_brightness,
                min_contrast,
                max_contrast,
                min_volume,
                max_volume,
                brightness,
                contrast,
                volume,
                input_source_code,
                power_mode_code,
                dynamic_contrast_brightness: brightness,
                dynamic_contrast_enabled,
                dynamic_contrast_toggle_visible: self.settings.dynamic_contrast_enabled
                    && !self.settings.dynamic_contrast_global,
                dynamic_contrast_ratio,
                capabilities: MonitorCapabilities {
                    supports_contrast: max_contrast > 0,
                    supports_volume: max_volume > 0,
                    supports_input_source,
                    supports_power_mode,
                },
            });
        }

        self.monitors = states;
        self.ddc = Some(ddc);
        Ok(())
    }

    pub(crate) fn monitors(&self) -> &[MonitorState] {
        &self.monitors
    }

    pub(crate) fn monitor(&self, index: usize) -> Option<&MonitorState> {
        self.monitors.get(index)
    }

    pub(crate) fn set_brightness(&mut self, index: usize, value: i32) -> bool {
        let value = clamp_percent(value);
        let Some(monitor) = self.monitors.get_mut(index) else {
            return false;
        };
        monitor.brightness = value;
        if let Some(ddc) = self.ddc.as_mut() {
            let _ = ddc.set_brightness_percentage(index, value);
        }
        true
    }

    pub(crate) fn set_contrast(&mut self, index: usize, value: i32) -> bool {
        let value = clamp_percent(value);
        let Some(monitor) = self.monitors.get_mut(index) else {
            return false;
        };
        monitor.contrast = value;
        if let Some(ddc) = self.ddc.as_mut() {
            let _ = ddc.set_contrast_percentage(index, value);
        }
        true
    }

    pub(crate) fn set_volume(&mut self, index: usize, value: i32) -> bool {
        let value = clamp_percent(value);
        let Some(monitor) = self.monitors.get_mut(index) else {
            return false;
        };
        monitor.volume = value;
        if let Some(ddc) = self.ddc.as_mut() {
            let _ = ddc.set_volume_percentage(index, value);
        }
        true
    }

    pub(crate) fn set_input_source(&mut self, index: usize, code: i32) -> bool {
        let code = code.clamp(0, u8::MAX as i32) as u8;
        let Some(monitor) = self.monitors.get_mut(index) else {
            return false;
        };
        monitor.input_source_code = code;
        if let Some(ddc) = self.ddc.as_mut() {
            let _ = ddc.set_input_source(index, InputSource::from_code(code));
        }
        true
    }

    pub(crate) fn set_power_mode(&mut self, index: usize, code: i32) -> bool {
        let code = code.clamp(0, u8::MAX as i32) as u8;
        let Some(monitor) = self.monitors.get_mut(index) else {
            return false;
        };
        monitor.power_mode_code = code;
        if let Some(ddc) = self.ddc.as_mut() {
            let _ = ddc.set_power_mode(index, PowerMode::from_code(code));
        }
        true
    }

    pub(crate) fn set_dynamic_contrast_brightness(&mut self, index: usize, value: i32) -> bool {
        let brightness = clamp_percent(value);
        let Some(monitor) = self.monitors.get_mut(index) else {
            return false;
        };
        let contrast = contrast_for_dynamic_brightness(brightness, monitor.dynamic_contrast_ratio);
        monitor.dynamic_contrast_brightness = brightness;
        monitor.brightness = brightness;
        monitor.contrast = contrast;

        if let Some(ddc) = self.ddc.as_mut() {
            let _ = ddc.set_brightness_percentage(index, brightness);
            let _ = ddc.set_contrast_percentage(index, contrast);
        }
        true
    }

    pub(crate) fn scroll_step(&self) -> u8 {
        self.settings.scroll_step
    }

    pub(crate) fn set_scroll_step(&mut self, value: i32) {
        self.settings.scroll_step = value.clamp(1, 10) as u8;
        self.save_settings();
    }

    pub(crate) fn dynamic_contrast_enabled(&self) -> bool {
        self.settings.dynamic_contrast_enabled
    }

    pub(crate) fn set_dynamic_contrast_enabled(&mut self, value: bool) {
        self.settings.dynamic_contrast_enabled = value;
        self.save_settings();
        self.refresh_dynamic_contrast_state();
    }

    pub(crate) fn dynamic_contrast_global(&self) -> bool {
        self.settings.dynamic_contrast_global
    }

    pub(crate) fn set_dynamic_contrast_global(&mut self, value: bool) {
        self.settings.dynamic_contrast_global = value;
        self.save_settings();
        self.refresh_dynamic_contrast_state();
    }

    pub(crate) fn dynamic_contrast_ratio(&self) -> f32 {
        self.settings.dynamic_contrast_ratio
    }

    pub(crate) fn set_dynamic_contrast_ratio(&mut self, value: f32) {
        self.settings.dynamic_contrast_ratio = clamp_ratio(value);
        self.save_settings();
        self.refresh_dynamic_contrast_state();
    }

    pub(crate) fn dynamic_contrast_per_monitor_ratio(&self) -> bool {
        self.settings.dynamic_contrast_per_monitor_ratio
    }

    pub(crate) fn set_dynamic_contrast_per_monitor_ratio(&mut self, value: bool) {
        self.settings.dynamic_contrast_per_monitor_ratio = value;
        self.save_settings();
        self.refresh_dynamic_contrast_state();
    }

    pub(crate) fn set_monitor_dynamic_contrast_enabled(
        &mut self,
        index: usize,
        value: bool,
    ) -> bool {
        let Some(name) = self.monitors.get(index).map(|monitor| monitor.name.clone()) else {
            return false;
        };
        self.settings.monitor_dynamic_contrast.insert(name, value);
        self.save_settings();
        self.refresh_dynamic_contrast_state();
        true
    }

    pub(crate) fn set_monitor_ratio(&mut self, index: usize, value: f32) -> bool {
        let Some(name) = self.monitors.get(index).map(|monitor| monitor.name.clone()) else {
            return false;
        };
        self.settings
            .monitor_ratios
            .insert(name, clamp_ratio(value));
        self.save_settings();
        self.refresh_dynamic_contrast_state();
        true
    }

    fn refresh_dynamic_contrast_state(&mut self) {
        for monitor in &mut self.monitors {
            monitor.dynamic_contrast_enabled =
                dynamic_contrast_enabled_for_monitor(&self.settings, &monitor.name);
            monitor.dynamic_contrast_toggle_visible =
                self.settings.dynamic_contrast_enabled && !self.settings.dynamic_contrast_global;
            monitor.dynamic_contrast_ratio = ratio_for_monitor(&self.settings, &monitor.name);
        }
    }

    fn save_settings(&self) {
        let _ = self.settings.save();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn monitor() -> MonitorState {
        MonitorState {
            name: "Test".into(),
            min_brightness: 0,
            max_brightness: 100,
            min_contrast: 0,
            max_contrast: 100,
            min_volume: 0,
            max_volume: 100,
            brightness: 50,
            contrast: 50,
            volume: 50,
            input_source_code: 17,
            power_mode_code: 1,
            dynamic_contrast_brightness: 50,
            dynamic_contrast_enabled: false,
            dynamic_contrast_toggle_visible: false,
            dynamic_contrast_ratio: 0.7,
            capabilities: MonitorCapabilities {
                supports_contrast: true,
                supports_volume: true,
                supports_input_source: true,
                supports_power_mode: true,
            },
        }
    }

    fn controller() -> CommonController {
        CommonController {
            ddc: None,
            settings: AppSettings::default(),
            monitors: vec![monitor()],
        }
    }

    #[test]
    fn monitor_commands_clamp_values_and_reject_invalid_indexes() {
        let mut controller = controller();

        assert!(controller.set_brightness(0, -10));
        assert!(controller.set_contrast(0, 120));
        assert!(controller.set_volume(0, 42));
        assert!(!controller.set_brightness(1, 50));

        let state = controller.monitor(0).unwrap();
        assert_eq!(state.brightness, 0);
        assert_eq!(state.contrast, 100);
        assert_eq!(state.volume, 42);
    }

    #[test]
    fn dynamic_contrast_command_updates_brightness_and_derived_contrast() {
        let mut controller = controller();
        controller.monitors[0].dynamic_contrast_ratio = 0.7;

        assert!(controller.set_dynamic_contrast_brightness(0, 80));

        let state = controller.monitor(0).unwrap();
        assert_eq!(state.dynamic_contrast_brightness, 80);
        assert_eq!(state.brightness, 80);
        assert_eq!(state.contrast, 56);
    }

    #[test]
    fn refreshing_dynamic_contrast_uses_shared_settings_rules() {
        let mut controller = controller();
        controller.settings.dynamic_contrast_enabled = true;
        controller.settings.dynamic_contrast_global = false;
        controller.settings.dynamic_contrast_per_monitor_ratio = true;
        controller
            .settings
            .monitor_dynamic_contrast
            .insert("Test".into(), true);
        controller
            .settings
            .monitor_ratios
            .insert("Test".into(), 1.4);

        controller.refresh_dynamic_contrast_state();

        let state = controller.monitor(0).unwrap();
        assert!(state.dynamic_contrast_enabled);
        assert!(state.dynamic_contrast_toggle_visible);
        assert_eq!(state.dynamic_contrast_ratio, 1.4);
    }
}
