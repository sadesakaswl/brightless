#[cfg(test)]
mod tests {
    const MAIN_QML: &str = include_str!("../qml/Main.qml");
    const MONITOR_CARD_QML: &str = include_str!("../qml/MonitorCard.qml");

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
    fn main_window_has_adaptive_space_for_controls() {
        assert!(MAIN_QML.contains("width: 720"), "default window width should fit monitor controls");
        assert!(MAIN_QML.contains("height: 560"), "default window height should show useful content");
        assert!(MAIN_QML.contains("minimumWidth: 560"), "window should not shrink below usable control width");
        assert!(MAIN_QML.contains("minimumHeight: 420"), "window should not shrink below usable control height");
    }
}
