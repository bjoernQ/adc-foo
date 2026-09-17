#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "soc/soc_caps.h"

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
#include "esp_efuse_rtc_calib.h"
#endif

#include "adc_tester_board.h"
#include "mcp4725.h"

static const char *TAG = "ADC_TESTER";

#if SOC_ADC_ATTEN_NUM <= 1
#define ADC_TEST_ATTEN ADC_ATTEN_DB_0
#else
#define ADC_TEST_ATTEN ADC_ATTEN_DB_12
#endif

#define ADC_LINK_TEST_MIN_DELTA 800

static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten,
                                   adc_cali_handle_t *out_handle, const char **out_scheme)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;
    const char *scheme = "none";

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    {
        int efuse_ver = esp_efuse_rtc_calib_get_ver();
        ESP_LOGI(TAG, "ADC eFuse calib version: %d", efuse_ver);
    }

    if (!calibrated) {
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
            scheme = "curve_fitting";
        } else {
            ESP_LOGW(TAG, "curve_fitting init failed: %s", esp_err_to_name(ret));
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
            scheme = "line_fitting";
        } else {
            ESP_LOGW(TAG, "line_fitting init failed: %s", esp_err_to_name(ret));
        }
    }
#endif

    *out_handle = handle;
    if (out_scheme) {
        *out_scheme = scheme;
    }

    if (calibrated) {
        ESP_LOGI(TAG, "Calibration success (%s)", scheme);
    } else if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "ADC calibration unavailable: eFuse calib data not programmed on this chip");
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Calibration init failed: %s", esp_err_to_name(ret));
    }

    return calibrated;
}

static void adc_calibration_deinit(adc_cali_handle_t handle, const char *scheme)
{
    if (handle == NULL) {
        return;
    }

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (scheme && strcmp(scheme, "curve_fitting") == 0) {
        ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));
        return;
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (scheme && strcmp(scheme, "line_fitting") == 0) {
        ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
        return;
    }
#endif
}

static uint16_t dac_code_for_step(int step, int total_steps)
{
    if (total_steps <= 1) {
        return 0;
    }
    if (step >= total_steps - 1) {
        return 4095;
    }
    return (uint16_t)((step * 4095) / (total_steps - 1));
}

static int adc_read_averaged(adc_oneshot_unit_handle_t adc_handle, adc_channel_t channel, int samples)
{
    int sum = 0;
    int raw = 0;

    for (int i = 0; i < samples; i++) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, channel, &raw));
        sum += raw;
    }

    return sum / samples;
}

static int adc_linear_mv(int raw)
{
    const int max_raw = (1 << SOC_ADC_RTC_MAX_BITWIDTH) - 1;
    return (int)((int64_t)raw * CONFIG_ADC_TEST_VREF_MV / max_raw);
}

static esp_err_t dac_set_and_verify(mcp4725_handle_t *dac, uint16_t code, uint16_t *readback)
{
    ESP_RETURN_ON_ERROR(mcp4725_set_code(dac, code), TAG, "DAC write failed");
    vTaskDelay(pdMS_TO_TICKS(CONFIG_ADC_TEST_SETTLE_MS));
    ESP_RETURN_ON_ERROR(mcp4725_get_code(dac, readback), TAG, "DAC readback failed");

    if (*readback != code) {
        ESP_LOGW(TAG, "DAC readback mismatch: wrote=%u read=%u", code, *readback);
    }

    return ESP_OK;
}

static void run_link_self_test(mcp4725_handle_t *dac, adc_oneshot_unit_handle_t adc_handle,
                               adc_channel_t adc_channel, int adc_gpio)
{
    uint16_t readback = 0;
    int raw_min = 0;
    int raw_max = 0;

    ESP_LOGI(TAG, "Running DAC/ADC link self-test on GPIO%d ...", adc_gpio);

    ESP_ERROR_CHECK(dac_set_and_verify(dac, 0, &readback));
    raw_min = adc_read_averaged(adc_handle, adc_channel, CONFIG_ADC_TEST_SAMPLES);

    ESP_ERROR_CHECK(dac_set_and_verify(dac, 4095, &readback));
    raw_max = adc_read_averaged(adc_handle, adc_channel, CONFIG_ADC_TEST_SAMPLES);

    const int delta = abs(raw_max - raw_min);
    ESP_LOGI(TAG, "Link self-test: raw@dac=0 -> %d, raw@dac=4095 -> %d, delta=%d",
             raw_min, raw_max, delta);

    if (delta < ADC_LINK_TEST_MIN_DELTA) {
        ESP_LOGE(TAG, "ADC raw barely changed (delta=%d). Check wiring:", delta);
        ESP_LOGE(TAG, "  1) MCP4725 VOUT must connect to GPIO%d on %s (see boot banner)", adc_gpio,
                 CONFIG_IDF_TARGET);
        ESP_LOGE(TAG, "  2) MCP4725 VDD/GND must be common with the ESP");
        ESP_LOGE(TAG, "  3) Verify I2C SCL=GPIO%d SDA=GPIO%d and MCP4725 addr 0x%02X",
                 CONFIG_I2C_MASTER_SCL, CONFIG_I2C_MASTER_SDA, CONFIG_MCP4725_I2C_ADDR);
    } else {
        ESP_LOGI(TAG, "Link self-test passed (delta=%d)", delta);
    }
}

