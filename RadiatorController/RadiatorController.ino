#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

#include "Credentials.h"
#include "RadiatorMqtt.h"

// NVM CONFIG
// Uncomment to write NVM config
//#define WRITE_NVM_CONFIG
//#define SERIAL_NUMBER 1
//#define ROOM_NAME  "room1"
//#define ROOM_NAME  "bedroom"
//#define ROOM_NAME  "kitchen"
//#define ROOM_NAME  "bathroom"

// Firmware version
#define VERSION "2.0.0"

// PINOUT
#define PIN_SERIAL_TX_DEBUG   1
#define PIN_DHT22_DATA        3 // Serial RX (for debug only)
#define PIN_RADIATOR_CTRL_NEG 2 // Builtin LED
#define PIN_RADIATOR_CTRL_POS 0

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define CTRL_ENABLE   LOW
#define CTRL_DISABLE  HIGH

// DHT22
#define DHT_PIN   PIN_DHT22_DATA
#define DHT_TYPE  DHT22

// Wifi
#define WIFI_HOSTNAME "%s-radiator" // %s replaced by ROOM_NAME

enum PilotWireState {
  PILOT_WIRE_STATE_COMFORT,
  PILOT_WIRE_STATE_ECO,
  PILOT_WIRE_STATE_FROST_PROTECTION,
  PILOT_WIRE_STATE_OFF,
  PILOT_WIRE_STATE_COMFORT_MINUS_1,
  PILOT_WIRE_STATE_COMFORT_MINUS_2,
};

#pragma pack(1)
struct NVMConfig {
  float     sensorTemperatureOffset;
  float     sensorHumidityOffset;
  uint32_t  deviceSerialNumber;
  char      roomName[32];
};
static_assert(sizeof(struct NVMConfig) == 4+4+4+32, "EEPROM config structure size is incorrect");

WiFiClient espClient;
PubSubClient client(espClient);
RadiatorMqtt mqtt(client);
DHT dht(DHT_PIN, DHT_TYPE);
char mqttMsg[MQTT_MSG_BUFFER_SIZE];
struct NVMConfig config = {};
enum Power currentPower = POWER_OFF;
enum Mode currentMode = MODE_UNKNOWN;
enum PresetMode currentPresetMode = PRESET_MODE_UNKNOWN;

#ifdef WRITE_NVM_CONFIG
void write_nvm_config() {
  Serial.println("WRITE NVM CONFIG:");

  EEPROM.begin(sizeof(NVMConfig));

#ifdef SERIAL_NUMBER
  Serial.print(" - SERIAL_NUMBER: ");
  Serial.println(SERIAL_NUMBER);
  EEPROM.put(offsetof(NVMConfig, deviceSerialNumber), SERIAL_NUMBER);
#endif

#ifdef ROOM_NAME
  Serial.print(" - ROOM_NAME: ");
  Serial.println(ROOM_NAME);
  EEPROM.put(offsetof(NVMConfig, roomName), ROOM_NAME);
#endif

  EEPROM.end();
}
#endif

void setup_wifi() {
  delay(10);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  char wifi_hostname[64] = {};
  snprintf(wifi_hostname, sizeof(wifi_hostname)-1, WIFI_HOSTNAME, config.roomName);

  WiFi.mode(WIFI_STA);
  WiFi.hostname(wifi_hostname);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connecté");
  Serial.print("MAC : ");
  Serial.println(WiFi.macAddress());
  Serial.print("Adresse IP : ");
  Serial.println(WiFi.localIP());
}

void mqtt_reconnect() {
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");

    // Create a random client ID
    String clientId = "ESP8266Client-Radiator-";
    clientId += String(config.roomName);
    clientId += "-";
    clientId += String(config.deviceSerialNumber);

    // Attempt to connect
    if (client.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD, mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_AVAILABILITY), 1, true, MQTT_PAYLOAD_OFFLINE)) {
      Serial.println("connected");
      client.subscribe(MQTT_TOPIC_HOMEASSISTANT_STATUS);
      client.subscribe(mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_POWER_SET));
      client.subscribe(mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_MODE_SET));
      client.subscribe(mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_PRESET_MODE_SET));
      client.subscribe(mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_FIRMWARE_VERSION_GET));
      client.subscribe(mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_SERIAL_NUMBER_GET));
      client.subscribe(mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_TEMPERATURE_OFFSET_SET));
      client.subscribe(mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_HUMIDITY_OFFSET_SET));
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup_mqtt() {
  client.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
  client.setCallback(mqtt_callback);
  mqtt_reconnect();
}

void setup_dht() {
  dht.begin();
}

void setup() {
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);

  // For serial, don't miss any message
  delay(2000);

  Serial.println("----------------");
  Serial.println("Starting...");

