#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

#include "Credentials.h"

// CONFIG SELECTION
// Uncomment mqtt topic to use
#define MQTT_TOPIC_PREFFIX      "home/room1"
//#define MQTT_TOPIC_PREFFIX      "home/kitchen"
//#define MQTT_TOPIC_PREFFIX      "home/bathroom"


// Firmware version
#define VERSION "1.0.2"

// PINOUT
#define PIN_SERIAL_TX_DEBUG   1
#define PIN_DHT22_DATA        3 // Serial RX (for debug only)
#define PIN_RADIATOR_CTRL_NEG 2 // Builtin LED
#define PIN_RADIATOR_CTRL_POS 0

#define EEPROM_TEMPERATURE_OFFSET_ADDR  0x00  // size 4 bytes
#define EEPROM_HUMIDITY_OFFSET_ADDR     0x04  // size 4 bytes
#define EEPROM_SIZE                     (8)

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define CTRL_ENABLE   LOW
#define CTRL_DISABLE  HIGH

// MQTT
#define MQTT_TOPIC_RAD_SENSOR                   MQTT_TOPIC_PREFFIX"/radiator/sensor"
#define MQTT_TOPIC_RAD_SWITCH                   MQTT_TOPIC_PREFFIX"/radiator/switch"
#define MQTT_TOPIC_RAD_SET_TEMPERATURE_OFFSET   MQTT_TOPIC_PREFFIX"/radiator/set_temperature_offset"
#define MQTT_TOPIC_RAD_SET_HUMIDITY_OFFSET      MQTT_TOPIC_PREFFIX"/radiator/set_humidity_offset"
#define MQTT_MSG_BUFFER_SIZE                    (50)

// DHT22
#define DHT_PIN   PIN_DHT22_DATA
#define DHT_TYPE  DHT22


WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHT_PIN, DHT_TYPE);
char mqttMsg[MQTT_MSG_BUFFER_SIZE];
float sensorTemperatureOffset = 0;
float sensorHumidityOffset = 0;

void setup_wifi()
{
  delay(10);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
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
      client.subscribe(MQTT_TOPIC_RAD_SWITCH);
      client.subscribe(MQTT_TOPIC_RAD_SET_TEMPERATURE_OFFSET);
      client.subscribe(MQTT_TOPIC_RAD_SET_HUMIDITY_OFFSET);
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
  setup_wifi();
  randomSeed(micros());
  setup_mqtt();
  setup_dht();

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_TEMPERATURE_OFFSET_ADDR, sensorTemperatureOffset);
  EEPROM.get(EEPROM_HUMIDITY_OFFSET_ADDR, sensorHumidityOffset);
  EEPROM.end();

  if (isnan(sensorTemperatureOffset)) {
    sensorTemperatureOffset = 0;
  }

  if (isnan(sensorHumidityOffset)) {
    sensorHumidityOffset = 0;
  }

  Serial.print("Firmware version: ");
  Serial.println(VERSION);
  Serial.print("Temperature offset: ");
  Serial.println(sensorTemperatureOffset);
  Serial.print("Humidity offset: ");
  Serial.println(sensorHumidityOffset);

  Serial.print("Mqtt topic prefix: ");
  Serial.println(MQTT_TOPIC_PREFFIX);

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

  if (strcmp(topic, MQTT_TOPIC_RAD_SWITCH) == 0) {
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
  else if (strcmp(topic, MQTT_TOPIC_RAD_SET_TEMPERATURE_OFFSET) == 0) {
    char *endptr = nullptr;
    float val = strtof((char*)payload, &endptr);
    Serial.print("Set temperature offset to ");
    Serial.println(val);
    if ((char*)payload == endptr) {
      Serial.println("Invalid temperature offset value");
    }
    else if (val != sensorTemperatureOffset) {
      sensorTemperatureOffset = val;
      EEPROM.begin(EEPROM_SIZE);
      EEPROM.put(EEPROM_TEMPERATURE_OFFSET_ADDR, sensorTemperatureOffset);
      EEPROM.end();
    }
  }
  else if (strcmp(topic, MQTT_TOPIC_RAD_SET_HUMIDITY_OFFSET) == 0) {
    char *endptr = nullptr;
    float val = strtof((char*)payload, &endptr);
    Serial.print("Set humidity offset to ");
    Serial.println(val);
    if ((char*)payload == endptr) {
      Serial.println("Invalid humidity offset value");
    }
    else if (val != sensorHumidityOffset) {
      sensorHumidityOffset = val;
      EEPROM.begin(EEPROM_SIZE);
      EEPROM.put(EEPROM_HUMIDITY_OFFSET_ADDR, sensorHumidityOffset);
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

  humidity += sensorHumidityOffset;
  temperature += sensorTemperatureOffset;

  snprintf (mqttMsg, MQTT_MSG_BUFFER_SIZE, "{\"temperature\":%.1f,\"humidity\":%.1f}", temperature, humidity);

  Serial.print("Publish message: ");
  Serial.println(mqttMsg);
  client.publish(MQTT_TOPIC_RAD_SENSOR, mqttMsg);
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
