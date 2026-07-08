use cxx_qt_build::{CxxQtBuilder, QmlModule};

fn main() {
    CxxQtBuilder::new_qml_module(
        QmlModule::new("com.brightless").qml_files(["qml/Main.qml", "qml/MonitorCard.qml"]),
    )
    .file("src/qt_bridge.rs")
    .qt_module("Gui")
    .qt_module("Quick")
    .qt_module("QuickControls2")
    .build();

    println!("cargo:rerun-if-changed=qml/Main.qml");
    println!("cargo:rerun-if-changed=qml/MonitorCard.qml");
}
