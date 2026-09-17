#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t dev;
    uint8_t i2c_addr;
} mcp4725_handle_t;

esp_err_t mcp4725_init(int scl_gpio, int sda_gpio, int i2c_freq_hz, uint8_t i2c_addr, mcp4725_handle_t *out);

esp_err_t mcp4725_set_code(mcp4725_handle_t *handle, uint16_t code);

esp_err_t mcp4725_get_code(mcp4725_handle_t *handle, uint16_t *code);

esp_err_t mcp4725_probe(mcp4725_handle_t *handle);

uint32_t mcp4725_code_to_target_mv(uint16_t code, uint32_t vref_mv);

#ifdef __cplusplus
}
#endif
