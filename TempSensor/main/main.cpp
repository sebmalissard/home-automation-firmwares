/*
Factory Data:
  Start ESP32 in bootlaoder mode.
  Backup:
    esptool --chip esp32c6 -b 115200 -p COM3 read-flash 0x3E0000 0x6000 fctry_backup.bin
  Generate (WSL):
    esp-matter-mfg-tool --vendor-id 0xFFF1 --product-id 0x8001 --target esp32c6 --vendor-name "Seb" --product-name "TempSensor" --hw-ver 1 --hw-ver-str "1.0" --serial-num "1001" --no-secure-cert-bin
  Write:
    esptool --chip esp32c6 -b 115200 -p COM3 write_flash 0x3E0000 .\out\fff1_8001\89ea4b87-5de9-43fa-9e21-15022167918f\89ea4b87-5de9-43fa-9e21-15022167918f-partition.bin

OTA Matter:
  Generate:
    python3 managed_components/espressif__esp_matter/connectedhomeip/connectedhomeip/src/app/ota_image_tool.py create -v 0xFFF1 -p 0x8001 -vn 2 -vs "0.2" -da sha256 build/TempSensor.bin build/TempSensor_0.2.ota
  Show:
    python3 managed_components/espressif__esp_matter/connectedhomeip/connectedhomeip/src/app/ota_image_tool.py show build/TempSensor_0.2.ota
*/

#include <stdio.h>
#include <cmath>

#include <freertos/FreeRTOS.h>

#include <esp_err.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_pm.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <nvs_flash.h>

#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_ota.h>
#include <data_model_provider/esp_matter_data_model_provider.h>
#include <app/clusters/temperature-measurement-server/TemperatureMeasurementCluster.h>
#include <app/clusters/relative-humidity-measurement-server/RelativeHumidityMeasurementCluster.h>

#include <platform/ESP32/OpenthreadLauncher.h>
#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>

#include <button_gpio.h>
#include <button_types.h>
#include <iot_button.h>

#include <i2cdev.h>
#include <bme680.h>

#include "common_macros.h"
#include "devices/Display.h"
#include "utils/i2c_tools.h"
#include "xiao_esp32c6.h"

// Button
static const gpio_num_t APP_BUTTON_GPIO = XIAO_ESP32C6_GPIO_D0;

// LED
static const gpio_num_t APP_LED_GPIO    = XIAO_ESP32C6_GPIO_LED;
static const int APP_LED_ACTIVE_LEVEL   = 0; // Active low

// I2C
static const gpio_num_t APP_I2C_SDA_GPIO = XIAO_ESP32C6_GPIO_I2C_SDA;
static const gpio_num_t APP_I2C_SCL_GPIO = XIAO_ESP32C6_GPIO_I2C_SCL;
static const i2c_port_num_t APP_I2C_PORT = XIAO_ESP32C6_I2C_PORT;
static const uint32_t APP_I2C_CLK_HZ = 400000;
static const uint8_t APP_I2C_DISPLAY_ADDR = 0x3C; // OLED 128x32 display 7 bits address
static const uint8_t APP_I2C_BME680_ADDR = 0x77; // BME680 7 bits address

// Display
static const uint64_t APP_DISPLAY_TIMEOUT_US = 20ULL * 1000 * 1000; // Display stays on 20 s after a button press

// Matter
static constexpr int16_t TEMP_REPORT_THRESHOLD = 10;        // repport accuracy 0.1 °C
static constexpr int16_t HUMIDITY_REPORT_THRESHOLD = 10;    // report accuracy 0.1 %

// Identify
static const uint64_t APP_LED_IDENTIFY_BLINK_MS = 250; // LED toggles every 250 ms while identifying
static const uint64_t APP_LED_IDENTIFY_PERIOD_MS = 30 * 1000; // LED toggles during 30 seconds maximum while identifying

// Log
static const char *TAG = "app_main";

// Global variables
button_handle_t g_button_handle = NULL;
static Display g_display;
static bme680_t g_bme680_sensor;
static esp_timer_handle_t g_led_identify_timer = NULL;// Periodic timer that toggles the LED while identifying
bool g_perform_factory_reset = false;
uint16_t g_temperature_endpoint_id = 0;
uint16_t g_humidity_endpoint_id = 0;
static bool g_led_identify_power_on = false;
static int32_t g_led_identify_toggles_cnt = -1;

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

