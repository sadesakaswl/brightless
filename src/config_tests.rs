#[cfg(test)]
mod tests {
    use std::path::Path;

    const BUILD_RS: &str = include_str!("../build.rs");
    const CARGO_TOML: &str = include_str!("../Cargo.toml");
    const MAIN_RS: &str = include_str!("main.rs");
    const MONITOR_ROW_RS: &str = include_str!("gtk/monitor_row.rs");
    const GTK_MOD_RS: &str = include_str!("gtk/mod.rs");
    const QT_MOD_RS: &str = include_str!("qt/mod.rs");
    const QT_TESTS_RS: &str = include_str!("qt/tests.rs");
    const QT_QRC: &str = include_str!("qt/qml.qrc");
    const README: &str = include_str!("../README.md");

    #[test]
    fn release_profile_enables_o3_lto_and_single_codegen_unit() {
        assert!(
            CARGO_TOML.contains("[profile.release]"),
            "release profile must be explicit"
        );
        assert!(
            CARGO_TOML.contains("opt-level = 3"),
            "release profile must use O3 optimization"
        );
        assert!(
            CARGO_TOML.contains("lto = true"),
            "release profile must enable full/fat LTO"
        );
        assert!(
            CARGO_TOML.contains("codegen-units = 1"),
            "release profile must use one codegen unit for better optimization"
        );
    }

    #[test]
    fn ui_features_are_explicit_optional_and_have_no_default() {
        for expected in [
            "cxx = { version = \"1\", optional = true }",
            "cxx-qt = { version = \"0.9.1\", optional = true }",
            "gtk = { package = \"gtk4\", version = \"0.11\", features = [\"v4_14\"], optional = true }",
            "adw = { package = \"libadwaita\", version = \"0.9\", features = [\"v1_8\"], optional = true }",
            "glib = { version = \"0.22\", optional = true }",
            "cxx-qt-build = { version = \"0.9.1\", optional = true }",
            "default = []",
            "qt = [\"dep:cxx\", \"dep:cxx-qt\", \"dep:cxx-qt-lib\", \"dep:cxx-qt-build\"]",
            "gtk = [\"dep:gtk\", \"dep:adw\", \"dep:glib\"]",
        ] {
            assert!(CARGO_TOML.contains(expected), "Cargo.toml missing feature contract: {expected}");
        }
    }

    #[test]
    fn gtk_feature_platform_requirements_are_documented() {
        assert!(
            CARGO_TOML.contains("rust-version = \"1.92\""),
            "Cargo.toml must declare the Rust version required by the selected GTK stack"
        );

        for expected in ["Rust 1.92", "GTK 4.14", "libadwaita 1.8"] {
            assert!(
                README.contains(expected),
                "README missing GTK platform requirement: {expected}"
            );
        }
    }

    #[test]
    fn invalid_frontend_selections_have_targeted_compile_errors() {
        assert!(BUILD_RS.contains("enable exactly one UI feature: `qt` or `gtk`"));
        assert!(BUILD_RS.contains("features `qt` and `gtk` are mutually exclusive"));
    }

    #[test]
    fn gtk_error_window_uses_adwaita_content_api() {
        assert!(GTK_MOD_RS.contains("window.set_content(Some(&label));"));
        assert!(!GTK_MOD_RS.contains("window.set_child(Some(&label));"));
    }

    #[test]
    fn qt_build_steps_are_gated_away_from_gtk_builds() {
        assert!(BUILD_RS.contains("all(feature = \"qt\", not(feature = \"gtk\"))"));
        assert!(MAIN_RS.contains("#[path = \"qt/mod.rs\"]"));
        assert!(MAIN_RS.contains("#[path = \"gtk/mod.rs\"]"));
        assert!(MAIN_RS.contains("frontend::run();"));
        assert!(QT_MOD_RS.contains("pub(crate) fn run()"));
        assert!(GTK_MOD_RS.contains("pub(crate) fn run()"));
        assert!(QT_TESTS_RS.contains("qml/Main.qml"));
        assert!(QT_QRC.contains("prefix=\"/qt/qml/com/brightless/qml\""));
        assert!(QT_QRC.contains("alias=\"Main.qml\">qml/Main.qml"));
        assert!(QT_QRC.contains("alias=\"MonitorCard.qml\">qml/MonitorCard.qml"));
    }

    #[test]
    fn gtk_frontend_enforces_and_uses_non_deprecated_selector_apis() {
        assert!(MAIN_RS.contains("#![cfg_attr(feature = \"gtk\", deny(deprecated))]"));
        assert!(MONITOR_ROW_RS.contains("DropDown"));

        for deprecated in [
            "ComboBoxText",
            "set_active_id",
            "active_id",
            "connect_changed",
        ] {
            assert!(
                !MONITOR_ROW_RS.contains(deprecated),
                "GTK monitor row still uses deprecated API: {deprecated}"
            );
        }
    }

    #[test]
    fn readme_documents_both_explicit_frontends() {
        for expected in [
            "cargo build --release --features qt",
            "cargo build --release --features gtk",
            "qt6-base-dev qt6-declarative-dev",
            "libadwaita-1-dev",
            "qt6-qtbase-devel qt6-qtdeclarative-devel",
            "libadwaita-devel",
            "qt6-base qt6-declarative",
            "gtk4 libadwaita",
            "There is no default frontend",
        ] {
            assert!(
                README.contains(expected),
                "README missing frontend documentation: {expected}"
            );
        }
    }

    #[test]
    fn toolkit_independent_sources_live_under_common() {
        for path in [
            "src/common/mod.rs",
            "src/common/model.rs",
            "src/common/ddc_manager.rs",
            "src/common/settings.rs",
        ] {
            assert!(Path::new(path).is_file(), "missing common source: {path}");
        }

        for path in ["src/app_state.rs", "src/ddc_manager.rs", "src/settings.rs"] {
            assert!(
                !Path::new(path).exists(),
                "legacy root source remains: {path}"
            );
        }
    }

    #[test]
    fn frontends_live_in_separate_modules_and_main_only_delegates() {
        for path in [
            "src/qt/mod.rs",
            "src/qt/bridge.rs",
            "src/qt/tests.rs",
            "src/qt/qml.qrc",
            "src/qt/qml/Main.qml",
            "src/qt/qml/MonitorCard.qml",
            "src/gtk/mod.rs",
            "src/gtk/window.rs",
            "src/gtk/monitor_row.rs",
        ] {
            assert!(Path::new(path).is_file(), "missing frontend source: {path}");
        }

        assert!(MAIN_RS.contains("#[path = \"qt/mod.rs\"]"));
        assert!(MAIN_RS.contains("#[path = \"gtk/mod.rs\"]"));
        assert!(MAIN_RS.contains("frontend::run();"));
        assert!(!MAIN_RS.contains("QGuiApplication::new"));
        assert!(!MAIN_RS.contains("Application::builder"));

        assert!(BUILD_RS.contains("src/qt/bridge.rs"));
        assert!(BUILD_RS.contains("src/qt/qml/Main.qml"));
    }
}