#ifdef WRITE_NVM_CONFIG
  write_nvm_config();
#endif
  EEPROM.begin(sizeof(NVMConfig));
  EEPROM.get(0x00, config);
  EEPROM.end();

  if (isnan(config.sensorTemperatureOffset)) {
    config.sensorTemperatureOffset = 0;
  }

  if (isnan(config.sensorHumidityOffset)) {
    config.sensorHumidityOffset = 0;
  }

  Serial.print("Firmware version: ");
  Serial.println(VERSION);
  Serial.print("Temperature offset: ");
  Serial.println(config.sensorTemperatureOffset);
  Serial.print("Humidity offset: ");
  Serial.println(config.sensorHumidityOffset);
  Serial.print("Serial number: ");
  Serial.println(config.deviceSerialNumber);
  Serial.print("Room name: ");
  Serial.println(config.roomName);

  setup_wifi();
  randomSeed(micros());
  mqtt.setup(config.roomName);
  setup_mqtt();
  setup_dht();

  delay(1000);

  pinMode(PIN_RADIATOR_CTRL_NEG, OUTPUT);
  pinMode(PIN_RADIATOR_CTRL_POS, OUTPUT);

  // Off by default
  set_pilot_wire_state(PILOT_WIRE_STATE_FROST_PROTECTION);

  // Set device online
  mqtt.publishMessage(mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_AVAILABILITY), MQTT_PAYLOAD_ONLINE, true);
}

void set_pilot_wire_state(PilotWireState state) {
  switch(state) {
    case PILOT_WIRE_STATE_COMFORT:
      Serial.println("--> Set radiator pilot wire state comfort");
      digitalWrite(PIN_RADIATOR_CTRL_NEG, CTRL_DISABLE);
      digitalWrite(PIN_RADIATOR_CTRL_POS, CTRL_DISABLE);
      break;
    case PILOT_WIRE_STATE_ECO:
      Serial.println("--> Set radiator pilot wire state eco");
      digitalWrite(PIN_RADIATOR_CTRL_NEG, CTRL_ENABLE);
      digitalWrite(PIN_RADIATOR_CTRL_POS, CTRL_ENABLE);
      break;
    case PILOT_WIRE_STATE_FROST_PROTECTION:
      Serial.println("--> Set radiator pilot wire state frost protection");
      digitalWrite(PIN_RADIATOR_CTRL_NEG, CTRL_ENABLE);
      digitalWrite(PIN_RADIATOR_CTRL_POS, CTRL_DISABLE);
      break;
    case PILOT_WIRE_STATE_OFF:
      Serial.println("--> Set radiator pilot wire state off");
      digitalWrite(PIN_RADIATOR_CTRL_NEG, CTRL_DISABLE);
      digitalWrite(PIN_RADIATOR_CTRL_POS, CTRL_ENABLE);
      break;
    case PILOT_WIRE_STATE_COMFORT_MINUS_1:
    case PILOT_WIRE_STATE_COMFORT_MINUS_2:
    default:
      Serial.println("ERROR: Radiator pilote wire state is not supported!");
      return;
  }
}

void set_power(enum Power power) {
  if (power == POWER_UNKNOWN) {
    Serial.println("ERROR: Unknown power!");
    return;
  }
  currentPower = power;
  mqtt.publishMessage(power);
  switch (power) {
    case POWER_OFF:
      Serial.println("Set radiator power OFF");
      set_pilot_wire_state(PILOT_WIRE_STATE_OFF);
      mqtt.publishMessage(MODE_OFF);
      break;
    case POWER_ON:
      Serial.println("Set radiator power ON");
      set_mode(currentMode);
      break;
    default:
      Serial.println("ERROR: power is not supported!");
  }
}

void set_mode(enum Mode mode)
{
  if (mode == MODE_UNKNOWN) {
    Serial.println("ERROR: Unknown mode!");
    return;
  }
  currentMode = mode;
  if (currentPower != POWER_ON) {
    Serial.println("Radiator is not power on, cannot apply mode now");
    mqtt.publishMessage(MODE_OFF);
    return;
  }
  mqtt.publishMessage(mode);
  switch (mode) {
    case MODE_OFF:
      Serial.println("Set radiator mode off");
      set_pilot_wire_state(PILOT_WIRE_STATE_OFF);
      break;
    case MODE_HEAT:
    case MODE_AUTO:
      Serial.println("Set radiator mode heat/auto");
      set_preset_mode(currentPresetMode);
      break;
    default:
      Serial.println("ERROR: Radiator mode is not supported!");
      return;
  }
}

