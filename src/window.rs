use crate::ddc_manager::{DdcError, DdcManager};
use crate::monitor_row::MonitorRow;
use crate::settings::AppSettings;
use adw::prelude::*;
use adw::{Application, ApplicationWindow, HeaderBar, ToolbarView, ViewStack, ViewSwitcher};
use glib::Propagation;
use gtk::{
    Box, Button, EventControllerScroll, EventControllerScrollFlags, Label, ListBox, Orientation,
    Popover, Scale, ScrolledWindow, SelectionMode, Switch,
};
use std::cell::RefCell;
use std::rc::Rc;

pub struct MainWindow {
    pub window: ApplicationWindow,
    pub stack: ViewStack,
    pub monitor_rows: Rc<RefCell<Vec<MonitorRow>>>,
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

        // --- Dynamic Contrast Section ---
        let dc_section_label = Label::new(Some("Dynamic Contrast"));
        dc_section_label.set_halign(gtk::Align::Start);
        dc_section_label.add_css_class("heading");
        popover_box.append(&dc_section_label);

        let dc_enable_row = Box::new(Orientation::Horizontal, 8);
        let dc_enable_label = Label::new(Some("Enable Dynamic Contrast"));
        dc_enable_label.set_hexpand(true);
        dc_enable_label.set_halign(gtk::Align::Start);
        let dc_enable_switch = Switch::new();
        dc_enable_switch.set_active(settings.borrow().dynamic_contrast_enabled);
        dc_enable_row.append(&dc_enable_label);
        dc_enable_row.append(&dc_enable_switch);
        popover_box.append(&dc_enable_row);

        // Sub-section container (visible only when DC enabled)
        let dc_sub_box = Box::new(Orientation::Vertical, 8);
        dc_sub_box.set_visible(settings.borrow().dynamic_contrast_enabled);
        popover_box.append(&dc_sub_box);

        let dc_global_row = Box::new(Orientation::Horizontal, 8);
        let dc_global_label = Label::new(Some("Apply to all monitors"));
        dc_global_label.set_hexpand(true);
        dc_global_label.set_halign(gtk::Align::Start);
        let dc_global_switch = Switch::new();
        dc_global_switch.set_active(settings.borrow().dynamic_contrast_global);
        dc_global_row.append(&dc_global_label);
        dc_global_row.append(&dc_global_switch);
        dc_sub_box.append(&dc_global_row);

        // Global ratio row
        let dc_ratio_row = Box::new(Orientation::Horizontal, 8);
        let dc_ratio_label = Label::new(Some("Contrast Ratio:"));
        dc_ratio_label.set_halign(gtk::Align::Start);
        dc_ratio_label.set_width_chars(12);
        let dc_ratio_value_label = Label::new(Some(&format!("{:.1}", settings.borrow().dynamic_contrast_ratio)));
        dc_ratio_value_label.set_halign(gtk::Align::End);
        dc_ratio_value_label.set_hexpand(true);
        dc_ratio_row.append(&dc_ratio_label);
        dc_ratio_row.append(&dc_ratio_value_label);
        dc_sub_box.append(&dc_ratio_row);

        let dc_ratio_scale = Scale::builder()
            .orientation(Orientation::Horizontal)
            .hexpand(true)
            .build();
        dc_ratio_scale.set_range(0.1, 2.0);
        dc_ratio_scale.set_digits(1);
        dc_ratio_scale.set_draw_value(false);
        dc_ratio_scale.set_value(settings.borrow().dynamic_contrast_ratio as f64);
        dc_sub_box.append(&dc_ratio_scale);

        // Per-monitor ratio switch
        let dc_per_monitor_row = Box::new(Orientation::Horizontal, 8);
        let dc_per_monitor_label = Label::new(Some("Per-monitor ratio"));
        dc_per_monitor_label.set_hexpand(true);
        dc_per_monitor_label.set_halign(gtk::Align::Start);
        let dc_per_monitor_switch = Switch::new();
        dc_per_monitor_switch.set_active(settings.borrow().dynamic_contrast_per_monitor_ratio);
        dc_per_monitor_row.append(&dc_per_monitor_label);
        dc_per_monitor_row.append(&dc_per_monitor_switch);
        dc_sub_box.append(&dc_per_monitor_row);

