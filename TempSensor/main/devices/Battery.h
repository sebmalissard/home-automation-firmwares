#pragma once

#include <stdint.h>

#include <esp_err.h>
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>

// Battery voltage measurement through a resistor divider on an ADC1 pin.
// Percentage table is tuned for a LiFePO4 cell (3.6 V full, 2.8 V empty).
class Battery {
public:
    struct Config {
        gpio_num_t adcGpio = GPIO_NUM_NC;   // GPIO connected to the divider midpoint (must be an ADC1 pin)
        float dividerRatio = 2.0f;          // (R1 + R2) / R2
        uint8_t samples = 32;               // Number of raw readings averaged per measurement
    };

    esp_err_t init(const Config &config);

    // Returns the battery voltage in volts
    esp_err_t readVoltage(float *volts);

    // Maps a LiFePO4 resting voltage to a 0-100 % charge level (linear interpolation)
    static uint8_t voltageToPercent(float volts);

private:
    Config m_config;
    adc_oneshot_unit_handle_t m_adc = nullptr;
    adc_cali_handle_t m_cali = nullptr;
    adc_channel_t m_channel = ADC_CHANNEL_0;
};
