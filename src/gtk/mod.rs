mod monitor_row;
mod window;

use self::window::MainWindow;
use adw::prelude::*;
use adw::Application;

pub(crate) fn run() {
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
