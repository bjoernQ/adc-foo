# ADC Tester (ESP-IDF)

Reference ESP-IDF application for comparing ADC calibration against a Rust implementation. An MCP4725 DAC drives known voltages over I2C; the ESP reads them back on an ADC pin and prints both raw and calibrated values.

## Hardware

Connect an MCP4725 breakout to your ESP board:

| Signal | GPIO | Notes |
|--------|------|-------|
| I2C SCL | **GPIO4** | Configurable in menuconfig |
| I2C SDA | **GPIO5** | Configurable in menuconfig |
| MCP4725 VOUT | **target-specific** | Connect to the ADC input pin for your chip |
| MCP4725 VDD | 3.3 V | Same rail as ESP |
| MCP4725 GND | GND | Common ground |

### Default ADC input GPIO per target

The ADC pin defaults vary by chip (GPIO4/GPIO5 are reserved for I2C):

| Target | Default ADC GPIO | ADC unit / channel |
|--------|-----------------|-------------------|
| ESP32 | GPIO34 | ADC1 ch6 |
| ESP32-C2 | GPIO3 | ADC1 ch3 |
| ESP32-C3 | GPIO3 | ADC1 ch3 |
| ESP32-C5 | GPIO6 | ADC1 ch5 |
| ESP32-C6 | GPIO6 | ADC1 ch6 |
| ESP32-C61 | GPIO3 | ADC1 ch1 |
| ESP32-H2 | GPIO3 | ADC1 ch2 |
| ESP32-H4 | GPIO28 | ADC1 ch0 |
| ESP32-P4 | GPIO16 | ADC1 ch0 |
| ESP32-S2 | GPIO7 | ADC1 ch6 |
| ESP32-S3 | GPIO7 | ADC1 ch6 |
| ESP32-S31 | GPIO48 | ADC1 ch6 |

**Always check the boot banner** after flashing — it prints the exact wiring for the target you built, including other available ADC1 pins.

## Build and flash

Requires ESP-IDF 6.1 with the environment activated.

```powershell
cd D:\projects\testing\ADC_TESTER\idf
idf.py set-target esp32s3
idf.py build flash monitor
```

Replace `esp32s3` with your chip. Re-run `set-target` when switching chips.

## Configuration

Run `idf.py menuconfig` → **ADC Tester Configuration**:

- I2C SCL/SDA GPIO and frequency
- MCP4725 I2C address (default `0x60`)
- ADC input GPIO (auto-defaults per target)
- Assumed Vref in mV (default 3300)
- Sweep step count, settling delay, sample averaging

## Serial output

On boot, a wiring banner is printed:

```
I (xxx) ADC_TESTER: === Wiring for esp32s3 ===
I (xxx) ADC_TESTER:   I2C SCL       : GPIO4
I (xxx) ADC_TESTER:   I2C SDA       : GPIO5
I (xxx) ADC_TESTER:   MCP4725 addr  : 0x60
I (xxx) ADC_TESTER:   MCP4725 VOUT  -> GPIO7 (ADC1 channel 6)
...
```

Each sweep step prints a parseable line:

```
I (xxx) ADC_TESTER: step=3/11 dac=818 dac_rb=818 target_mv=659 raw=812 cali_mv=655 lin_mv=654 err_mv=-4 adc_gpio=6 unit=1 chan=6 scheme=curve_fitting
```

| Field | Meaning |
|-------|---------|
| `dac` | 12-bit MCP4725 code written |
| `dac_rb` | Code read back over I2C (verifies DAC writes) |
| `target_mv` | Expected output based on Vref assumption |
| `raw` | Averaged ADC raw reading |
| `cali_mv` | ESP-IDF calibrated voltage (`-1` if calibration unavailable) |
| `lin_mv` | Linear estimate: `raw * Vref / max_raw` (fallback when cali unavailable) |
| `err_mv` | Uses `cali_mv` when available, otherwise `lin_mv`, minus `target_mv` |
| `scheme` | `curve_fitting`, `line_fitting`, or `none` |

### Troubleshooting flat `raw` readings

If `raw` stays constant while `dac` changes (e.g. stuck around 1680):

1. **Wrong ADC pin for your target** — on ESP32-C6 the default is **GPIO6**, not GPIO7. Check the boot banner.
2. **MCP4725 VOUT not connected** to the ADC GPIO — the link self-test at boot will warn if `raw@dac=0` and `raw@dac=4095` differ by less than ~800 counts.
3. **Check `dac_rb`** — if it tracks `dac`, I2C is fine and the issue is analog wiring.

### Calibration (`scheme=none`, `cali_mv=-1`)

On some chips (including many ESP32-C6 modules), factory ADC calibration eFuses are not programmed. This is a chip limitation, not an app bug. Use `lin_mv` as a rough reference, or compare raw values directly with Rust. The boot log prints the eFuse calib version.

## Rust comparison

Use this project as the ESP-IDF baseline. Mirror the same I2C pins, MCP4725 address, ADC GPIO (from the boot banner), attenuation, Vref, and sweep codes in your Rust app, then diff the `raw` and `cali_mv` columns per step.

Suggested Rust project location: [`../Rust/`](../Rust/) — same log format for side-by-side comparison.

## Reference

Based on the ESP-IDF 6.1 [adc/oneshot_read](https://github.com/espressif/esp-idf/tree/release/v6.1/examples/peripherals/adc/oneshot_read) example (successor to the v4.4 `single_read` API).