void set_preset_mode(enum PresetMode preset_mode) {
  if (preset_mode == PRESET_MODE_UNKNOWN) {
    Serial.println("ERROR: Unknown preset mode!");
    return;
  }
  currentPresetMode = preset_mode;
  mqtt.publishMessage(preset_mode);
  if (currentPower != POWER_ON) {
    Serial.println("Radiator is not power on, cannot apply preset mode now");
    return;
  }
  if (currentMode != MODE_HEAT && currentMode != MODE_AUTO) {
    Serial.println("Radiator is not mode heat/auto, cannot apply preset mode now");
    return;
  }
  switch (preset_mode) {
    case PRESET_MODE_COMFORT:
      Serial.println("Set radiator preset mode comfort");
      set_pilot_wire_state(PILOT_WIRE_STATE_COMFORT);
      break;
    case PRESET_MODE_ECO:
      Serial.println("Set radiator preset mode eco");
      set_pilot_wire_state(PILOT_WIRE_STATE_ECO);
      break;
    case PRESET_MODE_AWAY:
      Serial.println("Set radiator preset mode away");
      set_pilot_wire_state(PILOT_WIRE_STATE_FROST_PROTECTION);
      break;
    default:
      Serial.println("ERROR: Radiator mode is not supported!");
      return;
  }
}

void mqtt_callback(char* topic, byte* payload, unsigned int len) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  for (int i=0; i<len; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (isTopicEqual(topic, mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_FIRMWARE_VERSION_GET))) {
    mqtt.publishMessage(mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_FIRMWARE_VERSION), VERSION);
  }
  else if (isTopicEqual(topic, mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_SERIAL_NUMBER_GET))) {
    mqtt.publishMessage(mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_SERIAL_NUMBER), String(config.deviceSerialNumber).c_str());
  }
  else if (isTopicEqual(topic, mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_POWER_SET))) {
    set_power(getPowerFromMqttPayload((char*)payload, len));
  }
  else if (isTopicEqual(topic, mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_MODE_SET))) {
    set_mode(getModeFromMqttPayload((char*)payload, len));
  }
  else if (isTopicEqual(topic, mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_PRESET_MODE_SET))) {
    set_preset_mode(getPresetModeFromMqttPayload((char*)payload, len));
  }
  else if (isTopicEqual(topic, mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_TEMPERATURE_OFFSET_SET))) {
    char *endptr = nullptr;
    float val = strtof((char*)payload, &endptr);
    Serial.print("Set temperature offset to ");
    Serial.println(val);
    if ((char*)payload == endptr) {
      Serial.println("Invalid temperature offset value");
    }
    else if (val != config.sensorTemperatureOffset) {
      config.sensorTemperatureOffset = val;
      EEPROM.begin(sizeof(NVMConfig));
      EEPROM.put(offsetof(NVMConfig, sensorTemperatureOffset), config.sensorTemperatureOffset);
      EEPROM.end();
    }
  }
  else if (isTopicEqual(topic, mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_HUMIDITY_OFFSET_SET))) {
    char *endptr = nullptr;
    float val = strtof((char*)payload, &endptr);
    Serial.print("Set humidity offset to ");
    Serial.println(val);
    if ((char*)payload == endptr) {
      Serial.println("Invalid humidity offset value");
    }
    else if (val != config.sensorHumidityOffset) {
      config.sensorHumidityOffset = val;
      EEPROM.begin(sizeof(NVMConfig));
      EEPROM.put(offsetof(NVMConfig, sensorHumidityOffset), config.sensorHumidityOffset);
      EEPROM.end();
    }
  }
  else if (isTopicEqual(topic, MQTT_TOPIC_HOMEASSISTANT_STATUS)) {
    if (isPayloadEqual<MQTT_PAYLOAD_ONLINE>((char*) payload, len)) {
      Serial.println("Home Assistant is connected");
      mqtt.publishMessage(currentPower);
      mqtt.publishMessage(currentPower != POWER_ON ? MODE_OFF : currentMode);
      mqtt.publishMessage(currentPresetMode);
    }
  }
}

void loop_temp() {
  float humidity = dht.readHumidity();       // in %
  float temperature = dht.readTemperature(); // in Celsius

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Fail to read temperature or humidity from dht22 sensor");
    return;
  }

  if (humidity == 0 && temperature == 0) {
    Serial.println("Ignore zero value read from dht22 sensor");
    return;
  }

  humidity += config.sensorHumidityOffset;
  temperature += config.sensorTemperatureOffset;

  mqtt.publishMessage(mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_SENSOR_TEMPERATURE), temperature);
  mqtt.publishMessage(mqtt.getRadTopic(MQTT_TOPIC_RAD_SUFFIX_SENSOR_HUMIDITY), humidity);
}

void loop() {
  static unsigned long lastTime = 0;
  unsigned long currentTime = millis();
  
  mqtt_reconnect();
  client.loop();


  if (currentTime - lastTime > 5000) {
    lastTime = currentTime;
    loop_temp();
  }

  delay(500);
}
