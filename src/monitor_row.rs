use adw::prelude::*;
use adw::ActionRow;
use glib::Propagation;
use gtk::{
    Box, ComboBoxText, EventControllerScroll, EventControllerScrollFlags, Label, Orientation, Scale,
};
use std::cell::RefCell;
use std::rc::Rc;

pub struct MonitorRow {
    pub container: ActionRow,
    pub brightness_scale: Scale,
    pub brightness_label: Label,
    pub contrast_scale: Option<Scale>,
    pub contrast_label: Option<Label>,
    pub volume_scale: Option<Scale>,
    pub volume_label: Option<Label>,
    pub input_source_combo: Option<ComboBoxText>,
    pub power_mode_combo: Option<ComboBoxText>,
    brightness_label_inner: Rc<RefCell<Label>>,
    contrast_label_inner: Option<Rc<RefCell<Label>>>,
    volume_label_inner: Option<Rc<RefCell<Label>>>,
}

impl MonitorRow {
    pub fn new(
        name: String,
        min_brightness: u16,
        max_brightness: u16,
        _min_contrast: u16,
        max_contrast: u16,
        _min_volume: u16,
        max_volume: u16,
        supports_input_source: bool,
        supports_power_mode: bool,
        scroll_step: u8,
    ) -> Self {
        let brightness_scale = Scale::builder()
            .orientation(Orientation::Horizontal)
            .hexpand(true)
            .build();
        brightness_scale.set_range(0.0, 100.0);
        brightness_scale.set_digits(0);
        brightness_scale.set_draw_value(false);

        let brightness_label = Label::new(Some("50%"));
        brightness_label.set_width_chars(5);
        brightness_label.set_halign(gtk::Align::End);

        let brightness_label_inner = Rc::new(RefCell::new(brightness_label.clone()));

        // Add scroll controller for brightness slider
        let brightness_label_scroll = brightness_label_inner.clone();
        let brightness_scale_scroll = brightness_scale.clone();
        let brightness_scroll_controller =
            EventControllerScroll::new(EventControllerScrollFlags::VERTICAL);
        brightness_scroll_controller.connect_scroll(move |_, _dx, dy| {
            let current = brightness_scale_scroll.value();
            let step = scroll_step as f64;
            let new_value = if dy < 0.0 {
                (current + step).min(100.0)
            } else {
                (current - step).max(0.0)
            };
            brightness_scale_scroll.set_value(new_value);
            brightness_label_scroll
                .borrow()
                .set_text(&format!("{}%", new_value as u8));
            Propagation::Proceed
        });
        brightness_scale.add_controller(brightness_scroll_controller);

        let brightness_row = Box::new(Orientation::Horizontal, 8);
        let brightness_label_text = Label::new(Some("Brightness:"));
        brightness_label_text.set_width_chars(12);
        brightness_row.append(&brightness_label_text);
        brightness_row.append(&brightness_scale);
        brightness_row.append(&brightness_label);
        brightness_row.set_margin_top(8);

        let (contrast_scale, contrast_label, contrast_label_inner) = if max_contrast > 0 {
            let scale = Scale::builder()
                .orientation(Orientation::Horizontal)
                .hexpand(true)
                .build();
            scale.set_range(0.0, 100.0);
            scale.set_digits(0);
            scale.set_draw_value(false);

            let label = Label::new(Some("50%"));
            label.set_width_chars(5);
            label.set_halign(gtk::Align::End);

            // Add scroll controller for contrast slider
            let contrast_label_scroll = Rc::new(RefCell::new(label.clone()));
            let contrast_scale_scroll = scale.clone();
            let contrast_scroll_controller =
                EventControllerScroll::new(EventControllerScrollFlags::VERTICAL);
            contrast_scroll_controller.connect_scroll(move |_, _dx, dy| {
                let current = contrast_scale_scroll.value();
                let step = scroll_step as f64;
                let new_value = if dy < 0.0 {
                    (current + step).min(100.0)
                } else {
                    (current - step).max(0.0)
                };
                contrast_scale_scroll.set_value(new_value);
                contrast_label_scroll
                    .borrow()
                    .set_text(&format!("{}%", new_value as u8));
                Propagation::Proceed
            });
            scale.add_controller(contrast_scroll_controller);

            let label_inner = Rc::new(RefCell::new(label.clone()));

            (Some(scale), Some(label), Some(label_inner))
        } else {
            (None, None, None)
        };

        let main_box = Box::new(Orientation::Vertical, 0);
        main_box.append(&brightness_row);

        if let (Some(c_scale), Some(c_label)) = (&contrast_scale, &contrast_label) {
            let contrast_row = Box::new(Orientation::Horizontal, 8);
            let contrast_label_text = Label::new(Some("Contrast:"));
            contrast_label_text.set_width_chars(12);
            contrast_row.append(&contrast_label_text);
            contrast_row.append(c_scale);
            contrast_row.append(c_label);
            contrast_row.set_margin_top(8);
            contrast_row.set_margin_bottom(8);
            main_box.append(&contrast_row);
        }

        let (volume_scale, volume_label, volume_label_inner) = if max_volume > 0 {
            let scale = Scale::builder()
                .orientation(Orientation::Horizontal)
                .hexpand(true)
                .build();
            scale.set_range(0.0, 100.0);
            scale.set_digits(0);
            scale.set_draw_value(false);

            let label = Label::new(Some("50%"));
            label.set_width_chars(5);
            label.set_halign(gtk::Align::End);

            let volume_label_scroll = Rc::new(RefCell::new(label.clone()));
            let volume_scale_scroll = scale.clone();
            let volume_scroll_controller =
                EventControllerScroll::new(EventControllerScrollFlags::VERTICAL);
            volume_scroll_controller.connect_scroll(move |_, _dx, dy| {
                let current = volume_scale_scroll.value();
                let step = scroll_step as f64;
                let new_value = if dy < 0.0 {
                    (current + step).min(100.0)
                } else {
                    (current - step).max(0.0)
                };
                volume_scale_scroll.set_value(new_value);
                volume_label_scroll
                    .borrow()
                    .set_text(&format!("{}%", new_value as u8));
                Propagation::Proceed
            });
            scale.add_controller(volume_scroll_controller);

            let label_inner = Rc::new(RefCell::new(label.clone()));

            (Some(scale), Some(label), Some(label_inner))
        } else {
            (None, None, None)
        };

        if let (Some(v_scale), Some(v_label)) = (&volume_scale, &volume_label) {
            let volume_row = Box::new(Orientation::Horizontal, 8);
            let volume_label_text = Label::new(Some("Volume:"));
            volume_label_text.set_width_chars(12);
            volume_row.append(&volume_label_text);
            volume_row.append(v_scale);
            volume_row.append(v_label);
            volume_row.set_margin_top(8);
            volume_row.set_margin_bottom(8);
            main_box.append(&volume_row);
        }

        let input_source_combo = if supports_input_source {
            let combo = ComboBoxText::new();
            combo.append(Some("1"), "VGA");
            combo.append(Some("3"), "DVI");
            combo.append(Some("15"), "DisplayPort 1");
            combo.append(Some("16"), "DisplayPort 2");
            combo.append(Some("17"), "HDMI 1");
            combo.append(Some("18"), "HDMI 2");
            combo.append(Some("19"), "HDMI 3");
            combo.append(Some("20"), "HDMI 4");
            combo.append(Some("27"), "USB-C");
            Some(combo)
        } else {
            None
        };

        let power_mode_combo = if supports_power_mode {
            let combo = ComboBoxText::new();
            combo.append(Some("1"), "On");
            combo.append(Some("2"), "Standby");
            combo.append(Some("3"), "Suspend");
            combo.append(Some("4"), "Off");
            combo.append(Some("5"), "Normal");
            Some(combo)
        } else {
            None
        };

        if supports_input_source || supports_power_mode {
            let controls_row = Box::new(Orientation::Horizontal, 8);
            controls_row.set_margin_top(8);

            if let Some(ref combo) = &input_source_combo {
                let input_label = Label::new(Some("Input:"));
                input_label.set_width_chars(12);
                controls_row.append(&input_label);
                controls_row.append(combo);
            }

            if let Some(ref combo) = &power_mode_combo {
                let power_label = Label::new(Some("Power:"));
                power_label.set_width_chars(12);
                controls_row.append(&power_label);
                controls_row.append(combo);
            }

            main_box.append(&controls_row);
        }

        let container = ActionRow::builder().title(&name).build();
        container.add_suffix(&main_box);

        Self {
            container,
            brightness_scale,
            brightness_label,
            contrast_scale,
            contrast_label,
            volume_scale,
            volume_label,
            input_source_combo,
            power_mode_combo,
            brightness_label_inner,
            contrast_label_inner,
            volume_label_inner,
        }
    }