void app_main(void)
{
    const int adc_gpio = CONFIG_ADC_TEST_GPIO;
    const int i2c_scl = CONFIG_I2C_MASTER_SCL;
    const int i2c_sda = CONFIG_I2C_MASTER_SDA;
    const uint8_t mcp4725_addr = CONFIG_MCP4725_I2C_ADDR;
    const uint32_t vref_mv = CONFIG_ADC_TEST_VREF_MV;
    const int sweep_steps = CONFIG_ADC_TEST_SWEEP_STEPS;
    const int settle_ms = CONFIG_ADC_TEST_SETTLE_MS;
    const int samples = CONFIG_ADC_TEST_SAMPLES;

    adc_unit_t adc_unit = 0;
    adc_channel_t adc_channel = 0;
    ESP_ERROR_CHECK(adc_oneshot_io_to_channel(adc_gpio, &adc_unit, &adc_channel));

    mcp4725_handle_t dac = {0};
    ESP_ERROR_CHECK(mcp4725_init(i2c_scl, i2c_sda, CONFIG_I2C_MASTER_FREQUENCY, mcp4725_addr, &dac));

    esp_err_t probe_err = mcp4725_probe(&dac);
    if (probe_err != ESP_OK) {
        ESP_LOGE(TAG, "MCP4725 not found at 0x%02X: %s (check I2C wiring/address straps)",
                 mcp4725_addr, esp_err_to_name(probe_err));
    } else {
        ESP_LOGI(TAG, "MCP4725 detected at 0x%02X", mcp4725_addr);
    }

    adc_oneshot_unit_handle_t adc_handle = NULL;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = adc_unit,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_TEST_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, adc_channel, &chan_config));

    adc_cali_handle_t cali_handle = NULL;
    const char *calib_scheme = "none";
    bool do_calibration = adc_calibration_init(adc_unit, adc_channel, ADC_TEST_ATTEN, &cali_handle, &calib_scheme);

    adc_tester_wiring_t wiring = {
        .adc_gpio = adc_gpio,
        .i2c_scl_gpio = i2c_scl,
        .i2c_sda_gpio = i2c_sda,
        .mcp4725_addr = mcp4725_addr,
        .vref_mv = vref_mv,
        .adc_unit = adc_unit,
        .adc_channel = adc_channel,
        .atten = ADC_TEST_ATTEN,
        .calib_scheme = calib_scheme,
    };
    adc_tester_print_wiring_banner(&wiring);

    run_link_self_test(&dac, adc_handle, adc_channel, adc_gpio);

    while (1) {
        for (int step = 0; step < sweep_steps; step++) {
            const uint16_t dac_code = dac_code_for_step(step, sweep_steps);
            const uint32_t target_mv = mcp4725_code_to_target_mv(dac_code, vref_mv);
            uint16_t dac_readback = 0;

            ESP_ERROR_CHECK(mcp4725_set_code(&dac, dac_code));
            vTaskDelay(pdMS_TO_TICKS(settle_ms));
            ESP_ERROR_CHECK(mcp4725_get_code(&dac, &dac_readback));

            const int raw = adc_read_averaged(adc_handle, adc_channel, samples);
            int cali_mv = -1;
            if (do_calibration) {
                ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw, &cali_mv));
            }

            const int lin_mv = adc_linear_mv(raw);
            const int err_mv = (cali_mv >= 0) ? (cali_mv - (int)target_mv) : (lin_mv - (int)target_mv);

            ESP_LOGI(TAG,
                     "step=%d/%d dac=%u dac_rb=%u target_mv=%lu raw=%d cali_mv=%d lin_mv=%d err_mv=%d adc_gpio=%d unit=%d chan=%d scheme=%s",
                     step + 1, sweep_steps, dac_code, dac_readback, (unsigned long)target_mv, raw, cali_mv,
                     lin_mv, err_mv, adc_gpio, (int)adc_unit + 1, (int)adc_channel, calib_scheme);
        }

        ESP_LOGI(TAG, "Sweep complete, restarting in 3 s");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }

    adc_calibration_deinit(cali_handle, calib_scheme);
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc_handle));
}
