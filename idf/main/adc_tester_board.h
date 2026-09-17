#pragma once

#include "esp_adc/adc_oneshot.h"
#include "hal/adc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int adc_gpio;
    int i2c_scl_gpio;
    int i2c_sda_gpio;
    uint8_t mcp4725_addr;
    uint32_t vref_mv;
    adc_unit_t adc_unit;
    adc_channel_t adc_channel;
    adc_atten_t atten;
    const char *calib_scheme;
} adc_tester_wiring_t;

void adc_tester_print_wiring_banner(const adc_tester_wiring_t *wiring);

#ifdef __cplusplus
}
#endif
