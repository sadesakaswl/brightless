fn main() {
    cxx_qt_build::CxxQtBuilder::new()
        .file("src/qt_bridge.rs")
        .build();
}
