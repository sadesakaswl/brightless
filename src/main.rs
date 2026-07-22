#![cfg_attr(feature = "gtk", deny(deprecated))]

mod common;
#[cfg(test)]
mod config_tests;
#[cfg(all(feature = "gtk", not(feature = "qt")))]
mod monitor_row;
#[cfg(all(test, feature = "qt", not(feature = "gtk")))]
mod qml_tests;
#[cfg(all(feature = "qt", not(feature = "gtk")))]
mod qt_bridge;
#[cfg(all(feature = "gtk", not(feature = "qt")))]
mod window;

#[cfg(all(feature = "qt", not(feature = "gtk")))]
fn main() {
    use cxx_qt_lib::{QGuiApplication, QQmlApplicationEngine, QString, QUrl};

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

#[cfg(all(feature = "gtk", not(feature = "qt")))]
fn main() {
    use crate::window::MainWindow;
    use adw::prelude::*;
    use adw::Application;

    let application = Application::builder()
        .application_id("com.brightless.app")
        .build();

    application.connect_activate(move |app| match MainWindow::new(app) {
        Ok(window) => {
            window.init_brightness();
            window.window.present();
            std::mem::forget(window);
        }
        Err(error) => {
            eprintln!("Failed to initialize: {error}");
            let window = adw::ApplicationWindow::new(app);
            window.set_title(Some("Error"));
            window.set_default_size(300, 100);

            let label = gtk::Label::new(Some(&format!("Error: {error}")));
            label.set_margin_start(20);
            label.set_margin_end(20);
            label.set_margin_top(20);
            label.set_margin_bottom(20);
            window.set_content(Some(&label));
            window.present();
        }
    });

    application.run();
}

#[cfg(not(any(
    all(feature = "qt", not(feature = "gtk")),
    all(feature = "gtk", not(feature = "qt"))
)))]
fn main() {}
