use ddc::Ddc;
use ddc_i2c::I2cDdc;
use i2c_linux::I2c;
use std::collections::HashMap;
use std::fs;
use std::fs::File;
use std::io::Read;
use std::path::Path;
use thiserror::Error;

#[derive(Error, Debug)]
pub enum DdcError {
    #[error("Failed to open I2C device: {0}")]
    OpenError(String),
    #[error("DDC communication error: {0}")]
    CommError(String),
    #[error("No DDC monitors found")]
    NoMonitors,
    #[error("Permission denied: {0}")]
    PermissionDenied(String),
    #[error("IO error: {0}")]
    IoError(#[from] std::io::Error),
}

pub struct Monitor {
    pub handle: I2cDdc<I2c<File>>,
    pub name: String,
    pub connector: String,
    pub min_brightness: u16,
    pub max_brightness: u16,
    pub min_contrast: u16,
    pub max_contrast: u16,
}

pub struct DdcManager {
    pub monitors: Vec<Monitor>,
}

impl DdcManager {
    pub fn new() -> Result<Self, DdcError> {
        let monitors = Self::discover_monitors()?;
        Ok(Self { monitors })
    }

    fn get_connected_connectors() -> Vec<String> {
        let mut connectors = Vec::new();

        if let Ok(entries) = fs::read_dir("/sys/class/drm") {
            for entry in entries.flatten() {
                let path = entry.path();
                if path.is_dir() {
                    if let Some(name) = path.file_name().and_then(|n| n.to_str()) {
                        if !name.starts_with("card") || name.contains("-") {
                            let status_path = path.join("status");
                            if let Ok(status) = fs::read_to_string(&status_path) {
                                if status.trim() == "connected" {
                                    connectors.push(name.to_string());
                                }
                            }
                        }
                    }
                }
            }
        }

        connectors
    }

    fn read_edid(connector: &str) -> Option<Vec<u8>> {
        let edid_path = format!("/sys/class/drm/{}/edid", connector);
        let path = Path::new(&edid_path);

        if path.exists() {
            let mut file = match File::open(path) {
                Ok(f) => f,
                Err(_) => return None,
            };

            let mut data = Vec::new();
            if file.read_to_end(&mut data).is_ok() && !data.is_empty() && data.len() >= 128 {
                return Some(data);
            }
        }
        None
    }

    fn parse_edid_name(edid: &[u8]) -> Option<String> {
        if edid.len() < 128 {
            return None;
        }

        if edid[0] != 0x00
            || edid[1] != 0xFF
            || edid[2] != 0xFF
            || edid[3] != 0xFF
            || edid[4] != 0xFF
            || edid[5] != 0xFF
            || edid[6] != 0xFF
            || edid[7] != 0x00
        {
            return None;
        }

        let mfg_id =
            ((edid[8].saturating_sub(64) as usize) * 64) + (edid[9].saturating_sub(64) as usize);
        let mfg_chars = [
            ((mfg_id >> 10) & 0x1F) as u8,
            ((mfg_id >> 5) & 0x1F) as u8,
            (mfg_id & 0x1F) as u8,
        ];

        let manufacturer: String = mfg_chars
            .iter()
            .filter_map(|&c| {
                if c == 0 || c > 26 {
                    None
                } else {
                    Some((c + 64) as char)
                }
            })
            .collect();

        let product_code = u16::from_be_bytes([edid[10], edid[11]]);

        let mut name = String::new();
        for i in 0..14 {
            let offset = 0x36 + (i * 18);
            if offset + 18 > edid.len() {
                break;
            }
            if edid[offset] == 0x00 && edid[offset + 1] == 0x00 && edid[offset + 2] == 0x00 {
                if edid[offset + 3] == 0xFC {
                    for j in 0..13 {
                        let c = edid[offset + 5 + j];
                        if c == 0x0A {
                            break;
                        }
                        if c >= 0x20 && c < 0x7F {
                            name.push(c as char);
                        }
                    }
                    break;
                }
            }
        }

        if name.is_empty() {
            Some(format!("{} {:04x}", manufacturer, product_code))
        } else {
            Some(name)
        }
    }

    fn get_brightness_range(ddc: &mut I2cDdc<I2c<File>>) -> Option<(u16, u16)> {
        match ddc.get_vcp_feature(0x10) {
            Ok(vcp) => Some((0, vcp.maximum())),
            Err(_) => None,
        }
    }

    fn get_contrast_range(ddc: &mut I2cDdc<I2c<File>>) -> Option<(u16, u16)> {
        match ddc.get_vcp_feature(0x12) {
            Ok(vcp) => Some((0, vcp.maximum())),
            Err(_) => None,
        }
    }

    fn test_ddc_connection(path: &str) -> Option<(I2cDdc<I2c<File>>, u16, u16, u16, u16)> {
        match I2c::from_path(path) {
            Ok(i2c) => {
                let mut ddc = I2cDdc::new(i2c);
                if let Some((min_brightness, max_brightness)) = Self::get_brightness_range(&mut ddc)
                {
                    let (min_contrast, max_contrast) =
                        Self::get_contrast_range(&mut ddc).unwrap_or((0, 0));
                    return Some((
                        ddc,
                        min_brightness,
                        max_brightness,
                        min_contrast,
                        max_contrast,
                    ));
                }
            }
            Err(_) => {}
        }
        None
    }