constexpr auto k_timeout_seconds = 300;

#if CONFIG_ENABLE_ENCRYPTED_OTA
extern const char decryption_key_start[] asm("_binary_esp_image_encryption_key_pem_start");
extern const char decryption_key_end[] asm("_binary_esp_image_encryption_key_pem_end");

static const char *s_decryption_key = decryption_key_start;
static const uint16_t s_decryption_key_len = decryption_key_end - decryption_key_start;
#endif // CONFIG_ENABLE_ENCRYPTED_OTA

//
// Driver functions
//

esp_err_t led_set_power(bool on)
{
    esp_err_t err = ESP_OK;

    ESP_LOGI(TAG, "Setting LED power to %s", on ? "ON" : "OFF");

    gpio_hold_dis(APP_LED_GPIO);
    err = gpio_set_level(APP_LED_GPIO, on ? APP_LED_ACTIVE_LEVEL : !APP_LED_ACTIVE_LEVEL);
    gpio_hold_en(APP_LED_GPIO); // Keep state across deep sleep cycles
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LED GPIO level");
    }

    return err;
}

// Performs one BME680 measurement and returns the temperature (degC) and relative humidity (%).
esp_err_t get_measurements(float *temperature, float *humidity)
{
    bme680_values_float_t values;
    esp_err_t err = bme680_measure_float(&g_bme680_sensor, &values);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BME680 measurement failed: %s", esp_err_to_name(err));
        return err;
    }

    *temperature = values.temperature;
    *humidity = values.humidity;
    return ESP_OK;
}

// Updates the Matter TemperatureMeasurement::MeasuredValue (in 0.01 degC).
// The cluster is code-driven: it owns the MeasuredValue and serves reads/subscriptions itself, so
// attribute::update() (esp-matter storage only) is not enough. Use the cluster setter instead,
// which also notifies subscribers.
esp_err_t set_matter_temperature(int16_t centi_celsius)
{
    static int16_t last_value = -10000;

    if (abs(centi_celsius - last_value) < TEMP_REPORT_THRESHOLD) {
        return ESP_OK; // No change
    }
    esp_matter::lock::ScopedChipStackLock stack_lock(portMAX_DELAY);

    chip::app::ServerClusterInterface *iface = esp_matter::data_model::provider::get_instance().registry().Get(
        chip::app::ConcreteClusterPath(g_temperature_endpoint_id, TemperatureMeasurement::Id));
    if (iface == nullptr) {
        return ESP_ERR_NOT_FOUND;
    }

    auto *cluster = static_cast<chip::app::Clusters::TemperatureMeasurementCluster *>(iface);
    CHIP_ERROR err = cluster->SetMeasuredValue(chip::app::DataModel::MakeNullable(centi_celsius));
    if (err != CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "Failed to set Matter temperature: %" CHIP_ERROR_FORMAT, err.Format());
    } else {
        last_value = centi_celsius;
    }

    return (err == CHIP_NO_ERROR) ? ESP_OK : ESP_FAIL;
}

// Updates the Matter RelativeHumidityMeasurement::MeasuredValue (in 0.01 %).
// Like temperature, the cluster is code-driven: use the cluster setter, not attribute::update().
esp_err_t set_matter_humidity(uint16_t centi_percent)
{
    static uint16_t last_value = -10000;

    if (abs(centi_percent - last_value) < HUMIDITY_REPORT_THRESHOLD) {
        return ESP_OK; // No change
    }
    esp_matter::lock::ScopedChipStackLock stack_lock(portMAX_DELAY);

    chip::app::ServerClusterInterface *iface = esp_matter::data_model::provider::get_instance().registry().Get(
        chip::app::ConcreteClusterPath(g_humidity_endpoint_id, RelativeHumidityMeasurement::Id));
    if (iface == nullptr) {
        return ESP_ERR_NOT_FOUND;
    }

    auto *cluster = static_cast<chip::app::Clusters::RelativeHumidityMeasurementCluster *>(iface);
    CHIP_ERROR err = cluster->SetMeasuredValue(chip::app::DataModel::MakeNullable(centi_percent));
    if (err != CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "Failed to set Matter humidity: %" CHIP_ERROR_FORMAT, err.Format());
    } else {
        last_value = centi_percent;
    }

    return (err == CHIP_NO_ERROR) ? ESP_OK : ESP_FAIL;
}

