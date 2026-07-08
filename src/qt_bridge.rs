#[cxx_qt::bridge]
mod ffi {
    unsafe extern "C++" {
        type QString = cxx_qt_lib::QString;
    }

    extern "RustQt" {
        #[qobject]
        #[qml_element]
        #[qproperty(QString, startup_error, READ, NOTIFY)]
        type BrightlessController = super::BrightlessControllerRust;
    }
}

#[derive(Default)]
pub struct BrightlessControllerRust {
    startup_error: cxx_qt_lib::QString,
}