    fn discover_monitors() -> Result<Vec<Monitor>, DdcError> {
        let connectors = Self::get_connected_connectors();

        if connectors.is_empty() {
            return Err(DdcError::NoMonitors);
        }

        let mut monitors: Vec<Monitor> = Vec::new();
        let mut used_i2c: HashMap<String, bool> = HashMap::new();

        for connector in &connectors {
            let edid = Self::read_edid(connector);
            let name = edid
                .as_ref()
                .and_then(|e| Self::parse_edid_name(e))
                .unwrap_or_else(|| "Unknown Monitor".to_string());

            let entries = fs::read_dir("/dev").map_err(|e| DdcError::OpenError(e.to_string()))?;

            for entry in entries.flatten() {
                let path = entry.path();
                let path_str = path.to_string_lossy().to_string();

                if !path_str.starts_with("/dev/i2c-") {
                    continue;
                }

                if used_i2c.contains_key(&path_str) {
                    continue;
                }

                if let Some((handle, min_brightness, max_brightness, min_contrast, max_contrast)) =
                    Self::test_ddc_connection(&path_str)
                {
                    used_i2c.insert(path_str, true);

                    monitors.push(Monitor {
                        handle,
                        name: name.clone(),
                        connector: connector.clone(),
                        min_brightness,
                        max_brightness,
                        min_contrast,
                        max_contrast,
                    });
                    break;
                }
            }
        }

        if monitors.is_empty() {
            return Err(DdcError::NoMonitors);
        }

        for (i, monitor) in monitors.iter_mut().enumerate() {
            if monitor.name == "Unknown Monitor" {
                monitor.name = format!("Monitor {}", i + 1);
            }
        }

        Ok(monitors)
    }

    pub fn get_brightness_percentage(&mut self, index: usize) -> Result<u8, DdcError> {
        if index >= self.monitors.len() {
            return Err(DdcError::NoMonitors);
        }

        let vcp = self.monitors[index]
            .handle
            .get_vcp_feature(0x10)
            .map_err(|e| DdcError::CommError(format!("Failed to get brightness: {}", e)))?;

        let current = vcp.value();
        let min = self.monitors[index].min_brightness;
        let max = self.monitors[index].max_brightness;

        if max <= min {
            return Ok(0);
        }

        let current = current.clamp(min, max);
        let percentage = ((current.saturating_sub(min)) * 100) / (max - min);
        Ok(percentage as u8)
    }

    pub fn set_brightness_percentage(
        &mut self,
        index: usize,
        percentage: u8,
    ) -> Result<(), DdcError> {
        if index >= self.monitors.len() {
            return Err(DdcError::NoMonitors);
        }

        let min = self.monitors[index].min_brightness;
        let max = self.monitors[index].max_brightness;

        if max <= min {
            return Ok(());
        }

        let percentage = percentage.clamp(0, 100);
        let raw = min + ((percentage as u16 * (max - min)) / 100);

        self.monitors[index]
            .handle
            .set_vcp_feature(0x10, raw)
            .map_err(|e| DdcError::CommError(format!("Failed to set brightness: {}", e)))?;

        Ok(())
    }

    pub fn get_contrast_percentage(&mut self, index: usize) -> Result<u8, DdcError> {
        if index >= self.monitors.len() {
            return Err(DdcError::NoMonitors);
        }

        let min = self.monitors[index].min_contrast;
        let max = self.monitors[index].max_contrast;

        if max == 0 || max <= min {
            return Err(DdcError::CommError("Contrast not supported".to_string()));
        }

        let vcp = self.monitors[index]
            .handle
            .get_vcp_feature(0x12)
            .map_err(|e| DdcError::CommError(format!("Failed to get contrast: {}", e)))?;

        let current = vcp.value();
        let current = current.clamp(min, max);
        let percentage = ((current.saturating_sub(min)) * 100) / (max - min);
        Ok(percentage as u8)
    }

    pub fn set_contrast_percentage(
        &mut self,
        index: usize,
        percentage: u8,
    ) -> Result<(), DdcError> {
        if index >= self.monitors.len() {
            return Err(DdcError::NoMonitors);
        }

        let min = self.monitors[index].min_contrast;
        let max = self.monitors[index].max_contrast;

        if max == 0 || max <= min {
            return Ok(());
        }

        let percentage = percentage.clamp(0, 100);
        let raw = min + ((percentage as u16 * (max - min)) / 100);

        self.monitors[index]
            .handle
            .set_vcp_feature(0x12, raw)
            .map_err(|e| DdcError::CommError(format!("Failed to set contrast: {}", e)))?;

        Ok(())
    }

    pub fn supports_contrast(&self, index: usize) -> bool {
        if index >= self.monitors.len() {
            return false;
        }
        self.monitors[index].max_contrast > 0
    }
}
