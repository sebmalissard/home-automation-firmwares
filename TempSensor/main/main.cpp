/*
Factory Data:
  Start ESP32 in bootlaoder mode.
  Backup:
    esptool --chip esp32c6 -b 115200 -p COM3 read-flash 0x3E0000 0x6000 fctry_backup.bin
  Generate (WSL):
    esp-matter-mfg-tool --vendor-id 0xFFF1 --product-id 0x8000 --target esp32c6 --vendor-name "Seb" --product-name "WaterHeater" --hw-ver 1 --hw-ver-str "1.0"   --serial-num "1" --no-secure-cert-bin
  Write:
    esptool --chip esp32c6 -b 115200 -p COM3 write_flash 0x3E0000 .\out\fff1_8000\6ef52815-cd6d-45ca-a35c-477ea6be8b16\6ef52815-cd6d-45ca-a35c-477ea6be8b16-partition.bin
*/

#include <stdio.h>

#include <esp_err.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <nvs_flash.h>

#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_ota.h>

#include <platform/ESP32/OpenthreadLauncher.h>
#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>

#include <button_gpio.h>
#include <button_types.h>
#include <iot_button.h>

#include <u8g2.h>
#include <esp32_hw_i2c.h>

#include "common_macros.h"
#include "utils/i2c_tools.h"
#include "xiao_esp32c6.h"

// Button
static const gpio_num_t APP_BUTTON_GPIO = XIAO_ESP32C6_GPIO_D10;

// LED
static const gpio_num_t APP_LED_GPIO    = XIAO_ESP32C6_GPIO_LED;
static const int APP_LED_ACTIVE_LEVEL   = 0; // Active low

// I2C
static const gpio_num_t APP_I2C_SDA_GPIO = XIAO_ESP32C6_GPIO_I2C_SDA;
static const gpio_num_t APP_I2C_SCL_GPIO = XIAO_ESP32C6_GPIO_I2C_SCL;
static const i2c_port_num_t APP_I2C_PORT = XIAO_ESP32C6_I2C_PORT;
static const uint32_t APP_I2C_CLK_HZ = 400000;
static const uint8_t APP_I2C_DISPLAY_ADDR = 0x3C; // OLED 128x32 display 7 bits address

// Log
static const char *TAG = "app_main";

// Global variables
button_handle_t g_button_handle = NULL;
u8g2_t g_u8g2_display;
static u8g2_esp32_i2c_ctx_t g_u8g2_display_i2c_ctx;
bool g_perform_factory_reset = false;
uint16_t g_light_endpoint_id = 0;

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

esp_err_t led_set_power(esp_matter_attr_val_t *val)
{
    esp_err_t err = ESP_OK;

    if (val->type != ESP_MATTER_VAL_TYPE_BOOLEAN) {
        ESP_LOGE(TAG, "Invalid value type for LED power: %d", val->type);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Setting light power to %s", val->val.b ? "ON" : "OFF");

    err = gpio_set_level(APP_LED_GPIO, val->val.b ? APP_LED_ACTIVE_LEVEL : !APP_LED_ACTIVE_LEVEL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LED GPIO level");
    }

    return err;
}

void set_display_temp(float temp) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f \xb0""C", temp);  // \xb0 = °

    int w = u8g2_GetStrWidth(&g_u8g2_display, buf);
    u8g2_ClearBuffer(&g_u8g2_display);
    u8g2_DrawStr(&g_u8g2_display, (128 - w) / 2, 31, buf);   // center, line baseline at 31
    u8g2_SendBuffer(&g_u8g2_display);
}

//
// Callbacks
//

static void button_app_toggle_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Toggle button pressed");
    uint16_t endpoint_id = g_light_endpoint_id;
    uint32_t cluster_id = OnOff::Id;
    uint32_t attribute_id = OnOff::Attributes::OnOff::Id;

    attribute_t *attribute = attribute::get(endpoint_id, cluster_id, attribute_id);

    esp_matter_attr_val_t val;
    attribute::get_val(attribute, &val);
    val.val.b = !val.val.b;
    attribute::update(endpoint_id, cluster_id, attribute_id, &val);
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

// This callback is called for every attribute update. The callback implementation shall
// handle the desired attributes and return an appropriate error code. If the attribute
// is not of your interest, please do not return an error code and strictly return ESP_OK.
static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    esp_err_t err = ESP_OK;

    if (type == PRE_UPDATE) {
        if (endpoint_id == g_light_endpoint_id && cluster_id == OnOff::Id && attribute_id == OnOff::Attributes::OnOff::Id) {
            err = led_set_power(val);
        }
    }

    return err;
}

// This callback is invoked when clients interact with the Identify Cluster.
// In the callback implementation, an endpoint can identify itself. (e.g., by flashing an LED or light).
static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       uint8_t effect_variant, void *priv_data) {
    ESP_LOGI(TAG, "Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
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
    };
    err = iot_button_new_gpio_device(&button_config, &button_gpio_config, &g_button_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button device");
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

    /* Setup display with u8g2 */
    u8g2_esp32_i2c_config_t u8g2_cfg = U8G2_ESP32_I2C_CONFIG_DEFAULT();
    u8g2_cfg.i2c_port = APP_I2C_PORT;
    u8g2_cfg.sda_pin = APP_I2C_SDA_GPIO;
    u8g2_cfg.scl_pin = APP_I2C_SCL_GPIO;
    u8g2_cfg.clk_hz = APP_I2C_CLK_HZ;
    u8g2_cfg.dev_addr_7bit = APP_I2C_DISPLAY_ADDR;
    g_u8g2_display_i2c_ctx.cfg = u8g2_cfg;
    err = u8g2_esp32_i2c_set_default_context(&g_u8g2_display_i2c_ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to setup display I2C context");
    }

    u8g2_Setup_ssd1306_i2c_128x32_univision_f(&g_u8g2_display, U8G2_R0, u8x8_byte_esp32_hw_i2c, u8x8_gpio_and_delay_esp32_i2c);
    u8g2_InitDisplay(&g_u8g2_display);
    u8g2_SetPowerSave(&g_u8g2_display, 0);
    u8g2_SetFont(&g_u8g2_display, u8g2_font_logisoso28_tf);  // ~28 px height


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
// Main
//

extern "C" void app_main() 
{
    esp_err_t err = ESP_OK;

    /* Hardware setup */
    setup();

    // Test display
    set_display_temp(12.3);

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

    /* Create an on/off light endpoint */
    on_off_light::config_t light_config = {};
    light_config.on_off.on_off = true;
    light_config.on_off_lighting.start_up_on_off = nullptr;
    endpoint_t *endpoint = on_off_light::create(node, &light_config, ENDPOINT_FLAG_NONE, nullptr);
    ABORT_APP_ON_FAILURE(endpoint != nullptr, ESP_LOGE(TAG, "Failed to create on/off light endpoint"));

    g_light_endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "Light created with endpoint_id %d", g_light_endpoint_id);

    /* Matter start */
    err = esp_matter::start(app_event_cb);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to start Matter, err:%d", err));

    /* Set initial light state */
    esp_matter_attr_val_t val;
    attribute_t *attribute = attribute::get(g_light_endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id);
    attribute::get_val(attribute, &val);
    led_set_power(&val);


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

    while (true) {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
