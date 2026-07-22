#[cfg(not(any(feature = "qt", feature = "gtk")))]
compile_error!("enable exactly one UI feature: `qt` or `gtk`");

#[cfg(all(feature = "qt", feature = "gtk"))]
compile_error!("features `qt` and `gtk` are mutually exclusive");

#[cfg(all(feature = "qt", not(feature = "gtk")))]
use cxx_qt_build::{CxxQtBuilder, QmlModule};

#[cfg(all(feature = "qt", not(feature = "gtk")))]
fn main() {
    CxxQtBuilder::new_qml_module(QmlModule::new("com.brightless"))
        .file("src/qt/bridge.rs")
        .qrc("src/qt/qml.qrc")
        .qt_module("Gui")
        .qt_module("Quick")
        .qt_module("QuickControls2")
        .build();

    println!("cargo:rerun-if-changed=src/qt/qml/Main.qml");
    println!("cargo:rerun-if-changed=src/qt/qml/MonitorCard.qml");
}

#[cfg(not(any(
    all(feature = "qt", not(feature = "gtk")),
    all(feature = "gtk", not(feature = "qt"))
)))]
fn main() {}

#[cfg(all(feature = "gtk", not(feature = "qt")))]
fn main() {}
