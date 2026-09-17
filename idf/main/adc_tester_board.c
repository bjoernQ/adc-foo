#include "adc_tester_board.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "soc/soc_caps.h"

static const char *TAG = "ADC_TESTER";

static void append_gpio(char *buf, size_t buf_size, int gpio)
{
    size_t len = strlen(buf);
    if (len > 0 && len < buf_size - 1) {
        strncat(buf, ", ", buf_size - len - 1);
        len = strlen(buf);
    }
    char num[12];
    snprintf(num, sizeof(num), "GPIO%d", gpio);
    strncat(buf, num, buf_size - len - 1);
}

void adc_tester_print_wiring_banner(const adc_tester_wiring_t *wiring)
{
    char alt_pins[256] = {0};

    for (int gpio = SOC_GPIO_PIN_COUNT - 1; gpio >= 0; gpio--) {
        if (gpio == wiring->adc_gpio || gpio == wiring->i2c_scl_gpio || gpio == wiring->i2c_sda_gpio) {
            continue;
        }

        adc_unit_t unit = 0;
        adc_channel_t channel = 0;
        if (adc_oneshot_io_to_channel(gpio, &unit, &channel) != ESP_OK) {
            continue;
        }
        if (unit != ADC_UNIT_1) {
            continue;
        }

        append_gpio(alt_pins, sizeof(alt_pins), gpio);
    }

    const char *atten_str = "unknown";
    switch (wiring->atten) {
    case ADC_ATTEN_DB_0:
        atten_str = "0 dB";
        break;
    case ADC_ATTEN_DB_2_5:
        atten_str = "2.5 dB";
        break;
    case ADC_ATTEN_DB_6:
        atten_str = "6 dB";
        break;
    case ADC_ATTEN_DB_12:
        atten_str = "12 dB";
        break;
    default:
        break;
    }

    ESP_LOGI(TAG, "=== Wiring for %s ===", CONFIG_IDF_TARGET);
    ESP_LOGI(TAG, "  I2C SCL       : GPIO%d", wiring->i2c_scl_gpio);
    ESP_LOGI(TAG, "  I2C SDA       : GPIO%d", wiring->i2c_sda_gpio);
    ESP_LOGI(TAG, "  MCP4725 addr  : 0x%02X", wiring->mcp4725_addr);
    ESP_LOGI(TAG, "  MCP4725 VOUT  -> GPIO%d (ADC%d channel %d)", wiring->adc_gpio,
             (int)wiring->adc_unit + 1, (int)wiring->adc_channel);
    ESP_LOGI(TAG, "  Vref assumed  : %lu mV", (unsigned long)wiring->vref_mv);
    ESP_LOGI(TAG, "  Attenuation   : %s", atten_str);
    ESP_LOGI(TAG, "  Calib scheme  : %s", wiring->calib_scheme ? wiring->calib_scheme : "none");
    if (alt_pins[0] != '\0') {
        ESP_LOGI(TAG, "  Other ADC1 pins available (excl. I2C): %s", alt_pins);
    } else {
        ESP_LOGI(TAG, "  Other ADC1 pins available (excl. I2C): none");
    }
    ESP_LOGI(TAG, "=========================================");
}
