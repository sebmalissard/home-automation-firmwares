#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

#include "Credentials.h"

// NVM CONFIG
// Uncomment to write NVM config
//#define WRITE_NVM_CONFIG
//#define SERIAL_NUMBER 1
//#define ROOM_NAME  "room1"
//#define ROOM_NAME  "bedroom"
//#define ROOM_NAME  "kitchen"
//#define ROOM_NAME  "bathroom"

// Firmware version
#define VERSION "1.2.0"

// PINOUT
#define PIN_SERIAL_TX_DEBUG   1
#define PIN_DHT22_DATA        3 // Serial RX (for debug only)
#define PIN_RADIATOR_CTRL_NEG 2 // Builtin LED
#define PIN_RADIATOR_CTRL_POS 0

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define CTRL_ENABLE   LOW
#define CTRL_DISABLE  HIGH

// MQTT
#define MQTT_TOPIC_PREFIX                       "home/%s"  // %s replaced by ROOM_NAME
#define MQTT_TOPIC_RAD_FIRMWARE_VERSION         "/radiator/firmware_version"
#define MQTT_TOPIC_RAD_SERIAL_NUMBER            "/radiator/serial_number"
#define MQTT_TOPIC_RAD_SENSOR                   "/radiator/sensor"
#define MQTT_TOPIC_RAD_GET_FIRMWARE_VERSION     "/radiator/get_firmware_version"
#define MQTT_TOPIC_RAD_GET_SERIAL_NUMBER        "/radiator/get_serial_number"
#define MQTT_TOPIC_RAD_SET_SWITCH               "/radiator/switch" // Todo need to rename topic with 'set'
#define MQTT_TOPIC_RAD_SET_TEMPERATURE_OFFSET   "/radiator/set_temperature_offset"
#define MQTT_TOPIC_RAD_SET_HUMIDITY_OFFSET      "/radiator/set_humidity_offset"
#define MQTT_MSG_BUFFER_SIZE                    (50)

// DHT22
#define DHT_PIN   PIN_DHT22_DATA
#define DHT_TYPE  DHT22

// Wifi
#define WIFI_HOSTNAME "%s-radiator" // %s replaced by ROOM_NAME

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
DHT dht(DHT_PIN, DHT_TYPE);
char mqttMsg[MQTT_MSG_BUFFER_SIZE];
struct NVMConfig config = {};
String mqttTopicPrefix = "";

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

