#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

#include "Credentials.h"

// CONFIG SELECTION
// Uncomment mqtt topic to use
#define MQTT_TOPIC_PREFFIX      "home/room1"
//#define MQTT_TOPIC_PREFFIX      "home/kitchen"


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
#define MQTT_TOPIC_RAD_SENSOR   MQTT_TOPIC_PREFFIX"/radiator/sensor"
#define MQTT_TOPIC_RAD_SETPOINT MQTT_TOPIC_PREFFIX"/radiator/setpoint" //replace by switch
#define MQTT_MSG_BUFFER_SIZE    (50)

// DHT22
#define DHT_PIN   PIN_DHT22_DATA
#define DHT_TYPE  DHT22


WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHT_PIN, DHT_TYPE);
char mqttMsg[MQTT_MSG_BUFFER_SIZE];

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
      client.subscribe(MQTT_TOPIC_RAD_SETPOINT);
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

void loop_temp()
{
  float humidity = dht.readHumidity();          // in %
  float temperature = dht.readTemperature();    // in Celsius

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

  // TODO arrondir temp et humidité au 1/100

  snprintf (mqttMsg, MQTT_MSG_BUFFER_SIZE, "{\"temperature\":%f,\"humidity\":%f}", temperature, humidity);

  Serial.print("Publish message: ");
  Serial.println(mqttMsg);
  client.publish(MQTT_TOPIC_RAD_SENSOR, mqttMsg);
}

void loop()
{
  mqtt_reconnect();
  client.loop();
  loop_temp();

//TODO replace by millis()
  delay(5000);
}