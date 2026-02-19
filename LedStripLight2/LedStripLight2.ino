#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>

#include "Credentials.h"
#include "LedMqtt.h"
#include "OtaUpdater.h"
#include "Logger.h"

// OTA
#define VERSION "1.1.0"
#define DEVICE  "LedStripLight2"

// Settings (TODO later move in NVM config)
#define LED_PIN   0
#define LED_NUM   330

// Wifi
#define WIFI_HOSTNAME "%s-%d-ledStrip" // %s replaced by ROOM_NAME, %d replaced by SERIAL_NUMBER

// Sunrise
#define SUNRISE_BRIGHTNESS_MAX  50
#define SUNRISE_PIXELS_NB       (SUNRISE_BRIGHTNESS_MAX * LED_NUM)

// TODO Move in lib
#pragma pack(1)
struct NVMConfig {
  float     sensorTemperatureOffset;
  float     sensorHumidityOffset;
  uint32_t  deviceSerialNumber;
  char      roomName[32];
};
static_assert(sizeof(struct NVMConfig) == 4+4+4+32, "EEPROM config structure size is incorrect");

WiFiClient wifiClient;
PubSubClient client(wifiClient);
LedMqtt mqtt(client);
OtaUpdater ota(DEVICE, VERSION);
struct NVMConfig config = {};
Adafruit_NeoPixel leds(LED_NUM, LED_PIN, NEO_GRB + NEO_KHZ800);

// LED
State gLedState = STATE_UNKNOWN;
State gSunriseState = STATE_OFF;
uint8_t gLedRed = 0;
uint8_t gLedGreen = 0;
uint8_t gLedBlue = 0;

// Sunrise Mode
unsigned long sunriseStartTimeMs = 0;
unsigned long sunriseDurationTimeMs = 1800000; // 30 min
uint16_t sunriseCurrentLevel = 0;
uint16_t sunriseCurrentLedsInLevel = 0;

void setup_wifi() {
  delay(10);

  Log.info("Connecting to %s", WIFI_SSID);

  char wifi_hostname[64] = {};
  snprintf(wifi_hostname, sizeof(wifi_hostname)-1, WIFI_HOSTNAME, config.roomName, config.deviceSerialNumber);

  WiFi.mode(WIFI_STA);
  WiFi.hostname(wifi_hostname);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Log.debug(".");
  }

  Log.info("WiFi connecté");
  Log.info("MAC: %s", WiFi.macAddress());
  Log.info("Adresse IP: %s", WiFi.localIP());
}

void setup_mqtt() {
  client.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
  client.setCallback(mqtt_callback);
  mqtt_reconnect();
}

void mqtt_reconnect() {
  // Loop until we're reconnected
  while (!client.connected())
  {
    Log.info("Attempting MQTT connection...");

    // Create a random client ID
    String clientId = "ESP8266Client-LedStrip-";
    clientId += String(config.roomName);
    clientId += "-";
    clientId += String(config.deviceSerialNumber);

    // Attempt to connect
    if (client.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD, mqtt.getLedTopic(MQTT_TOPIC_LED_SUFFIX_AVAILABILITY), 1, true, MQTT_PAYLOAD_OFFLINE)) {
      Log.info("connected");
      client.subscribe(MQTT_TOPIC_HOMEASSISTANT_STATUS);
      client.subscribe(MQTT_TOPIC_OTA_CHECK_UPDATE);
      client.subscribe(mqtt.getLedTopic(MQTT_TOPIC_LED_SUFFIX_SUNRISE_SET));
      client.subscribe(mqtt.getLedTopic(MQTT_TOPIC_LED_SUFFIX_STATE_SET));
      client.subscribe(mqtt.getLedTopic(MQTT_TOPIC_LED_SUFFIX_RGB_SET));
      client.subscribe(mqtt.getLedTopic(MQTT_TOPIC_LED_SUFFIX_UPDATE_COMMAND));
      client.subscribe(Log.getMqttTopicLevel());
      // Set device online
      mqtt.publishMessage(mqtt.getLedTopic(MQTT_TOPIC_LED_SUFFIX_AVAILABILITY), MQTT_PAYLOAD_ONLINE, true);
      mqtt.publishMessageSwitchSuriseConfig();
      mqtt.publishMessageLightConfig();
      mqtt.publishMessageUpdateConfig();
      mqtt.publishMessageSensorRssiConfig();
    }
    else {
      Log.warning("failed, rc=%d try again in 5 seconds", client.state());
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);

  // For serial, don't miss any message
  delay(2000);

  Serial.println("----------------");
  Serial.println("Starting LedStripLight2...");

  // Read NVM
  EEPROM.begin(sizeof(NVMConfig));
  EEPROM.get(0x00, config);
  EEPROM.end();

  Log.setup(&client, config.roomName, "led");

  // Device info
  Log.info("Firmware version: %s\n", VERSION);
  Log.info("Serial number: %d\n", config.deviceSerialNumber);
  Log.info("Room name: %s\n", config.roomName);

  setup_wifi();
  randomSeed(micros());
  mqtt.setup(config.roomName, config.deviceSerialNumber, VERSION, WiFi.macAddress().c_str());
  setup_mqtt();

  delay(1000);

  // LED
  leds.begin();
  leds.clear();

  // Init
  setSunriseState(STATE_OFF);
}

