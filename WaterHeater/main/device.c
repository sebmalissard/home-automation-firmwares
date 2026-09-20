#include <driver/gpio.h>

#include "device.h"


#define LED_GPIO_PIN GPIO_NUM_15
#define BUTTON_GPIO_PIN GPIO_NUM_9

led_driver_config_t led_driver_get_config()
{
    led_driver_config_t config = {
        .gpio = LED_GPIO_PIN,
    };
    return config;
}

button_gpio_config_t button_driver_get_config()
{
    button_gpio_config_t config = {
        .gpio_num = BUTTON_GPIO_PIN,
        .active_level = 0,
    };
    return config;
}