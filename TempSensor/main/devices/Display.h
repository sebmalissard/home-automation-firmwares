#pragma once

#include <stdint.h>
#include <cmath>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <esp_err.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <driver/i2c_master.h>

#include <u8g2.h>
#include <esp32_hw_i2c.h>

// SSD1306 128x32 I2C display driven by u8g2.
// Starts powered down; a button press turns it on, then toggles between temperature and humidity.
// The display powers down automatically after Config::timeoutUs without a button press.
class Display {
public:
    struct Config {
        i2c_master_bus_handle_t i2cBus = nullptr;   // I2C bus already created (shared with other devices)
        i2c_port_num_t i2cPort = I2C_NUM_0;
        gpio_num_t sdaGpio = GPIO_NUM_NC;
        gpio_num_t sclGpio = GPIO_NUM_NC;
        uint32_t i2cClkHz = 400000;
        uint8_t i2cAddr = 0x3C;                     // 7 bits address
        uint64_t timeoutUs = 20ULL * 1000 * 1000;   // Display stays on this long after the last button press
    };

    Display() = default;
    Display(const Display &) = delete;
    Display &operator=(const Display &) = delete;

    // Creates the mutex/timer, sets up u8g2 and leaves the display powered down.
    esp_err_t init(const Config &config);

    // Stores the latest measurements. The display is only refreshed if it is on and showing that value.
    void updateTemp(float temperature);
    void updateHumidity(float humidity);

    // Button press handler:
    //  - display off -> powers it on (showing the temperature)
    //  - display on  -> toggles between temperature and humidity
    // In both cases the auto power-down timer is restarted.
    void onButtonPress();

private:
    enum class State {
        Error = -1,     // Not initialized or initialization failed
        Off = 0,
        Temp = 1,
        Humidity = 2,
    };

    static void offTimerCb(void *arg);
    void powerOff();
    void drawValue(float value, const char *unit);
    void drawCurrent();

    u8g2_t u8g2_ = {};
    u8g2_esp32_i2c_ctx_t i2cCtx_ = {};
    SemaphoreHandle_t mutex_ = nullptr;         // Protects the display (shared between button, timer and main tasks)
    esp_timer_handle_t offTimer_ = nullptr;     // One-shot timer that powers the display down
    uint64_t timeoutUs_ = 0;
    State state_ = State::Error;
    float lastTemperature_ = NAN;               // Last measured temperature, redrawn on wake/toggle
    float lastHumidity_ = NAN;                  // Last measured humidity, redrawn on wake/toggle
};