void setLedWS2812(uint8_t r, uint8_t g, uint8_t b) {
  for (int i=0; i<LED_NUM; i++){
    leds.setPixelColor(i, r, g, b);
  }
  leds.show();
}

void setLedColorRGB(uint8_t red, uint8_t green, uint8_t blue) {
  static uint8_t prevRed = 0;
  static uint8_t prevGreen = 0;
  static uint8_t prevBlue = 0;

  if (prevRed != red || prevGreen != green || prevBlue != blue) {
    Log.info("Setting LED color to > r: %u  g: %u  b: %u\n", red, green, blue);

    setLedWS2812(red, green, blue);
    mqtt.publishMessage(mqtt.getLedTopic(MQTT_TOPIC_LED_SUFFIX_RGB), red, green, blue);

    prevRed = red;
    prevGreen = green;
    prevBlue = blue;
  }
}

void setSunriseState(enum State state) {
  gSunriseState = state;
  Log.info("Set Switch Sunrise Mode to %s", getMqttPayload(gSunriseState));
  mqtt.publishMessage(mqtt.getLedTopic(MQTT_TOPIC_LED_SUFFIX_SUNRISE), gSunriseState);
}

void setLedState(enum State state) {
  gLedState = state;
  Log.info("Set Switch LED Mode to %s", getMqttPayload(gLedState));
  mqtt.publishMessage(mqtt.getLedTopic(MQTT_TOPIC_LED_SUFFIX_STATE), gLedState);
}

/**
 * Calculates a smooth sunrise coefficient using a cosine S-curve 
 * combined with gamma correction for human eye perception.
 * * @param progression: A value from 0.0 (start) to 1.0 (end).
 * @return: A normalized intensity coefficient between 0.0 and 1.0.
 */
float getSunriseIntensity(float progress) {
  // Constrain input to ensure it stays within the [0.0, 1.0] range
  if (progress < 0.0) progress = 0.0;
  if (progress > 1.0) progress = 1.0;

  // Only first part of the curbe is used
  progress = progress / 2;

  // 1. Cosine curve: Creates a smooth S-curve (Ease-in-Out) 
  // to prevent abrupt speed changes at the start and end.
  float cosinus = 0.5 * (1.0 - cos(progress * PI));
  
  // 2. Gamma Correction: Adjusts the linear signal to match 
  // the non-linear human perception of brightness.
  return pow(cosinus, 2.2) * 2;
}

void ledSunriseLoop() {
  unsigned long currentTime = millis();
  float progress = (float)(currentTime-sunriseStartTimeMs) / (float)sunriseDurationTimeMs;
  float sunrise_intensity = getSunriseIntensity(progress);
  uint32_t pixels_active = SUNRISE_PIXELS_NB * sunrise_intensity;
  uint16_t level = pixels_active / LED_NUM;
  uint16_t leds_in_level = pixels_active % LED_NUM;

  if (progress > 1.0) {
    Log.info("SUNRISE finish");
    setSunriseState(STATE_OFF);
    setLedColorRGB(SUNRISE_BRIGHTNESS_MAX, SUNRISE_BRIGHTNESS_MAX, SUNRISE_BRIGHTNESS_MAX);
    return;
  }

  if (level != sunriseCurrentLevel || leds_in_level != sunriseCurrentLedsInLevel) {
    Log.debug("> SUNRISE: progress=%f, sunrise_intensity=%f, pixels_active=%d, level=%d, leds_next_level=%d", progress, sunrise_intensity, pixels_active, level, leds_in_level);
  
    for (int i=0; i<LED_NUM; i++) {
      if (i < leds_in_level) {
        leds.setPixelColor(i, level+1, level+1, level+1);
      } else {
        leds.setPixelColor(i, level, level, level);
      }
    }
    leds.show();

    if (level != sunriseCurrentLevel) {
      gLedRed = level;
      gLedGreen = level;
      gLedBlue = level;
      mqtt.publishMessage(mqtt.getLedTopic(MQTT_TOPIC_LED_SUFFIX_RGB), gLedRed, gLedGreen, gLedBlue);
    }
    sunriseCurrentLevel = level;
    sunriseCurrentLedsInLevel = leds_in_level;
  }
}

