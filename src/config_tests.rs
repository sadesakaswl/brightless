#[cfg(test)]
mod tests {
    const BUILD_RS: &str = include_str!("../build.rs");
    const CARGO_TOML: &str = include_str!("../Cargo.toml");

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
    fn invalid_frontend_selections_have_targeted_compile_errors() {
        assert!(BUILD_RS.contains("enable exactly one UI feature: `qt` or `gtk`"));
        assert!(BUILD_RS.contains("features `qt` and `gtk` are mutually exclusive"));
    }

    #[test]
    fn qt_build_steps_are_gated_away_from_gtk_builds() {
        const MAIN_RS: &str = include_str!("main.rs");

        assert!(BUILD_RS.contains("all(feature = \"qt\", not(feature = \"gtk\"))"));
        assert!(MAIN_RS.contains("mod window;"));
        assert!(MAIN_RS.contains("mod monitor_row;"));
        assert!(MAIN_RS.contains("cxx_qt::init_crate!(brightless);"));
    }
}
