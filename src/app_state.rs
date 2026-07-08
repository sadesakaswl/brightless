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
}