void ledColorLoop() {
  static State prevLedState = STATE_OFF;
  static State prevSunriseState = STATE_OFF;

  if (gLedState != prevLedState) {
    Log.info("LED %s", getMqttPayload(gLedState));
    mqtt.publishMessage(mqtt.getLedTopic(MQTT_TOPIC_LED_SUFFIX_STATE), gLedState);
  }

  if (prevSunriseState != gSunriseState) {
    Log.info("Switch Sunrise Mode %s", getMqttPayload(gSunriseState));
    mqtt.publishMessage(mqtt.getLedTopic(MQTT_TOPIC_LED_SUFFIX_SUNRISE), gSunriseState);
  }

  if (gSunriseState == STATE_ON) {
    if (prevSunriseState == STATE_OFF) { // Sunrise started
      Log.info("Sunrise mode started");
      setLedColorRGB(0, 0, 0);
      setLedState(STATE_ON);
      sunriseStartTimeMs = millis();
      sunriseCurrentLevel = 0;
      sunriseCurrentLedsInLevel = 0;
    } else { // Mode surise active
      ledSunriseLoop();
    }
  } else {
    if (gLedState == STATE_ON) {
      setLedColorRGB(gLedRed, gLedGreen, gLedBlue);
    } else {
      setLedColorRGB(0, 0, 0);
    }
  }

  prevLedState = gLedState;
  prevSunriseState = gSunriseState;
}

void mqtt_callback(char* topic, byte* payload, unsigned int len) {
  Log.debug("Message arrived [%s] %s", topic, Log.getString(payload, len).c_str());

  if (isTopicEqual(topic, mqtt.getLedTopic(MQTT_TOPIC_LED_SUFFIX_SUNRISE_SET))) {
    gSunriseState = getStateFromMqttPayload((char*)payload, len);
  }
  else if (isTopicEqual(topic, mqtt.getLedTopic(MQTT_TOPIC_LED_SUFFIX_STATE_SET))) {
    gLedState = getStateFromMqttPayload((char*)payload, len);
    gSunriseState = STATE_OFF;
  }
  else if (isTopicEqual(topic, mqtt.getLedTopic(MQTT_TOPIC_LED_SUFFIX_RGB_SET))) {
    getMqttPayload((char*)payload, len, &gLedRed, &gLedGreen, &gLedBlue);
    gSunriseState = STATE_OFF;
  }
  else if (isTopicEqual(topic, MQTT_TOPIC_HOMEASSISTANT_STATUS)) {
    if (isPayloadEqual<MQTT_PAYLOAD_ONLINE>((char*) payload, len)) {
      Log.info("Home Assistant is connected");
      // Publish current state unkown if reboot
    }
  }
  else if (isTopicEqual(topic, MQTT_TOPIC_OTA_CHECK_UPDATE)) {
    Log.debug("Check OTA update requested");
    StaticJsonDocument<256> json;
    DeserializationError error = deserializeJson(json, payload, len);
    if (error) {
      Log.error("OTA check update JSON error: %s", error.f_str());
    } else {
      const char* url = json["url"];
      if (ota.checkUpdate(url) < 0) {
        // Error occur, set no update
        mqtt.publishMessageUpdateState(VERSION);
      }
      else {
        mqtt.publishMessageUpdateState(ota.getExpectedVersion().c_str());
      }
    }
  }
  else if (isTopicEqual(topic, mqtt.getLedTopic(MQTT_TOPIC_LED_SUFFIX_UPDATE_COMMAND))) {
    Log.debug("OTA update command received");
    if (len == 7 && strncmp((char*)payload, "INSTALL", len) == 0) {
      Log.debug("Install command valid do update");
      mqtt.deleteMessageUpdateCommand();
      if (ota.getExpectedVersion() != VERSION) {
        mqtt.publishMessageUpdateState(ota.getExpectedVersion().c_str(), true);
        ota.doUpdate();
        mqtt.publishMessageUpdateState(ota.getExpectedVersion().c_str(), false);
      }
    }
  }
  else if (isTopicEqual(topic, Log.getMqttTopicLevel())) {
    Log.debug("Set MQTT log level");
    Log.setMqttLevel((char*)payload, len);
  }
}

void rssiRssi() {
  static unsigned long prevTime = 0;
  unsigned long currentTime = millis();

  if (currentTime - prevTime > 10000) {
    if (WiFi.status() == WL_CONNECTED) {
      long rssi = WiFi.RSSI();
      mqtt.publishMessage(mqtt.getLedTopic(MQTT_TOPIC_LED_SUFFIX_SENSOR_RSSI), rssi);
      prevTime = currentTime;
    }
  }
}

void loop() {

  mqtt_reconnect();
  client.loop();

  ledColorLoop();

  rssiRssi();
}