void identify_blink_start(int32_t toggles)
{
    if (g_led_identify_timer == NULL) {
        return;
    }
    g_led_identify_toggles_cnt = toggles;
    if (!esp_timer_is_active(g_led_identify_timer)) {
        g_led_identify_power_on = true;
        led_set_power(true);
        esp_err_t err = esp_timer_start_periodic(g_led_identify_timer, APP_LED_IDENTIFY_BLINK_MS * 1000);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start identify blink timer: %s", esp_err_to_name(err));
        }
    }
}

void identify_blink_stop()
{
    if (g_led_identify_timer == NULL) {
        return;
    }
    esp_timer_stop(g_led_identify_timer);
    g_led_identify_toggles_cnt = -1;
    g_led_identify_power_on = false;
    led_set_power(false);
}

//
// Callbacks
//

// Called by the button driver each time it goes back to power save mode (no key activity).
// With CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP the button GPIO wakes the chip through ext1, but the
// driver disables ext1 on the first press and never re-enables it: without this, only the first press
// can wake the chip from light sleep and later presses are only seen when the CPU happens to be awake.
static void button_enter_power_save_cb(void *usr_data)
{
    esp_sleep_enable_ext1_wakeup_io(1ULL << APP_BUTTON_GPIO, ESP_EXT1_WAKEUP_ANY_LOW);
}

static void button_app_toggle_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Toggle button pressed");
    g_display.onButtonPress();
}

static void button_factory_reset_pressed_cb(void *arg, void *data)
{
    if (!g_perform_factory_reset) {
        ESP_LOGI(TAG, "Factory reset triggered. Release the button to start factory reset.");
        g_perform_factory_reset = true;
    }
}

static void button_factory_reset_released_cb(void *arg, void *data)
{
    if (g_perform_factory_reset) {
        ESP_LOGI(TAG, "Starting factory reset");
        esp_matter::factory_reset();
        g_perform_factory_reset = false;
    }
}

static void led_identify_blink_timer_cb(void *arg)
{
    g_led_identify_power_on = !g_led_identify_power_on;
    g_led_identify_toggles_cnt--;

    if (g_led_identify_toggles_cnt < 0) {
        esp_timer_stop(g_led_identify_timer);
        g_led_identify_power_on = false;
        led_set_power(false);
    } else {
        led_set_power(g_led_identify_power_on);
    }
}

// This callback is called for every attribute update. The sensors only publish values from the firmware,
// so there is nothing to handle here: just return ESP_OK.
static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    return ESP_OK;
}

