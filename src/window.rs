use crate::ddc_manager::{DdcError, DdcManager};
use crate::monitor_row::MonitorRow;
use crate::settings::AppSettings;
use adw::prelude::*;
use adw::{Application, ApplicationWindow, HeaderBar, ToolbarView, ViewStack, ViewSwitcher};
use glib::Propagation;
use gtk::{
    Box, Button, EventControllerScroll, EventControllerScrollFlags, Label, ListBox, Orientation,
    Popover, Scale, ScrolledWindow, SelectionMode,
};
use std::cell::RefCell;
use std::rc::Rc;

pub struct MainWindow {
    pub window: ApplicationWindow,
    pub stack: ViewStack,
    pub monitor_rows: Vec<MonitorRow>,
    ddc: Rc<RefCell<DdcManager>>,
    settings: Rc<RefCell<AppSettings>>,
}

impl MainWindow {
    pub fn new(app: &Application) -> Result<Self, DdcError> {
        let ddc = DdcManager::new()?;
        let monitor_count = ddc.monitors.len();
        let settings = Rc::new(RefCell::new(AppSettings::load()));
        let scroll_step = settings.borrow().scroll_step;

        let window = ApplicationWindow::builder()
            .application(app)
            .title("Brightless")
            .default_width(400)
            .default_height(300)
            .build();

        let toolbar_view = ToolbarView::new();
        let header_bar = HeaderBar::new();
        toolbar_view.add_top_bar(&header_bar);

        let stack = ViewStack::new();
        stack.set_vexpand(true);

        let view_switcher = ViewSwitcher::builder().stack(&stack).build();

        header_bar.set_title_widget(Some(&view_switcher));
        header_bar.set_show_start_title_buttons(true);
        header_bar.set_show_end_title_buttons(true);

        let settings_button = Button::builder()
            .icon_name("emblem-system-symbolic")
            .tooltip_text("Settings")
            .build();

        let popover = Popover::new();
        popover.set_autohide(true);
        popover.set_parent(&settings_button);

        let settings_inner = settings.clone();

        let popover_box = Box::new(Orientation::Vertical, 12);
        popover_box.set_margin_top(12);
        popover_box.set_margin_end(12);
        popover_box.set_margin_bottom(12);
        popover_box.set_margin_start(12);

        let scroll_step_label = Label::new(Some("Scroll Step:"));
        scroll_step_label.set_halign(gtk::Align::Start);
        popover_box.append(&scroll_step_label);

        let scroll_step_value_label = Label::new(Some(&format!("{}%", scroll_step)));
        scroll_step_value_label.set_halign(gtk::Align::End);
        scroll_step_value_label.set_hexpand(true);
        popover_box.append(&scroll_step_value_label);

        let scroll_step_scale = Scale::builder()
            .orientation(Orientation::Horizontal)
            .hexpand(true)
            .build();
        scroll_step_scale.set_range(1.0, 10.0);
        scroll_step_scale.set_digits(0);
        scroll_step_scale.set_draw_value(false);
        scroll_step_scale.set_value(scroll_step as f64);

        let scroll_step_value_label_inner = scroll_step_value_label.clone();
        let adjustment = scroll_step_scale.adjustment();
        adjustment.connect_value_changed(move |adj| {
            let val = adj.value() as u8;
            scroll_step_value_label_inner.set_text(&format!("{}%", val));
            settings_inner.borrow_mut().scroll_step = val;
            let _ = settings_inner.borrow().save();
        });

        let scroll_step_scale_inner = scroll_step_scale.clone();
        let scroll_step_label_scroll = scroll_step_value_label.clone();
        let settings_scroll = settings.clone();
        let scroll_controller = EventControllerScroll::new(EventControllerScrollFlags::VERTICAL);
        scroll_controller.connect_scroll(move |_, _dx, dy| {
            let current = scroll_step_scale_inner.value();
            let step = 2.0;
            let new_value = if dy < 0.0 {
                (current + step).min(10.0)
            } else {
                (current - step).max(1.0)
            };
            scroll_step_scale_inner.set_value(new_value);
            let val = new_value as u8;
            scroll_step_label_scroll.set_text(&format!("{}%", val));
            settings_scroll.borrow_mut().scroll_step = val;
            let _ = settings_scroll.borrow().save();
            Propagation::Stop
        });
        scroll_step_scale.add_controller(scroll_controller);

        popover_box.append(&scroll_step_scale);

        popover.set_child(Some(&popover_box));

        settings_button.connect_clicked(move |_| {
            popover.popup();
        });

        header_bar.pack_end(&settings_button);

        let mut monitor_rows = Vec::new();
        let ddc_ref = Rc::new(RefCell::new(ddc));

        for i in 0..monitor_count {
            let (
                name,
                min_brightness,
                max_brightness,
                min_contrast,
                max_contrast,
                min_volume,
                max_volume,
                supports_input_source,
                supports_power_mode,
            ) = {
                let ddc = ddc_ref.borrow();
                (
                    ddc.monitors[i].name.clone(),
                    ddc.monitors[i].min_brightness,
                    ddc.monitors[i].max_brightness,
                    ddc.monitors[i].min_contrast,
                    ddc.monitors[i].max_contrast,
                    ddc.monitors[i].min_volume,
                    ddc.monitors[i].max_volume,
                    ddc.monitors[i].supports_input_source,
                    ddc.monitors[i].supports_power_mode,
                )
            };

            let row = MonitorRow::new(
                name,
                min_brightness,
                max_brightness,
                min_contrast,
                max_contrast,
                min_volume,
                max_volume,
                supports_input_source,
                supports_power_mode,
                scroll_step,
            );

            let ddc_clone = ddc_ref.clone();
            let idx = i;
            row.connect_brightness_changed(move |value| {
                if let Ok(mut ddc) = ddc_clone.try_borrow_mut() {
                    let _ = ddc.set_brightness_percentage(idx, value);
                }
            });

            let ddc_clone2 = ddc_ref.clone();
            let idx2 = i;
            if row.has_contrast() {
                row.connect_contrast_changed(move |value| {
                    if let Ok(mut ddc) = ddc_clone2.try_borrow_mut() {
                        let _ = ddc.set_contrast_percentage(idx2, value);
                    }
                });
            }

            let ddc_clone3 = ddc_ref.clone();
            let idx3 = i;
            if row.has_volume() {
                row.connect_volume_changed(move |value| {
                    if let Ok(mut ddc) = ddc_clone3.try_borrow_mut() {
                        let _ = ddc.set_volume_percentage(idx3, value);
                    }
                });
            }

            let ddc_clone4 = ddc_ref.clone();
            let idx4 = i;
            if row.has_input_source() {
                row.connect_input_source_changed(move |value| {
                    use crate::ddc_manager::InputSource;
                    if let Ok(mut ddc) = ddc_clone4.try_borrow_mut() {
                        let _ = ddc.set_input_source(idx4, InputSource::from_code(value));
                    }
                });
            }

            let ddc_clone5 = ddc_ref.clone();
            let idx5 = i;
            if row.has_power_mode() {
                row.connect_power_mode_changed(move |value| {
                    use crate::ddc_manager::PowerMode;
                    if let Ok(mut ddc) = ddc_clone5.try_borrow_mut() {
                        let _ = ddc.set_power_mode(idx5, PowerMode::from_code(value));
                    }
                });
            }

            monitor_rows.push(row);
        }

        let content = Box::new(Orientation::Vertical, 0);
        content.append(&toolbar_view);

        let list = ListBox::builder()
            .margin_top(16)
            .margin_end(16)
            .margin_bottom(16)
            .margin_start(16)
            .selection_mode(SelectionMode::None)
            .css_classes(vec![String::from("boxed-list")])
            .build();

        for row in &monitor_rows {
            list.append(&row.container);
        }

        let scrolled = ScrolledWindow::new();
        scrolled.set_child(Some(&list));
        scrolled.set_vexpand(true);

        toolbar_view.set_content(Some(&scrolled));

        window.set_content(Some(&content));

        Ok(Self {
            window,
            stack,
            monitor_rows,
            ddc: ddc_ref,
            settings,
        })
    }

    pub fn init_brightness(&self) {
        let mut ddc = self.ddc.borrow_mut();
        for (i, row) in self.monitor_rows.iter().enumerate() {
            match ddc.get_brightness_percentage(i) {
                Ok(percentage) => {
                    row.set_brightness(percentage);
                }
                Err(_) => {}
            }

            if row.has_contrast() {
                match ddc.get_contrast_percentage(i) {
                    Ok(percentage) => {
                        row.set_contrast(percentage);
                    }
                    Err(_) => {}
                }
            }

            if row.has_volume() {
                match ddc.get_volume_percentage(i) {
                    Ok(percentage) => {
                        row.set_volume(percentage);
                    }
                    Err(_) => {}
                }
            }

            if row.has_input_source() {
                match ddc.get_input_source(i) {
                    Ok(source) => {
                        row.set_input_source(source.code());
                    }
                    Err(_) => {}
                }
            }

            if row.has_power_mode() {
                match ddc.get_power_mode(i) {
                    Ok(mode) => {
                        row.set_power_mode(mode.code());
                    }
                    Err(_) => {}
                }
            }
        }
    }
}