void setup_wifi()
{
  delay(10);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  char wifi_hostname[64] = {};
  snprintf(wifi_hostname, sizeof(wifi_hostname)-1, WIFI_HOSTNAME, config.roomName);

  WiFi.mode(WIFI_STA);
  WiFi.hostname(wifi_hostname);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
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

void mqtt_reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");

    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
 
    // Attempt to connect
    if (client.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD))
    {
      Serial.println("connected");
      Serial.println((mqttTopicPrefix + MQTT_TOPIC_RAD_GET_FIRMWARE_VERSION).c_str());
      client.subscribe((mqttTopicPrefix + MQTT_TOPIC_RAD_GET_FIRMWARE_VERSION).c_str());
      client.subscribe((mqttTopicPrefix + MQTT_TOPIC_RAD_GET_SERIAL_NUMBER).c_str());
      client.subscribe((mqttTopicPrefix + MQTT_TOPIC_RAD_SET_SWITCH).c_str());
      client.subscribe((mqttTopicPrefix + MQTT_TOPIC_RAD_SET_TEMPERATURE_OFFSET).c_str());
      client.subscribe((mqttTopicPrefix + MQTT_TOPIC_RAD_SET_HUMIDITY_OFFSET).c_str());
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup_mqtt()
{
  client.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
  client.setCallback(mqtt_callback);
  mqtt_reconnect();
}

void setup_dht()
{
  dht.begin();
}

void setup()
{ 
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

  char str[64] = {};
  snprintf(str, sizeof(str)-1, MQTT_TOPIC_PREFIX, config.roomName);
  mqttTopicPrefix = str;

  Serial.print("Mqtt topic prefix: ");
  Serial.println(mqttTopicPrefix);

  setup_wifi();
  randomSeed(micros());
  setup_mqtt();
  setup_dht();

  delay(1000);

  pinMode(PIN_RADIATOR_CTRL_NEG, OUTPUT);
  pinMode(PIN_RADIATOR_CTRL_POS, OUTPUT);

  // OFF = Hors gel
  digitalWrite(PIN_RADIATOR_CTRL_NEG, CTRL_ENABLE);
  digitalWrite(PIN_RADIATOR_CTRL_POS, CTRL_DISABLE);
}

void mqtt_callback(char* topic, byte* payload, unsigned int len)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  for (int i = 0; i<len; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  String topicStr(topic);

  if (topicStr == mqttTopicPrefix + MQTT_TOPIC_RAD_GET_FIRMWARE_VERSION) {
    snprintf (mqttMsg, MQTT_MSG_BUFFER_SIZE, "{\"firmware_version\":%s}", VERSION);
    Serial.print("Publish message: ");
    Serial.println(mqttMsg);
    client.publish((mqttTopicPrefix + MQTT_TOPIC_RAD_FIRMWARE_VERSION).c_str(), mqttMsg);
  }
  else if (topicStr == mqttTopicPrefix + MQTT_TOPIC_RAD_GET_SERIAL_NUMBER) {
    snprintf (mqttMsg, MQTT_MSG_BUFFER_SIZE, "{\"serial_number\":%d,}", config.deviceSerialNumber);
    Serial.print("Publish message: ");
    Serial.println(mqttMsg);
    client.publish((mqttTopicPrefix + MQTT_TOPIC_RAD_SERIAL_NUMBER).c_str(), mqttMsg);
  }
  else if (topicStr == mqttTopicPrefix + MQTT_TOPIC_RAD_SET_SWITCH) {
    if (strncmp((char*) payload, "ON", MIN(len, 2)) == 0)
    {
      Serial.println("Set radiator ON");
      digitalWrite(PIN_RADIATOR_CTRL_NEG, CTRL_DISABLE);
      digitalWrite(PIN_RADIATOR_CTRL_POS, CTRL_DISABLE);
    }
    else if (strncmp((char*) payload, "OFF", MIN(len, 3)) == 0)
    {
      Serial.println("Set radiator OFF");
      digitalWrite(PIN_RADIATOR_CTRL_NEG, CTRL_ENABLE);
      digitalWrite(PIN_RADIATOR_CTRL_POS, CTRL_DISABLE);
    }
    else
    {
      Serial.println("Invalid message receive!");
    }
  }
  else if (topicStr == mqttTopicPrefix + MQTT_TOPIC_RAD_SET_TEMPERATURE_OFFSET) {
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
  else if (topicStr == mqttTopicPrefix + MQTT_TOPIC_RAD_SET_HUMIDITY_OFFSET) {
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
}

void loop_temp()
{
  float humidity = dht.readHumidity();       // in %
  float temperature = dht.readTemperature(); // in Celsius

  if (isnan(humidity) || isnan(temperature))
  {
    Serial.println("Fail to read temperature or humidity from dht22 sensor");
    return;
  }

  if (humidity == 0 && temperature == 0)
  {
    Serial.println("Ignore zero value read from dht22 sensor");
    return;
  }

  humidity += config.sensorHumidityOffset;
  temperature += config.sensorTemperatureOffset;

  snprintf (mqttMsg, MQTT_MSG_BUFFER_SIZE, "{\"temperature\":%.1f,\"humidity\":%.1f}", temperature, humidity);

  Serial.print("Publish message: ");
  Serial.println(mqttMsg);
  client.publish((mqttTopicPrefix + MQTT_TOPIC_RAD_SENSOR).c_str(), mqttMsg);
}

void loop()
{
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
