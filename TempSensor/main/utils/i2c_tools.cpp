#include <stdio.h>
#include <esp_log.h>
#include <driver/i2c_master.h>

#include "i2c_tools.h"

static const char *TAG = "i2c_tools";

/**
 * Scans the 7-bit addresses 0x08..0x77 and prints those that respond.
 */
esp_err_t i2c_scan(const i2c_port_num_t i2c_port, const gpio_num_t sda_gpio, const gpio_num_t scl_gpio)
{
    esp_err_t err;
    i2c_master_bus_handle_t bus = NULL;
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = i2c_port;
    bus_cfg.sda_io_num = sda_gpio;
    bus_cfg.scl_io_num = scl_gpio;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;

    err = i2c_new_master_bus(&bus_cfg, &bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Create I2C master bus failed: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }
 
    int found = 0;
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
    for (int row = 0; row < 8; row++) {
        printf("%02x: ", row * 16);
        for (int col = 0; col < 16; col++) {
            uint8_t addr = row * 16 + col;
            if (addr < 0x08 || addr > 0x77) {
                printf("   ");
                continue;
            }
            // 50ms timeout for each address probe
            if (i2c_master_probe(bus, addr, 50) == ESP_OK) {
                printf("%02x ", addr);
                found++;
            } else {
                printf("-- ");
            }
        }
        printf("\n");
    }
    ESP_LOGI(TAG, "%d devices found)", found);
 
    i2c_del_master_bus(bus);
    return ESP_OK;
}
