#[cfg(test)]
mod tests {
    const MAIN_QML: &str = include_str!("../qml/Main.qml");

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
}
