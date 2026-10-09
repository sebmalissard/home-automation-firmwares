#include "Battery.h"

#include <esp_log.h>
#include <esp_adc/adc_cali_scheme.h>

static const char *TAG = "Battery";

// LiFePO4 resting voltage (V) -> charge (%), sorted by descending voltage
struct CurvePoint {
    float volts;
    float percent;
};

static const CurvePoint LIFEPO4_CURVE[] = {
    {3.60f, 100.0f},
    {3.40f, 90.0f},
    {3.33f, 70.0f},
    {3.28f, 40.0f},
    {3.20f, 20.0f},
    {3.00f, 5.0f},
    {2.80f, 0.0f},
};

esp_err_t Battery::init(const Config &config)
{
    m_config = config;

    adc_unit_t unit;
    esp_err_t err = adc_oneshot_io_to_channel(m_config.adcGpio, &unit, &m_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO %d is not an ADC pin: %s", (int)m_config.adcGpio, esp_err_to_name(err));
        return err;
    }

    adc_oneshot_unit_init_cfg_t unit_cfg = {};
    unit_cfg.unit_id = unit;
    err = adc_oneshot_new_unit(&unit_cfg, &m_adc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ADC unit: %s", esp_err_to_name(err));
        return err;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {};
    chan_cfg.atten = ADC_ATTEN_DB_12;
    chan_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
    err = adc_oneshot_config_channel(m_adc, m_channel, &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(err));
        return err;
    }

    adc_cali_curve_fitting_config_t cali_cfg = {};
    cali_cfg.unit_id = unit;
    cali_cfg.chan = m_channel;
    cali_cfg.atten = ADC_ATTEN_DB_12;
    cali_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
    err = adc_cali_create_scheme_curve_fitting(&cali_cfg, &m_cali);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ADC calibration scheme: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

esp_err_t Battery::readVoltage(float *volts)
{
    if (m_adc == nullptr || m_cali == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    int sum = 0;
    for (uint8_t i = 0; i < m_config.samples; i++) {
        int raw = 0;
        esp_err_t err = adc_oneshot_read(m_adc, m_channel, &raw);
        if (err != ESP_OK) {
            return err;
        }
        sum += raw;
    }

    int millivolts = 0;
    esp_err_t err = adc_cali_raw_to_voltage(m_cali, sum / m_config.samples, &millivolts);
    if (err != ESP_OK) {
        return err;
    }

    *volts = millivolts * m_config.dividerRatio / 1000.0f;
    return ESP_OK;
}

uint8_t Battery::voltageToPercent(float volts)
{
    constexpr size_t count = sizeof(LIFEPO4_CURVE) / sizeof(LIFEPO4_CURVE[0]);

    if (volts >= LIFEPO4_CURVE[0].volts) {
        return 100;
    }
    if (volts <= LIFEPO4_CURVE[count - 1].volts) {
        return 0;
    }

    for (size_t i = 1; i < count; i++) {
        if (volts >= LIFEPO4_CURVE[i].volts) {
            const CurvePoint &hi = LIFEPO4_CURVE[i - 1];
            const CurvePoint &lo = LIFEPO4_CURVE[i];
            float ratio = (volts - lo.volts) / (hi.volts - lo.volts);
            return (uint8_t)(lo.percent + ratio * (hi.percent - lo.percent) + 0.5f);
        }
    }
    return 0;
}
