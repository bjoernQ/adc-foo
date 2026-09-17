pub const I2C_SCL_GPIO: u8 = 4;
pub const I2C_SDA_GPIO: u8 = 5;
pub const SWEEP_STEPS: usize = 11;
pub const SETTLE_MS: u32 = 20;
pub const ADC_SAMPLES: usize = 8;
pub const LINK_TEST_MIN_DELTA: u16 = 800;

#[cfg(feature = "esp32")]
pub const CHIP: &str = "esp32";
#[cfg(feature = "esp32c2")]
pub const CHIP: &str = "esp32c2";
#[cfg(feature = "esp32c3")]
pub const CHIP: &str = "esp32c3";
#[cfg(feature = "esp32c5")]
pub const CHIP: &str = "esp32c5";
#[cfg(feature = "esp32c6")]
pub const CHIP: &str = "esp32c6";
#[cfg(feature = "esp32c61")]
pub const CHIP: &str = "esp32c61";
#[cfg(feature = "esp32h2")]
pub const CHIP: &str = "esp32h2";
#[cfg(feature = "esp32p4")]
pub const CHIP: &str = "esp32p4";
#[cfg(feature = "esp32s2")]
pub const CHIP: &str = "esp32s2";
#[cfg(feature = "esp32s3")]
pub const CHIP: &str = "esp32s3";
#[cfg(feature = "esp32s31")]
pub const CHIP: &str = "esp32s31";

#[cfg(any(feature = "esp32s2", feature = "esp32s3"))]
pub const ADC_GPIO: u8 = 7;
#[cfg(feature = "esp32")]
pub const ADC_GPIO: u8 = 34;
#[cfg(any(feature = "esp32c2", feature = "esp32c3", feature = "esp32c61", feature = "esp32h2"))]
pub const ADC_GPIO: u8 = 3;
#[cfg(any(feature = "esp32c5", feature = "esp32c6"))]
pub const ADC_GPIO: u8 = 6;
#[cfg(feature = "esp32p4")]
pub const ADC_GPIO: u8 = 16;
#[cfg(feature = "esp32s31")]
pub const ADC_GPIO: u8 = 48;

#[cfg(feature = "esp32s31")]
pub const CALIB_SCHEME: &str = "none";
#[cfg(any(feature = "esp32", feature = "esp32c2", feature = "esp32s2"))]
pub const CALIB_SCHEME: &str = "line_fitting";
#[cfg(not(any(
    feature = "esp32s31",
    feature = "esp32",
    feature = "esp32c2",
    feature = "esp32s2"
)))]
pub const CALIB_SCHEME: &str = "curve_fitting";

pub fn linear_mv(raw: u16) -> u32 {
    (raw as u32 * crate::mcp4725::VREF_MV) / 4095
}

pub fn print_wiring_banner(adc_channel: u8) {
    esp_println::println!("=== Wiring for {CHIP} (Rust) ===");
    esp_println::println!("  I2C SCL       : GPIO{I2C_SCL_GPIO}");
    esp_println::println!("  I2C SDA       : GPIO{I2C_SDA_GPIO}");
    esp_println::println!(
        "  MCP4725 addr  : 0x{:02X}",
        crate::mcp4725::MCP4725_ADDR
    );
    esp_println::println!("  MCP4725 VOUT  -> GPIO{ADC_GPIO} (ADC1 channel {adc_channel})");
    esp_println::println!("  Vref assumed  : {} mV", crate::mcp4725::VREF_MV);
    esp_println::println!("  Attenuation   : 11 dB");
    esp_println::println!("  Calib scheme  : {CALIB_SCHEME}");
    esp_println::println!("=========================================");
}