// This callback is invoked when clients interact with the Identify Cluster.
// The LED blinks while the endpoint is identifying (Identify command) or for ~3 s (TriggerEffect).
static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       uint8_t effect_variant, void *priv_data) {
    ESP_LOGI(TAG, "Identification callback: endpoint: %u, type: %u, effect: %u, variant: %u",
             endpoint_id, type, effect_id, effect_variant);

    switch (type) {
        case identification::START:
        case identification::EFFECT:
            identify_blink_start(APP_LED_IDENTIFY_PERIOD_MS/APP_LED_IDENTIFY_BLINK_MS);
            break;
        case identification::STOP:
            identify_blink_stop();
            break;
        default:
            break;
    }

    return ESP_OK;
}

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
        case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
            ESP_LOGI(TAG, "Interface IP Address changed");
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
            ESP_LOGI(TAG, "Commissioning complete");
            break;

        case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
            ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
            ESP_LOGI(TAG, "Commissioning session started");
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
            ESP_LOGI(TAG, "Commissioning session stopped");
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
            ESP_LOGI(TAG, "Commissioning window opened");
            break;

        case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
            ESP_LOGI(TAG, "Commissioning window closed");
            break;

        case chip::DeviceLayer::DeviceEventType::kFabricRemoved: {
            ESP_LOGI(TAG, "Fabric removed successfully");
            if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0) {
                chip::CommissioningWindowManager  &commissionMgr = chip::Server::GetInstance().GetCommissioningWindowManager();
                constexpr auto kTimeoutSeconds = chip::System::Clock::Seconds16(k_timeout_seconds);
                if (!commissionMgr.IsCommissioningWindowOpen()) {
                    /* After removing last fabric, this example does not remove the Wi-Fi credentials
                    * and still has IP connectivity so, only advertising on DNS-SD.
                    */
                    CHIP_ERROR err = commissionMgr.OpenBasicCommissioningWindow(kTimeoutSeconds,
                                                                                chip::CommissioningWindowAdvertisement::kDnssdOnly);
                    if (err != CHIP_NO_ERROR) {
                        ESP_LOGE(TAG, "Failed to open commissioning window, err:%" CHIP_ERROR_FORMAT, err.Format());
                    }
                }
            }
            break;
        }

        case chip::DeviceLayer::DeviceEventType::kFabricWillBeRemoved:
            ESP_LOGI(TAG, "Fabric will be removed");
            break;

        case chip::DeviceLayer::DeviceEventType::kFabricUpdated:
            ESP_LOGI(TAG, "Fabric is updated");
            break;

        case chip::DeviceLayer::DeviceEventType::kFabricCommitted:
            ESP_LOGI(TAG, "Fabric is committed");
            break;

        case chip::DeviceLayer::DeviceEventType::kBLEDeinitialized:
            ESP_LOGI(TAG, "BLE deinitialized and memory reclaimed");
            break;

        default:
            break;
    }
}

//
// Setup
//

// Enables automatic light sleep. CONFIG_PM_ENABLE alone does nothing until esp_pm_configure() is called:
// without it the CPU stays at full speed and never sleeps, even when the Thread device is a sleepy end device.
static void power_management_init()
{
#if CONFIG_PM_ENABLE && CONFIG_FREERTOS_USE_TICKLESS_IDLE
    esp_pm_config_t pm_config = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = 40, // ESP32-C6 XTAL frequency
        .light_sleep_enable = true,
    };
    esp_err_t err = esp_pm_configure(&pm_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure power management: %s", esp_err_to_name(err));
    }
#endif
}

