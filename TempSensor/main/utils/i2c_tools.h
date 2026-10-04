#pragma once

#include <esp_err.h>
#include <driver/gpio.h>
#include <driver/i2c_master.h>

esp_err_t i2c_scan(const i2c_port_num_t i2c_port, const gpio_num_t sda_gpio, const gpio_num_t scl_gpio);
