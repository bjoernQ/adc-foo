idf_component_register(SRCS "adc_tester_main.c" "adc_tester_board.c" "mcp4725.c"
                    PRIV_REQUIRES esp_adc esp_driver_i2c efuse freertos
                    INCLUDE_DIRS ".")
