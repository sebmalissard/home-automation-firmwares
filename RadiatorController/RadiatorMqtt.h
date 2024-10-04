#pragma once

#include <PubSubClient.h>

constexpr size_t MQTT_TOPIC_MAX_SIZE  = 64;
constexpr size_t MQTT_MSG_BUFFER_SIZE = 50;

/* MQTT TOPIC */
constexpr const char* MQTT_TOPIC_RAD_PREFIX                          = "home/%s/radiator";      // %s replaced by ROOM_NAME
// Home Assitant
constexpr const char* MQTT_TOPIC_HOMEASSISTANT_STATUS                = "homeassistant/status";  // ["online", "offline"]
// Home Assistant switch topics
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_POWER                    = "/power";                // ["OFF", "ON"]
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_POWER_SET                = "/power/set";            // ["OFF", "ON"]
// Home Assistant climate topics
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_MODE                     = "/mode";                 // ["off", "heat"]
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_MODE_SET                 = "/mode/set";             // ["off", "heat"]
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_PRESET_MODE              = "/preset_mode";          // ["comfort", "eco", "away"]
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_PRESET_MODE_SET          = "/preset_mode/set";      // ["comfort", "eco", "away"]
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_SENSOR_TEMPERATURE       = "/sensor/temperature";   // [float]
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_SENSOR_HUMIDITY          = "/sensor/humidity";      // [float]
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_ATTRIBUTES               = "/attributes";           // [json: name, serial_number , sw_version]
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_AVAILABILITY             = "/availibility";         // ["online", "offline"]
// Custom topics
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_FIRMWARE_VERSION         = "/firmware_version";
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_FIRMWARE_VERSION_GET     = "/firmware_version/get";
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_SERIAL_NUMBER            = "/serial_number";
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_SERIAL_NUMBER_GET        = "/serial_number/get";
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_TEMPERATURE_OFFSET_SET   = "/sensor/temperature_offset/set";
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_HUMIDITY_OFFSET_SET      = "/sensor/humidity_offset/set";

/* MQTT PAYPLOAD */
// Home Assitant
constexpr const char* MQTT_PAYLOAD_ONLINE  = "online";
constexpr const char* MQTT_PAYLOAD_OFFLINE = "offline";
// Power
constexpr const char* MQTT_PAYLOAD_POWER_UNKNOWN  = "unknown";
constexpr const char* MQTT_PAYLOAD_POWER_OFF      = "OFF";
constexpr const char* MQTT_PAYLOAD_POWER_ON       = "ON";
enum Power {
  POWER_UNKNOWN,
  POWER_OFF,
  POWER_ON,
};
// Mode
constexpr const char* MQTT_PAYLOAD_MODE_UNKOWN    = "unknown";
constexpr const char* MQTT_PAYLOAD_MODE_OFF       = "off";
constexpr const char* MQTT_PAYLOAD_MODE_HEAT      = "heat";
constexpr const char* MQTT_PAYLOAD_MODE_AUTO      = "auto";
enum Mode {
  MODE_UNKNOWN,
  MODE_OFF,
  MODE_HEAT,
  MODE_AUTO,
};
// Preset mode
constexpr const char* MQTT_PAYLOAD_PRESET_MODE_UNKNOWN  = "unknown";
constexpr const char* MQTT_PAYLOAD_PRESET_MODE_COMFORT  = "comfort";
constexpr const char* MQTT_PAYLOAD_PRESET_MODE_ECO      = "eco";
constexpr const char* MQTT_PAYLOAD_PRESET_MODE_AWAY     = "away";
enum PresetMode {
  PRESET_MODE_UNKNOWN,
  PRESET_MODE_COMFORT,
  PRESET_MODE_ECO,
  PRESET_MODE_AWAY,
};

/* Helper */

template<const char* const& ConstStr>
inline bool isPayloadEqual(const char* payload, size_t size) {
    return size == std::strlen(ConstStr) && std::strncmp(payload, ConstStr, size) == 0;
}

inline bool isTopicEqual(const char* topic1, const char* topic2) {
    return strcmp(topic1, topic2) == 0;
}

const char* getMqttPayload(enum Power power) {
  switch (power) {
    case POWER_OFF: return MQTT_PAYLOAD_POWER_OFF;
    case POWER_ON: return MQTT_PAYLOAD_POWER_ON;
    default: return MQTT_PAYLOAD_POWER_UNKNOWN;
  }
}