void setup()
{
    esp_err_t err;

    /* Init the ESP NVS layer */
    nvs_flash_init();

    /* Debug: scan I2C */
    i2c_scan(XIAO_ESP32C6_I2C_PORT, XIAO_ESP32C6_GPIO_I2C_SDA, XIAO_ESP32C6_GPIO_I2C_SCL);

    /* Setup button */
    button_config_t button_config = {
        .long_press_time = 5000, // 5s (factory reset)
    };
    button_gpio_config_t button_gpio_config = {
        .gpio_num = APP_BUTTON_GPIO,
        .active_level = 0,
        .enable_power_save = true,
    };
    err = iot_button_new_gpio_device(&button_config, &button_gpio_config, &g_button_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button device");
    }

    /* Re-arm the ext1 wakeup each time the button driver goes back to power save (see callback) */
    button_power_save_config_t button_power_save_config = {
        .enter_power_save_cb = button_enter_power_save_cb,
        .usr_data = NULL,
    };
    err = iot_button_register_power_save_cb(&button_power_save_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register button power save callback");
    }

    /* Register button callback for app usage */
    err = iot_button_register_cb(g_button_handle, BUTTON_PRESS_DOWN, NULL, button_app_toggle_cb, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register button callback for app usage");
    }

    /* Register button callback for factory reset */
    err = iot_button_register_cb(g_button_handle, BUTTON_LONG_PRESS_HOLD, NULL, button_factory_reset_pressed_cb, NULL);
    err |= iot_button_register_cb(g_button_handle, BUTTON_PRESS_UP, NULL, button_factory_reset_released_cb, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register button callback for factory reset");
    }

    /* Setup LED */
    err = gpio_reset_pin(APP_LED_GPIO);
    err |= gpio_set_direction(APP_LED_GPIO, GPIO_MODE_OUTPUT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to setup LED");
    }
    led_set_power(false);

    /* Identify blink timer (periodic, started/stopped by the Identify cluster callback) */
    const esp_timer_create_args_t led_identify_timer_args = {
        .callback = led_identify_blink_timer_cb,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "led_identify_blink",
        .skip_unhandled_events = false,
    };
    err = esp_timer_create(&led_identify_timer_args, &g_led_identify_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create led identify blink timer: %s", esp_err_to_name(err));
    }

    /* Setup I2C bus.
     * The ESP32-C6 has a single I2C master port, shared by the BME680 and the display:
     * i2cdev (BME680 driver) creates the bus, then u8g2 reuses it through the shared handle. */
    i2cdev_init();

    err = bme680_init_desc(&g_bme680_sensor, APP_I2C_BME680_ADDR, (i2c_port_t)APP_I2C_PORT, APP_I2C_SDA_GPIO, APP_I2C_SCL_GPIO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init BME680 descriptor: %s", esp_err_to_name(err));
    }
    g_bme680_sensor.i2c_dev.cfg.sda_pullup_en = 1;
    g_bme680_sensor.i2c_dev.cfg.scl_pullup_en = 1;
    g_bme680_sensor.i2c_dev.cfg.master.clk_speed = APP_I2C_CLK_HZ;

    /* Probing the sensor makes i2cdev create the I2C master bus (even if the sensor is absent) */
     esp_err_t bme680_present = i2c_dev_check_present(&g_bme680_sensor.i2c_dev);
    if (bme680_present != ESP_OK) {
        ESP_LOGW(TAG, "BME680 not found at 0x%02X: %s", APP_I2C_BME680_ADDR, esp_err_to_name(bme680_present));
    }

    i2c_master_bus_handle_t i2c_bus = NULL;
    err = i2cdev_get_shared_handle((i2c_port_t)APP_I2C_PORT, (void **)&i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get shared I2C bus: %s", esp_err_to_name(err));
    } else {
        /* Setup display on the shared bus */
        Display::Config display_config;
        display_config.i2cBus = i2c_bus;
        display_config.i2cPort = APP_I2C_PORT;
        display_config.sdaGpio = APP_I2C_SDA_GPIO;
        display_config.sclGpio = APP_I2C_SCL_GPIO;
        display_config.i2cClkHz = APP_I2C_CLK_HZ;
        display_config.i2cAddr = APP_I2C_DISPLAY_ADDR;
        display_config.timeoutUs = APP_DISPLAY_TIMEOUT_US;
        err = g_display.init(display_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init display: %s", esp_err_to_name(err));
        }
    }

    /* Setup BME680 sensor */
    if (bme680_present == ESP_OK) {
        err = bme680_init_sensor(&g_bme680_sensor);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init BME680 sensor: %s", esp_err_to_name(err));
        } else {
            /* Temperature only: no gas measurement (no heater, faster and less self-heating) */
            bme680_use_heater_profile(&g_bme680_sensor, BME680_HEATER_NOT_USED);
        }
    }

    /* Set OpenThread platform config */
    esp_openthread_platform_config_t ot_config = {
        .radio_config = {
            .radio_mode = RADIO_MODE_NATIVE,
        },
        .host_config = {
            .host_connection_mode = HOST_CONNECTION_MODE_NONE,
        },
        .port_config = {
            .storage_partition_name = "nvs",
            .netif_queue_size = 10,
            .task_queue_size = 10,
        }
    };
    set_openthread_platform_config(&ot_config);
}

//
// Task functions
//

void update_measurements()
{
    float temperature = 0;
    float humidity = 0;
    if (get_measurements(&temperature, &humidity) != ESP_OK) {
        return;
    }

    ESP_LOGI(TAG, "Temperature: %.2f C, humidity: %.2f %%", temperature, humidity);
    g_display.updateTemp(temperature);
    g_display.updateHumidity(humidity);

    if (!std::isfinite(temperature) || temperature < -273.15f || temperature > 327.66f) {
        ESP_LOGE(TAG, "Temperature %.2f C is outside the Matter measurement range", temperature);
    } else {
        esp_err_t err = set_matter_temperature(static_cast<int16_t>(std::lround(temperature * 100.0f)));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to update Matter temperature: %s", esp_err_to_name(err));
        }
    }

    if (!std::isfinite(humidity) || humidity < 0.0f || humidity > 100.0f) {
        ESP_LOGE(TAG, "Humidity %.2f %% is outside the Matter measurement range", humidity);
    } else {
        esp_err_t err = set_matter_humidity(static_cast<uint16_t>(std::lround(humidity * 100.0f)));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to update Matter humidity: %s", esp_err_to_name(err));
        }
    }
}


