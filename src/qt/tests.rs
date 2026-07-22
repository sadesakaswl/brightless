#[cfg(test)]
mod tests {
    const MAIN_QML: &str = include_str!("qml/Main.qml");
    const MONITOR_CARD_QML: &str = include_str!("qml/MonitorCard.qml");

    #[test]
    fn monitor_repeaters_bind_to_reactive_monitor_count_property() {
        assert!(
            MAIN_QML.contains("model: controller.monitor_count"),
            "monitor repeaters must bind to the monitor_count property so they update after initialize()"
        );
        assert!(
            !MAIN_QML.contains("model: controller.monitor_count()"),
            "monitor repeaters must not call monitor_count() as an invokable because QML cannot observe that value changing"
        );
    }

    #[test]
    fn slider_wheel_handlers_call_backend_setters_directly() {
        assert!(
            MONITOR_CARD_QML.contains("function sliderWheel(slider, wheel, applyValue)"),
            "slider wheel helper must accept a backend update callback"
        );
        assert!(
            MONITOR_CARD_QML.contains("applyValue(Math.round(slider.value))"),
            "scrolling sliders must update the DDC backend directly instead of only moving the local slider"
        );
        assert!(
            MONITOR_CARD_QML.matches("MouseArea {").count() >= 4,
            "each slider must have a MouseArea wheel overlay so ScrollView does not steal wheel events"
        );
        assert!(
            !MONITOR_CARD_QML.contains("WheelHandler"),
            "slider wheel handling must not use WheelHandler because it is unreliable inside the ScrollView"
        );
        assert!(
            !MONITOR_CARD_QML.contains("slider.moved()"),
            "scrolling must not rely on manually emitting Slider.moved(), which does not reliably call the setter"
        );
    }

    #[test]
    fn input_and_power_combos_have_fallback_current_values() {
        assert!(
            MONITOR_CARD_QML.contains("function choicesWithCurrent(choices, code)"),
            "ComboBox models must include a fallback entry for unknown/current DDC codes"
        );
        assert!(
            MONITOR_CARD_QML.contains("Current ("),
            "unknown non-zero DDC input/power codes must be displayed instead of leaving the ComboBox blank"
        );
        assert!(
            MONITOR_CARD_QML.contains("Unknown"),
            "failed DDC reads with code 0 must display Unknown instead of leaving the ComboBox blank"
        );
        assert!(
            !MONITOR_CARD_QML.contains("currentIndex: indexOfValue("),
            "ComboBox initial selection must not depend on indexOfValue returning a known hardcoded code"
        );
    }

    #[test]
    fn main_window_sizes_from_content_instead_of_fixed_dimensions() {
        assert!(
            MAIN_QML.contains("id: monitorColumn"),
            "main content column must expose implicit size for adaptive window sizing"
        );
        assert!(
            MAIN_QML.contains("width: Math.min(Screen.desktopAvailableWidth"),
            "window width should be derived from content and capped to screen width"
        );
        assert!(
            MAIN_QML.contains("height: Math.min(Screen.desktopAvailableHeight"),
            "window height should be derived from content and capped to screen height"
        );
        assert!(
            !MAIN_QML.contains("width: 720"),
            "window width must not be a fixed large default"
        );
        assert!(
            !MAIN_QML.contains("height: 560"),
            "window height must not be a fixed large default"
        );
    }

    #[test]
    fn main_window_contains_settings_controls() {
        for expected in [
            "id: settingsPopup",
            "Scroll Step:",
            "controller.set_scroll_step",
            "Dynamic Contrast",
            "controller.set_dynamic_contrast_enabled",
            "Apply to all monitors",
            "controller.set_dynamic_contrast_global",
            "Contrast Ratio:",
            "controller.set_dynamic_contrast_ratio",
            "Per-monitor ratio",
            "controller.set_dynamic_contrast_per_monitor_ratio",
            "controller.set_monitor_ratio",
        ] {
            assert!(
                MAIN_QML.contains(expected),
                "Main.qml missing expected settings control: {expected}"
            );
        }
    }

    #[test]
    fn monitor_card_contains_all_monitor_controls() {
        for expected in [
            "Brightness:",
            "controller.set_brightness",
            "Contrast:",
            "controller.set_contrast",
            "Dynamic Contrast:",
            "controller.set_dynamic_contrast_brightness",
            "Volume:",
            "controller.set_volume",
            "Input:",
            "controller.set_input_source",
            "Power:",
            "controller.set_power_mode",
        ] {
            assert!(
                MONITOR_CARD_QML.contains(expected),
                "MonitorCard.qml missing expected monitor control: {expected}"
            );
        }
    }

    #[test]
    fn monitor_card_preserves_input_and_power_option_sets() {
        for expected in [
            "VGA",
            "DVI",
            "DisplayPort 1",
            "DisplayPort 2",
            "HDMI 1",
            "HDMI 2",
            "HDMI 3",
            "HDMI 4",
            "USB-C",
            "On",
            "Standby",
            "Suspend",
            "Off",
            "Normal",
        ] {
            assert!(
                MONITOR_CARD_QML.contains(expected),
                "MonitorCard.qml missing option: {expected}"
            );
        }
    }
}
