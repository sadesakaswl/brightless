use super::monitor_row::MonitorRow;
use crate::common::{CommonController, DdcError};
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
    controller: Rc<RefCell<CommonController>>,
}

impl MainWindow {
    pub fn new(app: &Application) -> Result<Self, DdcError> {
        let mut controller = CommonController::new();
        controller.initialize()?;
        let controller = Rc::new(RefCell::new(controller));
        let monitor_states = controller.borrow().monitors().to_vec();
        let monitor_count = monitor_states.len();
        let scroll_step = controller.borrow().scroll_step();
        let dynamic_contrast_enabled = controller.borrow().dynamic_contrast_enabled();
        let dynamic_contrast_global = controller.borrow().dynamic_contrast_global();
        let dynamic_contrast_ratio = controller.borrow().dynamic_contrast_ratio();
        let dynamic_contrast_per_monitor_ratio =
            controller.borrow().dynamic_contrast_per_monitor_ratio();

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

        let controller_inner = controller.clone();

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
            controller_inner.borrow_mut().set_scroll_step(val as i32);
        });

        let scroll_step_scale_inner = scroll_step_scale.clone();
        let scroll_step_label_scroll = scroll_step_value_label.clone();
        let controller_scroll = controller.clone();
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
            controller_scroll.borrow_mut().set_scroll_step(val as i32);
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
        dc_enable_switch.set_active(dynamic_contrast_enabled);
        dc_enable_row.append(&dc_enable_label);
        dc_enable_row.append(&dc_enable_switch);
        popover_box.append(&dc_enable_row);

        // Sub-section container (visible only when DC enabled)
        let dc_sub_box = Box::new(Orientation::Vertical, 8);
        dc_sub_box.set_visible(dynamic_contrast_enabled);
        popover_box.append(&dc_sub_box);

        let dc_global_row = Box::new(Orientation::Horizontal, 8);
        let dc_global_label = Label::new(Some("Apply to all monitors"));
        dc_global_label.set_hexpand(true);
        dc_global_label.set_halign(gtk::Align::Start);
        let dc_global_switch = Switch::new();
        dc_global_switch.set_active(dynamic_contrast_global);
        dc_global_row.append(&dc_global_label);
        dc_global_row.append(&dc_global_switch);
        dc_sub_box.append(&dc_global_row);

        // Global ratio row
        let dc_ratio_row = Box::new(Orientation::Horizontal, 8);
        let dc_ratio_label = Label::new(Some("Contrast Ratio:"));
        dc_ratio_label.set_halign(gtk::Align::Start);
        dc_ratio_label.set_width_chars(12);
        let dc_ratio_value_label = Label::new(Some(&format!("{:.1}", dynamic_contrast_ratio)));
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
        dc_ratio_scale.set_value(dynamic_contrast_ratio as f64);
        dc_sub_box.append(&dc_ratio_scale);

        // Per-monitor ratio switch
        let dc_per_monitor_row = Box::new(Orientation::Horizontal, 8);
        let dc_per_monitor_label = Label::new(Some("Per-monitor ratio"));
        dc_per_monitor_label.set_hexpand(true);
        dc_per_monitor_label.set_halign(gtk::Align::Start);
        let dc_per_monitor_switch = Switch::new();
        dc_per_monitor_switch.set_active(dynamic_contrast_per_monitor_ratio);
        dc_per_monitor_row.append(&dc_per_monitor_label);
        dc_per_monitor_row.append(&dc_per_monitor_switch);
        dc_sub_box.append(&dc_per_monitor_row);

        // Per-monitor ratio scales container
        let dc_per_monitor_box = Box::new(Orientation::Vertical, 8);
        dc_per_monitor_box.set_visible(dynamic_contrast_per_monitor_ratio);
        dc_sub_box.append(&dc_per_monitor_box);

        popover.set_child(Some(&popover_box));

        settings_button.connect_clicked(move |_| {
            popover.popup();
        });

        header_bar.pack_end(&settings_button);

        let mut monitor_rows_vec = Vec::new();

        for i in 0..monitor_count {
            let state = &monitor_states[i];

            let row = MonitorRow::new(
                state.name.clone(),
                state.min_brightness,
                state.max_brightness,
                state.min_contrast,
                state.max_contrast,
                state.min_volume,
                state.max_volume,
                state.capabilities.supports_input_source,
                state.capabilities.supports_power_mode,
                scroll_step,
                state.dynamic_contrast_enabled,
                dynamic_contrast_global,
                state.dynamic_contrast_ratio,
            );

            let controller_clone = controller.clone();
            let idx = i;
            row.connect_brightness_changed(move |value| {
                if let Ok(mut controller) = controller_clone.try_borrow_mut() {
                    controller.set_brightness(idx, value as i32);
                }
            });

            let controller_clone2 = controller.clone();
            let idx2 = i;
            if row.has_contrast() {
                row.connect_contrast_changed(move |value| {
                    if let Ok(mut controller) = controller_clone2.try_borrow_mut() {
                        controller.set_contrast(idx2, value as i32);
                    }
                });
            }

            let controller_clone3 = controller.clone();
            let idx3 = i;
            if row.has_volume() {
                row.connect_volume_changed(move |value| {
                    if let Ok(mut controller) = controller_clone3.try_borrow_mut() {
                        controller.set_volume(idx3, value as i32);
                    }
                });
            }

            let controller_clone4 = controller.clone();
            let idx4 = i;
            if row.has_input_source() {
                row.connect_input_source_changed(move |value| {
                    if let Ok(mut controller) = controller_clone4.try_borrow_mut() {
                        controller.set_input_source(idx4, value as i32);
                    }
                });
            }

            let controller_clone5 = controller.clone();
            let idx5 = i;
            if row.has_power_mode() {
                row.connect_power_mode_changed(move |value| {
                    if let Ok(mut controller) = controller_clone5.try_borrow_mut() {
                        controller.set_power_mode(idx5, value as i32);
                    }
                });
            }

            let controller_clone_dc = controller.clone();
            let idx_dc = i;
            if row.has_dynamic_contrast() {
                row.connect_dynamic_contrast_changed(move |brightness| {
                    if let Ok(mut controller) = controller_clone_dc.try_borrow_mut() {
                        controller.set_dynamic_contrast_brightness(idx_dc, brightness as i32);
                    }
                });
            }

            let controller_clone_toggle = controller.clone();
            let idx_toggle = i;
            if row.has_dynamic_contrast() {
                row.connect_dynamic_contrast_toggle_changed(move |enabled| {
                    if let Ok(mut controller) = controller_clone_toggle.try_borrow_mut() {
                        controller.set_monitor_dynamic_contrast_enabled(idx_toggle, enabled);
                    }
                });
            }

            monitor_rows_vec.push(row);
        }

        // Build per-monitor ratio UI now that monitor_rows_vec is populated
        for (index, row) in monitor_rows_vec.iter().enumerate() {
            if !row.has_dynamic_contrast() {
                continue;
            }
            let name = row.name.clone();
            let ratio = monitor_states[index].dynamic_contrast_ratio;
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

            let controller_pm = controller.clone();
            let pm_value_inner = pm_value.clone();
            let pm_adj = pm_scale.adjustment();
            pm_adj.connect_value_changed(move |adj| {
                let val = adj.value() as f32;
                pm_value_inner.set_text(&format!("{:.1}", val));
                if let Ok(mut controller) = controller_pm.try_borrow_mut() {
                    controller.set_monitor_ratio(index, val);
                }
            });
        }

        // Wire settings signals after monitor_rows_vec is built
        let monitor_rows_ref: Rc<RefCell<Vec<MonitorRow>>> =
            Rc::new(RefCell::new(monitor_rows_vec));

        let dc_sub_box_inner = dc_sub_box.clone();
        let monitor_rows_enable = monitor_rows_ref.clone();
        let controller_enable = controller.clone();
        dc_enable_switch.connect_state_set(move |_, state| {
            let global = {
                let mut controller = controller_enable.borrow_mut();
                controller.set_dynamic_contrast_enabled(state);
                controller.dynamic_contrast_global()
            };
            dc_sub_box_inner.set_visible(state);
            for row in monitor_rows_enable.borrow().iter() {
                if state && global {
                    row.set_dynamic_contrast_mode(true);
                } else if !state {
                    row.set_dynamic_contrast_mode(false);
                }
                row.set_dynamic_contrast_toggle_visible(state && !global);
            }
            Propagation::Proceed
        });

        let monitor_rows_global = monitor_rows_ref.clone();
        let controller_global = controller.clone();
        dc_global_switch.connect_state_set(move |_, state| {
            let enabled = {
                let mut controller = controller_global.borrow_mut();
                controller.set_dynamic_contrast_global(state);
                controller.dynamic_contrast_enabled()
            };
            for row in monitor_rows_global.borrow().iter() {
                if enabled {
                    row.set_dynamic_contrast_mode(state);
                    row.set_dynamic_contrast_toggle_visible(!state);
                    if let Some(ref toggle) = row.dynamic_contrast_toggle {
                        toggle.set_active(state);
                    }
                }
            }
            Propagation::Proceed
        });

        let controller_ratio = controller.clone();
        let dc_ratio_value_label_inner = dc_ratio_value_label.clone();
        let _dc_ratio_scale_inner = dc_ratio_scale.clone();
        let adjustment_ratio = dc_ratio_scale.adjustment();
        adjustment_ratio.connect_value_changed(move |adj| {
            let val = adj.value() as f32;
            dc_ratio_value_label_inner.set_text(&format!("{:.1}", val));
            controller_ratio
                .borrow_mut()
                .set_dynamic_contrast_ratio(val);
        });

        let dc_per_monitor_box_inner = dc_per_monitor_box.clone();
        let dc_ratio_row_inner = dc_ratio_row.clone();
        let dc_ratio_scale_inner2 = dc_ratio_scale.clone();
        let controller_pm_switch = controller.clone();
        dc_per_monitor_switch.connect_state_set(move |_, state| {
            controller_pm_switch
                .borrow_mut()
                .set_dynamic_contrast_per_monitor_ratio(state);
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
            controller,
        })
    }

    pub fn init_brightness(&self) {
        let controller = self.controller.borrow();
        let rows = self.monitor_rows.borrow();

        for (index, row) in rows.iter().enumerate() {
            let Some(state) = controller.monitor(index) else {
                continue;
            };

            row.set_brightness(state.brightness);
            if row.has_dynamic_contrast() {
                row.set_dynamic_contrast(state.dynamic_contrast_brightness);
            }
            if row.has_contrast() {
                row.set_contrast(state.contrast);
            }
            if row.has_volume() {
                row.set_volume(state.volume);
            }
            if row.has_input_source() {
                row.set_input_source(state.input_source_code);
            }
            if row.has_power_mode() {
                row.set_power_mode(state.power_mode_code);
            }
        }
    }
}