//
// Main
//

extern "C" void app_main() 
{
    esp_err_t err = ESP_OK;
    endpoint_t *endpoint = nullptr;

    /* Hardware setup */
    setup();

    /* Create a Matter node and add the mandatory Root Node device type on endpoint 0 */
    node::config_t node_config;

    /* node handle can be used to add/modify other endpoints. */
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    ABORT_APP_ON_FAILURE(node != nullptr, ESP_LOGE(TAG, "Failed to create Matter node"));

    /* Expose the optional SerialNumber attribute on the Basic Information cluster.
     * The value is served at read time by the DeviceInstanceInfoProvider (fctry partition). */
    endpoint_t *root_endpoint = endpoint::get(node, 0);
    ABORT_APP_ON_FAILURE(root_endpoint != nullptr, ESP_LOGE(TAG, "Failed to get root endpoint"));
    cluster_t *basic_cluster = cluster::get(root_endpoint, chip::app::Clusters::BasicInformation::Id);
    ABORT_APP_ON_FAILURE(basic_cluster != nullptr, ESP_LOGE(TAG, "Failed to get Basic Information cluster"));
    attribute_t *serial_attr = cluster::basic_information::attribute::create_serial_number(basic_cluster, NULL, 0);
    ABORT_APP_ON_FAILURE(serial_attr != nullptr, ESP_LOGE(TAG, "Failed to create SerialNumber attribute"));

    /* Create a Matter temperature sensor endpoint */
    temperature_sensor::config_t temperature_config = {};
    temperature_config.temperature_measurement.measured_value = nullable<int16_t>();
    temperature_config.temperature_measurement.min_measured_value = nullable<int16_t>(-4000);
    temperature_config.temperature_measurement.max_measured_value = nullable<int16_t>(8500);
    endpoint = temperature_sensor::create(node, &temperature_config, ENDPOINT_FLAG_NONE, nullptr);
    ABORT_APP_ON_FAILURE(endpoint != nullptr, ESP_LOGE(TAG, "Failed to create temperature sensor endpoint"));

    g_temperature_endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "Temperature sensor created with endpoint_id %d", g_temperature_endpoint_id);

    /* Create a Matter humidity sensor endpoint */
    humidity_sensor::config_t humidity_config = {};
    humidity_config.relative_humidity_measurement.measured_value = nullable<uint16_t>();
    humidity_config.relative_humidity_measurement.min_measured_value = nullable<uint16_t>(0);
    humidity_config.relative_humidity_measurement.max_measured_value = nullable<uint16_t>(10000);
    endpoint = humidity_sensor::create(node, &humidity_config, ENDPOINT_FLAG_NONE, nullptr);
    ABORT_APP_ON_FAILURE(endpoint != nullptr, ESP_LOGE(TAG, "Failed to create humidity sensor endpoint"));

    g_humidity_endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "Humidity sensor created with endpoint_id %d", g_humidity_endpoint_id);

    /* Matter start */
    err = esp_matter::start(app_event_cb);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to start Matter, err:%d", err));


#if CONFIG_ENABLE_ENCRYPTED_OTA
    err = esp_matter_ota_requestor_encrypted_init(s_decryption_key, s_decryption_key_len);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to initialized the encrypted OTA, err: %d", err));
#endif // CONFIG_ENABLE_ENCRYPTED_OTA

#if CONFIG_ENABLE_CHIP_SHELL
    esp_matter::console::diagnostics_register_commands();
    esp_matter::console::factoryreset_register_commands();
    esp_matter::console::attribute_register_commands();
#if CONFIG_OPENTHREAD_CLI
    esp_matter::console::otcli_register_commands();
#endif
    esp_matter::console::init();
#endif

    /* Enable automatic light sleep */
    power_management_init();

    while (true) {
        update_measurements();
        vTaskDelay(20000 / portTICK_PERIOD_MS);
    }
}
