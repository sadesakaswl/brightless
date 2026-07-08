#[cfg(test)]
mod tests {
    const CARGO_TOML: &str = include_str!("../Cargo.toml");

    #[test]
    fn release_profile_enables_o3_lto_and_single_codegen_unit() {
        assert!(CARGO_TOML.contains("[profile.release]"), "release profile must be explicit");
        assert!(CARGO_TOML.contains("opt-level = 3"), "release profile must use O3 optimization");
        assert!(CARGO_TOML.contains("lto = true"), "release profile must enable full/fat LTO");
        assert!(CARGO_TOML.contains("codegen-units = 1"), "release profile must use one codegen unit for better optimization");
    }
}