    pub fn set_brightness(&self, percentage: u8) {
        self.brightness_scale.set_value(percentage as f64);
        self.brightness_label.set_text(&format!("{}%", percentage));
    }

    pub fn set_contrast(&self, percentage: u8) {
        if let Some(ref scale) = self.contrast_scale {
            scale.set_value(percentage as f64);
        }
        if let Some(ref label) = self.contrast_label {
            label.set_text(&format!("{}%", percentage));
        }
    }

    pub fn connect_brightness_changed<F>(&self, callback: F)
    where
        F: Fn(u8) + Clone + 'static,
    {
        let label_inner = self.brightness_label_inner.clone();
        let callback_clone = callback.clone();
        let adjustment = self.brightness_scale.adjustment();
        adjustment.connect_value_changed(move |adj| {
            let val = adj.value() as u8;
            callback_clone(val);
            label_inner.borrow().set_text(&format!("{}%", val));
        });
    }

    pub fn connect_contrast_changed<F>(&self, callback: F)
    where
        F: Fn(u8) + Clone + 'static,
    {
        if let (Some(ref scale), Some(ref label_inner)) =
            (&self.contrast_scale, &self.contrast_label_inner)
        {
            let label_inner = label_inner.clone();
            let callback_clone = callback.clone();
            let adjustment = scale.adjustment();
            adjustment.connect_value_changed(move |adj| {
                let val = adj.value() as u8;
                callback_clone(val);
                label_inner.borrow().set_text(&format!("{}%", val));
            });
        }
    }