        // Per-monitor ratio scales container
        let dc_per_monitor_box = Box::new(Orientation::Vertical, 8);
        dc_per_monitor_box.set_visible(settings.borrow().dynamic_contrast_per_monitor_ratio);
        dc_sub_box.append(&dc_per_monitor_box);

        popover.set_child(Some(&popover_box));

        settings_button.connect_clicked(move |_| {
            popover.popup();
        });

        header_bar.pack_end(&settings_button);

        let mut monitor_rows_vec = Vec::new();
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

            let dc_enabled_for_monitor = if settings.borrow().dynamic_contrast_enabled {
                if settings.borrow().dynamic_contrast_global {
                    true
                } else {
                    *settings.borrow().monitor_dynamic_contrast.get(&name).unwrap_or(&true)
                }
            } else {
                false
            };
            let ratio = if settings.borrow().dynamic_contrast_per_monitor_ratio {
                *settings.borrow().monitor_ratios.get(&name).unwrap_or(&settings.borrow().dynamic_contrast_ratio)
            } else {
                settings.borrow().dynamic_contrast_ratio
            };

            let row = MonitorRow::new(
                name.clone(),
                min_brightness,
                max_brightness,
                min_contrast,
                max_contrast,
                min_volume,
                max_volume,
                supports_input_source,
                supports_power_mode,
                scroll_step,
                dc_enabled_for_monitor,
                settings.borrow().dynamic_contrast_global,
                ratio,
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

            let ddc_clone_dc = ddc_ref.clone();
            let idx_dc = i;
            let settings_clone_dc = settings.clone();
            let name_clone_dc = name.clone();
            if row.has_dynamic_contrast() {
                row.connect_dynamic_contrast_changed(move |brightness| {
                    let settings = settings_clone_dc.borrow();
                    let ratio = if settings.dynamic_contrast_per_monitor_ratio {
                        *settings.monitor_ratios.get(&name_clone_dc).unwrap_or(&settings.dynamic_contrast_ratio)
                    } else {
                        settings.dynamic_contrast_ratio
                    };
                    let contrast = ((brightness as f32 * ratio).round() as u8).min(100);
                    if let Ok(mut ddc) = ddc_clone_dc.try_borrow_mut() {
                        let _ = ddc.set_brightness_percentage(idx_dc, brightness);
                        let _ = ddc.set_contrast_percentage(idx_dc, contrast);
                    }
                });
            }

            let settings_clone_toggle = settings.clone();
            let name_clone_toggle = name.clone();
            if row.has_dynamic_contrast() {
                row.connect_dynamic_contrast_toggle_changed(move |enabled| {
                    settings_clone_toggle.borrow_mut().monitor_dynamic_contrast.insert(name_clone_toggle.clone(), enabled);
                    let _ = settings_clone_toggle.borrow().save();
                });
            }

            monitor_rows_vec.push(row);
        }

        // Build per-monitor ratio UI now that monitor_rows_vec is populated
        for row in &monitor_rows_vec {
            if !row.has_dynamic_contrast() {
                continue;
            }
            let name = row.name.clone();
            let ratio = *settings.borrow().monitor_ratios.get(&name).unwrap_or(&settings.borrow().dynamic_contrast_ratio);
            let pm_label = Label::new(Some(&format!("{} Ratio:", name)));
            pm_label.set_halign(gtk::Align::Start);
            pm_label.set_hexpand(true);
            let pm_value = Label::new(Some(&format!("{:.1}", ratio)));
            pm_value.set_halign(gtk::Align::End);
            let pm_row = Box::new(Orientation::Horizontal, 8);
            pm_row.append(&pm_label);
            pm_row.append(&pm_value);
            dc_per_monitor_box.append(&pm_row);

            let pm_scale = Scale::builder()
                .orientation(Orientation::Horizontal)
                .hexpand(true)
                .build();
            pm_scale.set_range(0.1, 2.0);
            pm_scale.set_digits(1);
            pm_scale.set_draw_value(false);
            pm_scale.set_value(ratio as f64);
            dc_per_monitor_box.append(&pm_scale);

            let settings_pm = settings.clone();
            let name_pm = name.clone();
            let pm_value_inner = pm_value.clone();
            let pm_adj = pm_scale.adjustment();
            pm_adj.connect_value_changed(move |adj| {
                let val = adj.value() as f32;
                pm_value_inner.set_text(&format!("{:.1}", val));
                settings_pm.borrow_mut().monitor_ratios.insert(name_pm.clone(), val);
                let _ = settings_pm.borrow().save();
            });
        }

