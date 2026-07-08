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
            !MONITOR_CARD_QML.contains("slider.moved()"),
            "scrolling must not rely on manually emitting Slider.moved(), which does not reliably call the setter"
        );
    }

    #[test]
    fn input_and_power_combos_bind_initial_selection_to_backend_codes() {
        assert!(
            MONITOR_CARD_QML.contains("currentIndex: indexOfValue(controller.input_source_code(root.monitorIndex))"),
            "input ComboBox must show the current backend input source code"
        );
        assert!(
            MONITOR_CARD_QML.contains("currentIndex: indexOfValue(controller.power_mode_code(root.monitorIndex))"),
            "power ComboBox must show the current backend power mode code"
        );
    }
}