    pub fn has_contrast(&self) -> bool {
        self.contrast_scale.is_some()
    }

    pub fn has_volume(&self) -> bool {
        self.volume_scale.is_some()
    }

    pub fn has_input_source(&self) -> bool {
        self.input_source_combo.is_some()
    }

    pub fn has_power_mode(&self) -> bool {
        self.power_mode_combo.is_some()
    }

    pub fn set_volume(&self, percentage: u8) {
        if let Some(ref scale) = self.volume_scale {
            scale.set_value(percentage as f64);
        }
        if let Some(ref label) = self.volume_label {
            label.set_text(&format!("{}%", percentage));
        }
    }

    pub fn set_input_source(&self, source_code: u8) {
        if let Some(ref combo) = self.input_source_combo {
            let code_str = source_code.to_string();
            combo.set_active_id(Some(&code_str));
        }
    }

    pub fn set_power_mode(&self, mode_code: u8) {
        if let Some(ref combo) = self.power_mode_combo {
            let code_str = mode_code.to_string();
            combo.set_active_id(Some(&code_str));
        }
    }

    pub fn connect_volume_changed<F>(&self, callback: F)
    where
        F: Fn(u8) + Clone + 'static,
    {
        if let (Some(ref scale), Some(ref label_inner)) =
            (&self.volume_scale, &self.volume_label_inner)
        {
            let label_inner = label_inner.clone();
            let callback_clone = callback.clone();
            let adjustment = scale.adjustment();
            adjustment.connect_value_changed(move |adj| {
                let val = adj.value() as u8;
                callback_clone(val);
                label_inner.borrow().set_text(&format!("{}%", val));
            });
        }
    }

    pub fn connect_input_source_changed<F>(&self, callback: F)
    where
        F: Fn(u8) + Clone + 'static,
    {
        if let Some(ref combo) = self.input_source_combo {
            let callback_clone = callback.clone();
            combo.connect_changed(move |combo| {
                if let Some(id) = combo.active_id() {
                    if let Ok(code) = id.parse::<u8>() {
                        callback_clone(code);
                    }
                }
            });
        }
    }

    pub fn connect_power_mode_changed<F>(&self, callback: F)
    where
        F: Fn(u8) + Clone + 'static,
    {
        if let Some(ref combo) = self.power_mode_combo {
            let callback_clone = callback.clone();
            combo.connect_changed(move |combo| {
                if let Some(id) = combo.active_id() {
                    if let Ok(code) = id.parse::<u8>() {
                        callback_clone(code);
                    }
                }
            });
        }
    }
}