        // Wire settings signals after monitor_rows_vec is built
        let monitor_rows_ref: Rc<RefCell<Vec<MonitorRow>>> = Rc::new(RefCell::new(monitor_rows_vec));

        let dc_sub_box_inner = dc_sub_box.clone();
        let monitor_rows_enable = monitor_rows_ref.clone();
        let settings_enable = settings.clone();
        dc_enable_switch.connect_state_set(move |_, state| {
            settings_enable.borrow_mut().dynamic_contrast_enabled = state;
            let _ = settings_enable.borrow().save();
            dc_sub_box_inner.set_visible(state);
            for row in monitor_rows_enable.borrow().iter() {
                if state && settings_enable.borrow().dynamic_contrast_global {
                    row.set_dynamic_contrast_mode(true);
                } else if !state {
                    row.set_dynamic_contrast_mode(false);
                }
                row.set_dynamic_contrast_toggle_visible(state && !settings_enable.borrow().dynamic_contrast_global);
            }
            Propagation::Proceed
        });

        let monitor_rows_global = monitor_rows_ref.clone();
        let settings_global = settings.clone();
        dc_global_switch.connect_state_set(move |_, state| {
            settings_global.borrow_mut().dynamic_contrast_global = state;
            let _ = settings_global.borrow().save();
            for row in monitor_rows_global.borrow().iter() {
                if settings_global.borrow().dynamic_contrast_enabled {
                    row.set_dynamic_contrast_mode(state);
                    row.set_dynamic_contrast_toggle_visible(!state);
                    if let Some(ref toggle) = row.dynamic_contrast_toggle {
                        toggle.set_active(state);
                    }
                }
            }
            Propagation::Proceed
        });

        let settings_ratio = settings.clone();
        let dc_ratio_value_label_inner = dc_ratio_value_label.clone();
        let _dc_ratio_scale_inner = dc_ratio_scale.clone();
        let adjustment_ratio = dc_ratio_scale.adjustment();
        adjustment_ratio.connect_value_changed(move |adj| {
            let val = adj.value() as f32;
            dc_ratio_value_label_inner.set_text(&format!("{:.1}", val));
            settings_ratio.borrow_mut().dynamic_contrast_ratio = val;
            let _ = settings_ratio.borrow().save();
        });

        let dc_per_monitor_box_inner = dc_per_monitor_box.clone();
        let dc_ratio_row_inner = dc_ratio_row.clone();
        let dc_ratio_scale_inner2 = dc_ratio_scale.clone();
        let settings_pm_switch = settings.clone();
        dc_per_monitor_switch.connect_state_set(move |_, state| {
            settings_pm_switch.borrow_mut().dynamic_contrast_per_monitor_ratio = state;
            let _ = settings_pm_switch.borrow().save();
            dc_per_monitor_box_inner.set_visible(state);
            dc_ratio_row_inner.set_visible(!state);
            dc_ratio_scale_inner2.set_visible(!state);
            Propagation::Proceed
        });

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

        {
            let rows = monitor_rows_ref.borrow();
            for row in rows.iter() {
                list.append(&row.container);
            }
        }

        let scrolled = ScrolledWindow::new();
        scrolled.set_child(Some(&list));
        scrolled.set_vexpand(true);

        toolbar_view.set_content(Some(&scrolled));

        window.set_content(Some(&content));

        Ok(Self {
            window,
            stack,
            monitor_rows: monitor_rows_ref,
            ddc: ddc_ref,
            settings,
        })
    }

    pub fn init_brightness(&self) {
        let mut ddc = self.ddc.borrow_mut();
        let rows = self.monitor_rows.borrow();
        for (i, row) in rows.iter().enumerate() {
            match ddc.get_brightness_percentage(i) {
                Ok(percentage) => {
                    row.set_brightness(percentage);
                    if row.has_dynamic_contrast() {
                        row.set_dynamic_contrast(percentage);
                    }
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
