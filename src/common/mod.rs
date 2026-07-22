mod ddc_manager;
#[cfg(all(feature = "qt", not(feature = "gtk")))]
mod model;
mod settings;

#[cfg(all(feature = "gtk", not(feature = "qt")))]
pub(crate) use ddc_manager::DdcError;
pub(crate) use ddc_manager::{DdcManager, InputSource, PowerMode};
#[cfg(all(feature = "qt", not(feature = "gtk")))]
pub(crate) use model::{
    clamp_percent, clamp_ratio, contrast_for_dynamic_brightness,
    dynamic_contrast_enabled_for_monitor, ratio_for_monitor, valid_index, MonitorCapabilities,
    MonitorUiState,
};
pub(crate) use settings::AppSettings;
