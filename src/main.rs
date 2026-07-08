mod app_state;
mod ddc_manager;
mod qt_bridge;
mod settings;

use cxx_qt_lib::{QGuiApplication, QQmlApplicationEngine, QString, QUrl};

fn main() {
    let mut app = QGuiApplication::new();
    app.pin_mut()
        .set_application_name(&QString::from("Brightless"));
    app.pin_mut()
        .set_application_display_name(&QString::from("Brightless"));

    let mut engine = QQmlApplicationEngine::new();
    engine
        .pin_mut()
        .load(&QUrl::from("qrc:/qt/qml/com/brightless/Main.qml"));

    app.pin_mut().exec();
}
