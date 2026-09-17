#![no_std]
#![no_main]

mod board;
mod mcp4725;

use esp_backtrace as _;
use esp_hal::{
    analog::adc::{Adc, AdcCalScheme, AdcChannel, AdcConfig, AdcPin, Attenuation},
    delay::Delay,
    i2c::master::{Config as I2cConfig, I2c},
    main,
    time::Rate,
};

esp_bootloader_esp_idf::esp_app_desc!();

type Adc1 = esp_hal::peripherals::ADC1<'static>;

#[cfg(feature = "esp32s31")]
fn calibrate_raw(_raw: u16, _channel: u8) -> Option<u16> {
    None
}

#[cfg(all(
    not(feature = "esp32s31"),
    any(
        feature = "esp32c3",
        feature = "esp32c5",
        feature = "esp32c6",
        feature = "esp32c61",
        feature = "esp32h2",
        feature = "esp32p4",
        feature = "esp32s3"
    )
))]
fn calibrate_raw(raw: u16, channel: u8) -> Option<u16> {
    use esp_hal::analog::adc::AdcCalCurve;

    let cal = AdcCalCurve::<Adc1>::new_cal_with_channel(Attenuation::_11dB, channel);
    Some(cal.adc_val(raw))
}

#[cfg(all(
    not(feature = "esp32s31"),
    any(feature = "esp32", feature = "esp32c2", feature = "esp32s2")
))]
fn calibrate_raw(raw: u16, channel: u8) -> Option<u16> {
    use esp_hal::analog::adc::AdcCalLine;

    let cal = AdcCalLine::<Adc1>::new_cal_with_channel(Attenuation::_11dB, channel);
    Some(cal.adc_val(raw))
}

fn read_adc_averaged<PIN>(adc: &mut Adc<'_, Adc1, esp_hal::Blocking>, pin: &mut AdcPin<PIN, Adc1>) -> u16
where
    PIN: AdcChannel,
{
    let mut sum = 0u32;
    for _ in 0..board::ADC_SAMPLES {
        sum += nb::block!(adc.read_oneshot(pin)).unwrap_or(0) as u32;
    }
    (sum / board::ADC_SAMPLES as u32) as u16
}

fn run_link_self_test<PIN>(
    i2c: &mut I2c<'_, esp_hal::Blocking>,
    adc: &mut Adc<'_, Adc1, esp_hal::Blocking>,
    pin: &mut AdcPin<PIN, Adc1>,
    delay: &mut Delay,
) where
    PIN: AdcChannel,
{
    esp_println::println!(
        "Running DAC/ADC link self-test on GPIO{} ...",
        board::ADC_GPIO
    );

    mcp4725::set_code(i2c, 0, delay).unwrap();
    delay.delay_millis(board::SETTLE_MS);
    let raw_min = read_adc_averaged(adc, pin);

    mcp4725::set_code(i2c, 4095, delay).unwrap();
    delay.delay_millis(board::SETTLE_MS);
    let raw_max = read_adc_averaged(adc, pin);

    let delta = raw_max.abs_diff(raw_min);
    esp_println::println!(
        "Link self-test: raw@dac=0 -> {raw_min}, raw@dac=4095 -> {raw_max}, delta={delta}"
    );

    if delta < board::LINK_TEST_MIN_DELTA {
        esp_println::println!("ERROR: ADC raw barely changed (delta={delta}). Check wiring:");
        esp_println::println!(
            "  MCP4725 VOUT must connect to GPIO{} on {}",
            board::ADC_GPIO,
            board::CHIP
        );
    } else {
        esp_println::println!("Link self-test passed (delta={delta})");
    }
}

#[main]
fn main() -> ! {
    esp_println::logger::init_logger_from_env();
    let peripherals = esp_hal::init(esp_hal::Config::default());

    let mut delay = Delay::new();

    let mut i2c = I2c::new(
        peripherals.I2C0,
        I2cConfig::default().with_frequency(Rate::from_khz(100)),
    )
    .unwrap()
    .with_sda(peripherals.GPIO5)
    .with_scl(peripherals.GPIO4);

    let adc_gpio = {
        cfg_if::cfg_if! {
            if #[cfg(any(feature = "esp32s2", feature = "esp32s3"))] {
                peripherals.GPIO7
            } else if #[cfg(feature = "esp32")] {
                peripherals.GPIO34
            } else if #[cfg(any(feature = "esp32c2", feature = "esp32c3", feature = "esp32c61", feature = "esp32h2"))] {
                peripherals.GPIO3
            } else if #[cfg(feature = "esp32p4")] {
                peripherals.GPIO16
            } else if #[cfg(feature = "esp32s31")] {
                peripherals.GPIO48
            } else {
                peripherals.GPIO6
            }
        }
    };

    let mut adc_config = AdcConfig::<Adc1>::new();
    let mut adc_pin = adc_config.enable_pin(adc_gpio, Attenuation::_11dB);
    let adc_channel = adc_pin.pin.adc_channel();
    let mut adc = Adc::new(peripherals.ADC1, adc_config);

    board::print_wiring_banner(adc_channel);
    run_link_self_test(&mut i2c, &mut adc, &mut adc_pin, &mut delay);

    loop {
        for step in 0..board::SWEEP_STEPS {
            let dac_code = mcp4725::dac_code_for_step(step, board::SWEEP_STEPS);
            let target_mv = mcp4725::code_to_target_mv(dac_code);

            mcp4725::set_code(&mut i2c, dac_code, &mut delay).unwrap();
            delay.delay_millis(board::SETTLE_MS);
            let dac_rb = mcp4725::read_code(&mut i2c).unwrap_or(0);

            let raw = read_adc_averaged(&mut adc, &mut adc_pin);
            let lin_mv = board::linear_mv(raw);
            let cali_mv = calibrate_raw(raw, adc_channel);
            let cali_mv_i = cali_mv.map(|v| v as i32).unwrap_or(-1);
            let err_mv = cali_mv
                .map(|v| v as i32 - target_mv as i32)
                .unwrap_or(lin_mv as i32 - target_mv as i32);

            esp_println::println!(
                "step={}/{} dac={dac_code} dac_rb={dac_rb} target_mv={target_mv} raw={raw} cali_mv={cali_mv_i} lin_mv={lin_mv} err_mv={err_mv} adc_gpio={} unit=1 chan={adc_channel} scheme={}",
                step + 1,
                board::SWEEP_STEPS,
                board::ADC_GPIO,
                board::CALIB_SCHEME
            );
        }

        esp_println::println!("Sweep complete, restarting in 3 s");
        delay.delay_millis(3000);
    }
}
