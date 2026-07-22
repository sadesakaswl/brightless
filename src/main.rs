#![cfg_attr(feature = "gtk", deny(deprecated))]

mod common;
#[cfg(test)]
mod config_tests;
#[cfg(all(feature = "gtk", not(feature = "qt")))]
#[path = "gtk/mod.rs"]
mod frontend;
#[cfg(all(feature = "qt", not(feature = "gtk")))]
#[path = "qt/mod.rs"]
mod frontend;

#[cfg(any(
    all(feature = "qt", not(feature = "gtk")),
    all(feature = "gtk", not(feature = "qt"))
))]
fn main() {
    frontend::run();
}

#[cfg(not(any(
    all(feature = "qt", not(feature = "gtk")),
    all(feature = "gtk", not(feature = "qt"))
)))]
fn main() {}

#[cfg(not(any(feature = "qt", feature = "gtk")))]
compile_error!("enable exactly one UI feature: `qt` or `gtk`");

#[cfg(all(feature = "qt", feature = "gtk"))]
compile_error!("features `qt` and `gtk` are mutually exclusive");