const char* getMqttPayload(enum Mode mode) {
  switch (mode) {
    case MODE_OFF: return MQTT_PAYLOAD_MODE_OFF;
    case MODE_HEAT: return MQTT_PAYLOAD_MODE_HEAT;
    case MODE_AUTO: return MQTT_PAYLOAD_MODE_AUTO;
    default: return "unknown";
  }
}

const char* getMqttPayload(enum PresetMode preset_mode) {
  switch (preset_mode) {
    case PRESET_MODE_COMFORT: return MQTT_PAYLOAD_PRESET_MODE_COMFORT;
    case PRESET_MODE_ECO: return MQTT_PAYLOAD_PRESET_MODE_ECO;
    case PRESET_MODE_AWAY: return MQTT_PAYLOAD_PRESET_MODE_AWAY;
    default: return MQTT_PAYLOAD_PRESET_MODE_UNKNOWN;
  }
}

enum Power getPowerFromMqttPayload(const char* payload, size_t size) {
  if      (isPayloadEqual<MQTT_PAYLOAD_POWER_OFF>(payload, size)) return POWER_OFF;
  else if (isPayloadEqual<MQTT_PAYLOAD_POWER_ON>(payload, size))  return POWER_ON;
  return POWER_UNKNOWN;
}

enum Mode getModeFromMqttPayload(const char* payload, size_t size) {
  if      (isPayloadEqual<MQTT_PAYLOAD_MODE_OFF>(payload, size))  return MODE_OFF;
  else if (isPayloadEqual<MQTT_PAYLOAD_MODE_HEAT>(payload, size)) return MODE_HEAT;
  else if (isPayloadEqual<MQTT_PAYLOAD_MODE_AUTO>(payload, size)) return MODE_AUTO;
  return MODE_UNKNOWN;
}

enum PresetMode getPresetModeFromMqttPayload(const char* payload, size_t size) {
  if      (isPayloadEqual<MQTT_PAYLOAD_PRESET_MODE_COMFORT>(payload, size)) return PRESET_MODE_COMFORT;
  else if (isPayloadEqual<MQTT_PAYLOAD_PRESET_MODE_ECO>(payload, size))     return PRESET_MODE_ECO;
  else if (isPayloadEqual<MQTT_PAYLOAD_PRESET_MODE_AWAY>(payload, size))    return PRESET_MODE_AWAY;
  return PRESET_MODE_UNKNOWN;
}

/* Class */

class RadiatorMqtt {
public:
  RadiatorMqtt(PubSubClient &client) : mClient(client) {
  }

  void setup(const char * roomName) {
    char str[MQTT_TOPIC_MAX_SIZE] = {};
    snprintf(str, sizeof(str)-1, MQTT_TOPIC_RAD_PREFIX, roomName);
    mMqttTopicPrefix = str;
    Serial.print("Mqtt topic prefix: ");
    Serial.println(mMqttTopicPrefix);
  }

  const char* getRadTopic(const char* topicSuffix) {
    static char buf[MQTT_TOPIC_MAX_SIZE] = {}; 
    snprintf(buf, MQTT_TOPIC_MAX_SIZE-1, "%s%s", mMqttTopicPrefix.c_str(), topicSuffix);
    return buf;
  }

  void publishMessage(const char* topic, const char* payload, bool retain = false) {
    Serial.print("Publish message [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.println(payload);
    mClient.publish(topic, payload, retain);
  }

  void publishMessage(const char* topic, const float value) {
    snprintf (mqttMsg, MQTT_MSG_BUFFER_SIZE, "%.1f", value);
    publishMessage(topic, mqttMsg);
  }

  void publishMessage(enum Power power) {
    publishMessage(getRadTopic(MQTT_TOPIC_RAD_SUFFIX_POWER), getMqttPayload(power));
  }

  void publishMessage(enum Mode mode) {
    publishMessage(getRadTopic(MQTT_TOPIC_RAD_SUFFIX_MODE), getMqttPayload(mode));
  }

  void publishMessage(enum PresetMode preset_mode) {
    publishMessage(getRadTopic(MQTT_TOPIC_RAD_SUFFIX_PRESET_MODE), getMqttPayload(preset_mode));
  }

private:
  String mMqttTopicPrefix = "";
  PubSubClient &mClient;
  char mqttMsg[MQTT_MSG_BUFFER_SIZE] = {};
};