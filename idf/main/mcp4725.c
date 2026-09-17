#include "mcp4725.h"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "mcp4725";

#define MCP4725_TIMEOUT_MS 1000
#define MCP4725_SETTLE_MS  5

static esp_err_t mcp4725_write_fast(mcp4725_handle_t *handle, uint16_t code)
{
    /* Fast mode: [C2:C0=000 + D11:D8][D7:D0] — no command prefix byte */
    uint8_t buf[2] = {
        (uint8_t)((code >> 8) & 0x0F),
        (uint8_t)(code & 0xFF),
    };
    return i2c_master_transmit(handle->dev, buf, sizeof(buf), MCP4725_TIMEOUT_MS);
}

static esp_err_t mcp4725_write_register(mcp4725_handle_t *handle, uint16_t code)
{
    /* Write DAC register: [0x40][D11:D4][D3:D0 << 4] — 3 bytes per datasheet */
    uint8_t buf[3] = {
        0x40,
        (uint8_t)(code >> 4),
        (uint8_t)((code & 0x0F) << 4),
    };
    return i2c_master_transmit(handle->dev, buf, sizeof(buf), MCP4725_TIMEOUT_MS);
}

esp_err_t mcp4725_init(int scl_gpio, int sda_gpio, int i2c_freq_hz, uint8_t i2c_addr, mcp4725_handle_t *out)
{
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "null handle");

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = sda_gpio,
        .scl_io_num = scl_gpio,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &out->bus), TAG, "i2c bus init failed");

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = i2c_addr,
        .scl_speed_hz = i2c_freq_hz,
    };
    esp_err_t err = i2c_master_bus_add_device(out->bus, &dev_config, &out->dev);
    if (err != ESP_OK) {
        i2c_del_master_bus(out->bus);
        out->bus = NULL;
        return err;
    }

    out->i2c_addr = i2c_addr;
    return ESP_OK;
}

esp_err_t mcp4725_probe(mcp4725_handle_t *handle)
{
    ESP_RETURN_ON_FALSE(handle && handle->bus, ESP_ERR_INVALID_ARG, TAG, "invalid handle");
    return i2c_master_probe(handle->bus, handle->i2c_addr, MCP4725_TIMEOUT_MS);
}

esp_err_t mcp4725_set_code(mcp4725_handle_t *handle, uint16_t code)
{
    ESP_RETURN_ON_FALSE(handle && handle->dev, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    code &= 0x0FFF;

    esp_err_t err = mcp4725_write_fast(handle, code);
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(MCP4725_SETTLE_MS));

    uint16_t readback = 0;
    if (mcp4725_get_code(handle, &readback) == ESP_OK && readback == code) {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "fast write readback %u != %u, trying register write", readback, code);
    err = mcp4725_write_register(handle, code);
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(MCP4725_SETTLE_MS));
    return ESP_OK;
}

esp_err_t mcp4725_get_code(mcp4725_handle_t *handle, uint16_t *code)
{
    ESP_RETURN_ON_FALSE(handle && handle->dev && code, ESP_ERR_INVALID_ARG, TAG, "invalid arg");

    uint8_t data[3] = {0};
    ESP_RETURN_ON_ERROR(i2c_master_receive(handle->dev, data, sizeof(data), MCP4725_TIMEOUT_MS), TAG,
                        "read failed");

    /* Byte0=status, byte1=D11:D4, byte2=D3:D0 in upper nibble */
    *code = ((uint16_t)data[1] << 4) | (data[2] >> 4);
    return ESP_OK;
}

uint32_t mcp4725_code_to_target_mv(uint16_t code, uint32_t vref_mv)
{
    return ((uint32_t)(code & 0x0FFF) * vref_mv) / 4096;
}
