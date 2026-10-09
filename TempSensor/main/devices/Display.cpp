#include "display.h"

#include <stdio.h>
#include <cmath>

#include <esp_log.h>

static const char *TAG = "display";

// Draws a value followed by a unit, centered. NaN is shown as "--".
// The caller must hold mutex_ and the display must be on.
void Display::drawValue(float value, const char *unit)
{
    char buf[16];
    if (std::isfinite(value)) {
        snprintf(buf, sizeof(buf), "%.1f %s", value, unit);
    } else {
        snprintf(buf, sizeof(buf), "--");
    }

    int w = u8g2_GetStrWidth(&u8g2_, buf);
    u8g2_ClearBuffer(&u8g2_);
    u8g2_DrawStr(&u8g2_, (128 - w) / 2, 31, buf);   // center, line baseline at 31
    u8g2_SendBuffer(&u8g2_);
}

// Draws whatever the current state asks for. The caller must hold mutex_.
void Display::drawCurrent()
{
    if (state_ == State::Temp) {
        drawValue(lastTemperature_, "\xb0""C");  // \xb0 = °
    } else if (state_ == State::Humidity) {
        drawValue(lastHumidity_, "%");
    }
}

// Powers the display down (called from the off timer).
void Display::powerOff()
{
    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (state_ != State::Off && state_ != State::Error) {
        u8g2_SetPowerSave(&u8g2_, 1);
        state_ = State::Off;
        ESP_LOGI(TAG, "Display off");
    }
    xSemaphoreGive(mutex_);
}

void Display::offTimerCb(void *arg)
{
    static_cast<Display *>(arg)->powerOff();
}

esp_err_t Display::init(const Config &config)
{
    if (config.i2cBus == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    state_ = State::Error;
    timeoutUs_ = config.timeoutUs;

    /* Auto power-down: mutex + one-shot timer */
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create display mutex");
        return ESP_ERR_NO_MEM;
    }
    const esp_timer_create_args_t timer_args = {
        .callback = offTimerCb,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "display_off",
        .skip_unhandled_events = false,
    };
    esp_err_t err = esp_timer_create(&timer_args, &offTimer_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create display off timer: %s", esp_err_to_name(err));
        return err;
    }

    /* u8g2 on the shared I2C bus */
    u8g2_esp32_i2c_config_t u8g2_cfg = U8G2_ESP32_I2C_CONFIG_DEFAULT();
    u8g2_cfg.i2c_port = config.i2cPort;
    u8g2_cfg.sda_pin = config.sdaGpio;
    u8g2_cfg.scl_pin = config.sclGpio;
    u8g2_cfg.clk_hz = config.i2cClkHz;
    u8g2_cfg.dev_addr_7bit = config.i2cAddr;
    i2cCtx_.cfg = u8g2_cfg;
    i2cCtx_.bus_handle = config.i2cBus;   // u8g2 reuses the bus created elsewhere
    i2cCtx_.initialized = 1;               // so it does not try to create its own
    err = u8g2_esp32_i2c_set_default_context(&i2cCtx_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to setup display I2C context: %s", esp_err_to_name(err));
        return err;
    }

    u8g2_Setup_ssd1306_i2c_128x32_univision_f(&u8g2_, U8G2_R0, u8x8_byte_esp32_hw_i2c, u8x8_gpio_and_delay_esp32_i2c);
    u8g2_InitDisplay(&u8g2_);
    u8g2_SetFont(&u8g2_, u8g2_font_logisoso28_tf);  // ~28 px height

    /* The display starts powered down: a button press turns it on for timeoutUs */
    u8g2_ClearBuffer(&u8g2_);
    u8g2_SendBuffer(&u8g2_);
    u8g2_SetPowerSave(&u8g2_, 1);
    state_ = State::Off;

    return ESP_OK;
}

void Display::updateTemp(float temperature)
{
    if (mutex_ == nullptr) {
        return;
    }
    xSemaphoreTake(mutex_, portMAX_DELAY);
    lastTemperature_ = temperature;
    if (state_ == State::Temp) {
        drawCurrent();
    }
    xSemaphoreGive(mutex_);
}

void Display::updateHumidity(float humidity)
{
    if (mutex_ == nullptr) {
        return;
    }
    xSemaphoreTake(mutex_, portMAX_DELAY);
    lastHumidity_ = humidity;
    if (state_ == State::Humidity) {
        drawCurrent();
    }
    xSemaphoreGive(mutex_);
}

void Display::onButtonPress()
{
    if (mutex_ == nullptr) {
        return;
    }

    xSemaphoreTake(mutex_, portMAX_DELAY);
    switch (state_) {
        case State::Off:
            u8g2_SetPowerSave(&u8g2_, 0);
            state_ = State::Temp;
            ESP_LOGI(TAG, "Display on (temperature)");
            break;
        case State::Temp:
            state_ = State::Humidity;
            ESP_LOGI(TAG, "Display toggled to humidity");
            break;
        case State::Humidity:
            state_ = State::Temp;
            ESP_LOGI(TAG, "Display toggled to temperature");
            break;
        case State::Error:
        default:
            xSemaphoreGive(mutex_);
            return;
    }
    drawCurrent();

    /* Restart the timer: every press extends the display time to a full timeout */
    esp_timer_stop(offTimer_); // Returns an error if not running, which is fine
    esp_err_t err = esp_timer_start_once(offTimer_, timeoutUs_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start display off timer: %s", esp_err_to_name(err));
    }
    xSemaphoreGive(mutex_);
}
