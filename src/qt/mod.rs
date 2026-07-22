mod bridge;
#[cfg(test)]
mod tests;

use cxx_qt_lib::{QGuiApplication, QQmlApplicationEngine, QString, QUrl};

pub(crate) fn run() {
    cxx_qt::init_crate!(brightless);

    let mut app = QGuiApplication::new();
    app.pin_mut()
        .set_application_name(&QString::from("Brightless"));
    app.pin_mut()
        .set_application_display_name(&QString::from("Brightless"));

    let mut engine = QQmlApplicationEngine::new();
    engine
        .pin_mut()
        .load(&QUrl::from("qrc:/qt/qml/com/brightless/qml/Main.qml"));

    app.pin_mut().exec();
}
