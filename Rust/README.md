# ADC Tester (Rust / esp-hal)

Rust counterpart to the ESP-IDF [`../idf/`](../idf/) project for comparing ADC calibration. Drives an MCP4725 over I2C, sweeps DAC codes, reads ADC raw + calibrated values, and prints the same log format as the IDF app.

Uses **local esp-hal** from [`D:/projects/esp/esp-hal`](D:/projects/esp/esp-hal) (path dependencies in `Cargo.toml`).

## Hardware

Same wiring as the IDF project:

| Signal | GPIO |
|--------|------|
| I2C SCL | GPIO4 |
| I2C SDA | GPIO5 |
| MCP4725 VOUT | target-specific ADC GPIO (GPIO6 on ESP32-C6) |
| MCP4725 addr | 0x60 |

See the IDF [README](../idf/README.md) for the full per-target ADC GPIO table.

## Build and flash

Default target: **ESP32-C6** (`riscv32imac-unknown-none-elf`).

```powershell
cd D:\projects\testing\ADC_TESTER\Rust
cargo build --release
cargo run --release
```

### Other chips

1. Enable the chip feature, e.g. `--features esp32s3`
2. Update `.cargo/config.toml`:
   - `[build] target` — `xtensa-esp32s3-none-elf` for S3, `riscv32imac-unknown-none-elf` for C6/C3/etc.
   - `runner` — `--chip esp32s3` (or matching chip)

Example for ESP32-S3:

```powershell
cargo build --release --features esp32s3
cargo run --release --features esp32s3
```

## Output format

Matches the IDF app for easy diffing:

```
step=3/11 dac=819 dac_rb=819 target_mv=659 raw=688 cali_mv=681 lin_mv=554 err_mv=22 adc_gpio=6 unit=1 chan=6 scheme=curve_fitting
```

| Field | Rust source |
|-------|-------------|
| `raw` | Uncalibrated ADC counts (`enable_pin` without cal scheme) |
| `cali_mv` | `AdcCalCurve` or `AdcCalLine` applied to raw (`-1` on ESP32-S31) |
| `scheme` | `curve_fitting` (C3/C5/C6/C61/H2/P4/S3), `line_fitting` (ESP32/C2/S2), `none` (S31) |

## Calibration comparison notes

- ESP-IDF uses `adc_cali_raw_to_voltage()` (curve or line fitting in IDF 6.1).
- Rust applies `AdcCalCurve::adc_val()` or `AdcCalLine::adc_val()` to the **raw** reading, matching esp-hal's internal path but exposing raw separately for comparison.
- If `raw` matches IDF but `cali_mv` differs, focus on the Rust calibration implementation in `esp-hal/src/analog/adc/calibration/`.

## Supported targets

Features mirror the IDF app (except ESP32-H4, not yet in esp-hal):

`esp32`, `esp32c2`, `esp32c3`, `esp32c5`, `esp32c6`, `esp32c61`, `esp32h2`, `esp32p4`, `esp32s2`, `esp32s3`, `esp32s31`
