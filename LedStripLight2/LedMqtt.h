#pragma once

#include <ArduinoJson.h>
#include <PubSubClient.h>

#include "Logger.h"

constexpr size_t MQTT_MSG_TOPIC_MAX_SIZE  = 64;
constexpr size_t MQTT_MSG_PAYLOAD_MAX_SIZE = 4096;

/* EXTERNAL MQTT TOPIC */
// Home Assistant
constexpr const char* MQTT_TOPIC_HOMEASSISTANT_STATUS                = "homeassistant/status";  // ["online", "offline"]
constexpr const char* MQTT_TOPIC_HOMEASSISTANT_SWITCH_SUNRISE_CONFIG = "homeassistant/switch/led_sunrise_%s_%d/config";   // %s replaced by ROOM_NAME, %d replaced by SERIAL_NUMBER
constexpr const char* MQTT_TOPIC_HOMEASSISTANT_LIGHT_CONFIG          = "homeassistant/light/led_light_%s_%d/config";      // %s replaced by ROOM_NAME, %d replaced by SERIAL_NUMBER
constexpr const char* MQTT_TOPIC_HOMEASSISTANT_UPDATE_CONFIG         = "homeassistant/update/led_update_%s_%d/config";    // %s replaced by ROOM_NAME, %d replaced by SERIAL_NUMBER
constexpr const char* MQTT_TOPIC_HOMEASSISTANT_SENSOR_RSSI_CONFIG    = "homeassistant/sensor/led_rssi_%s_%d/config";      // %s replaced by ROOM_NAME, %d replaced by SERIAL_NUMBER

// OTA firmware server
constexpr const char* MQTT_TOPIC_OTA_CHECK_UPDATE                    = "home/ota/check_update";

/* LED MQTT TOPIC */
constexpr const char* MQTT_TOPIC_LED_PREFIX                          = "home/%s/led";           // %s replaced by ROOM_NAME
// Home Assitant common topics
constexpr const char* MQTT_TOPIC_LED_SUFFIX_AVAILABILITY             = "/availability";         // ["online", "offline"]
// Home Assistant switch sunrise topics
constexpr const char* MQTT_TOPIC_LED_SUFFIX_SUNRISE                  = "/sunrise";              // ["OFF", "ON"]
constexpr const char* MQTT_TOPIC_LED_SUFFIX_SUNRISE_SET              = "/sunrise/set";          // ["OFF", "ON"]
// Home Assistant light topics
constexpr const char* MQTT_TOPIC_LED_SUFFIX_STATE_SET                = "/state/set";            // ["OFF", "ON"]
constexpr const char* MQTT_TOPIC_LED_SUFFIX_STATE                    = "/state";                // ["OFF", "ON"]
constexpr const char* MQTT_TOPIC_LED_SUFFIX_RGB_SET                  = "/rgb/set";              // [red, green, blue] between [0..255]
constexpr const char* MQTT_TOPIC_LED_SUFFIX_RGB                      = "/rgb";                  // [red, green, blue] between [0..255]
// Home Assistant update topics
constexpr const char* MQTT_TOPIC_LED_SUFFIX_UPDATE_STATE             = "/update/state";         // {installed_version, in_progress }
constexpr const char* MQTT_TOPIC_LED_SUFFIX_UPDATE_COMMAND           = "/update/command";
// Home Assisanst sensors
constexpr const char* MQTT_TOPIC_LED_SUFFIX_SENSOR_RSSI              = "/sensor/rssi";          // [float]


/* MQTT PAYPLOAD */
// Home Assitant
constexpr const char* MQTT_PAYLOAD_ONLINE  = "online";
constexpr const char* MQTT_PAYLOAD_OFFLINE = "offline";
// State
constexpr const char* MQTT_PAYLOAD_STATE_UNKNOWN  = "unknown";
constexpr const char* MQTT_PAYLOAD_STATE_OFF      = "OFF";
constexpr const char* MQTT_PAYLOAD_STATE_ON       = "ON";
enum State {
  STATE_UNKNOWN,
  STATE_OFF,
  STATE_ON,
};

// Entity category
constexpr const char* MQTT_PAYLOAD_CATEGORY_CONFIG = "config";
constexpr const char* MQTT_PAYLOAD_CATEGORY_DIAGNOSTIC = "diagnostic";

/* Helper */

template<const char* const& ConstStr>
inline bool isPayloadEqual(const char* payload, size_t size) {
    return size == std::strlen(ConstStr) && std::strncmp(payload, ConstStr, size) == 0;
}

inline bool isTopicEqual(const char* topic1, const char* topic2) {
    return strcmp(topic1, topic2) == 0;
}

const char* getMqttPayload(enum State state) {
  switch (state) {
    case STATE_OFF: return MQTT_PAYLOAD_STATE_OFF;
    case STATE_ON: return MQTT_PAYLOAD_STATE_ON;
    default: return MQTT_PAYLOAD_STATE_UNKNOWN;
  }
}

enum State getStateFromMqttPayload(const char* payload, size_t size) {
  if      (isPayloadEqual<MQTT_PAYLOAD_STATE_OFF>(payload, size)) return STATE_OFF;
  else if (isPayloadEqual<MQTT_PAYLOAD_STATE_ON>(payload, size))  return STATE_ON;
  return STATE_UNKNOWN;
}

void getMqttPayload(const char* payload, size_t size, uint8_t* val1, uint8_t* val2, uint8_t* val3) {
  char str[size+1];
  memcpy(str, payload, size);
  str[size] = '\0';
  sscanf(str, "%hhu, %hhu, %hhu", val1, val2, val3);
}

/* Class */

class LedMqtt {
public:
  LedMqtt(PubSubClient &client) : mClient(client) {
    client.setBufferSize(MQTT_MSG_PAYLOAD_MAX_SIZE);
  }

  void setup(const char *roomName, int serialNumber, const char* version, const char* macWifi) {
    mRoomName = roomName;
    mSerialNumber = serialNumber;
    mVersion = version;
    mMacWifi = macWifi;

    mMqttTopicLedPrefix = MQTT_TOPIC_LED_PREFIX;
    mMqttTopicLedPrefix.replace("%s", roomName);
    Log.debug("Mqtt topic led prefix: %s", mMqttTopicLedPrefix);

    mMqttTopicSwitchSunriseConfig = MQTT_TOPIC_HOMEASSISTANT_SWITCH_SUNRISE_CONFIG;
    mMqttTopicSwitchSunriseConfig.replace("%s", roomName);
    mMqttTopicSwitchSunriseConfig.replace("%d", String(serialNumber));

    mMqttTopicLightConfig = MQTT_TOPIC_HOMEASSISTANT_LIGHT_CONFIG;
    mMqttTopicLightConfig.replace("%s", roomName);
    mMqttTopicLightConfig.replace("%d", String(serialNumber));

    mMqttTopicUpdateConfig = MQTT_TOPIC_HOMEASSISTANT_UPDATE_CONFIG;
    mMqttTopicUpdateConfig.replace("%s", roomName);
    mMqttTopicUpdateConfig.replace("%d", String(serialNumber));

    mMqttTopicSensorRssiConfig = MQTT_TOPIC_HOMEASSISTANT_SENSOR_RSSI_CONFIG;
    mMqttTopicSensorRssiConfig.replace("%s", roomName);
    mMqttTopicSensorRssiConfig.replace("%d", String(serialNumber));
  }

  char* getLedTopic(const char* topicSuffix) {
    static char buf[MQTT_MSG_TOPIC_MAX_SIZE] = {}; 
    snprintf(buf, MQTT_MSG_TOPIC_MAX_SIZE-1, "%s%s", mMqttTopicLedPrefix.c_str(), topicSuffix);
    return buf;
  }

  void publishMessage(const char* topic, const char* payload, bool retain = false) {
    Log.info("Publish message [%s]: %s", topic, payload);
    if (!mClient.publish(topic, payload, retain)) {
      Log.error("Fail to publish message");
    }
  }

  void publishMessage(const char* topic, const float value) {
    snprintf (mMsgPayload, MQTT_MSG_PAYLOAD_MAX_SIZE, "%.2f", value);
    publishMessage(topic, mMsgPayload);
  }

  void publishMessage(const char* topic, const long value) {
    snprintf (mMsgPayload, MQTT_MSG_PAYLOAD_MAX_SIZE, "%ld", value);
    publishMessage(topic, mMsgPayload);
  }

  void publishMessage(const char* topic, const uint8_t value) {
    snprintf (mMsgPayload, MQTT_MSG_PAYLOAD_MAX_SIZE, "%d", value);
    publishMessage(topic, mMsgPayload);
  }

  void publishMessage(const char* topic, const uint8_t val1, const uint8_t val2, const uint8_t val3) {
    snprintf (mMsgPayload, MQTT_MSG_PAYLOAD_MAX_SIZE, "%d, %d, %d", val1, val2, val3);
    publishMessage(topic, mMsgPayload);
  }

  void publishMessage(const char* topic, enum State state) {
    publishMessage(topic, getMqttPayload(state));
  }

  void publishMessageSwitchSuriseConfig() {
    StaticJsonDocument<MQTT_MSG_PAYLOAD_MAX_SIZE> config;
    config["name"] = "Sunrise";
    config["unique_id"] = "id_led_sunrise_" + mRoomName + "_" + mSerialNumber;
    config["platform"] = "switch";
    config["command_topic"] = getLedTopic(MQTT_TOPIC_LED_SUFFIX_SUNRISE_SET);
    config["availability_topic"] = getLedTopic(MQTT_TOPIC_LED_SUFFIX_AVAILABILITY);
    config["retain"] = true;
    addDeviceJson(config);
    size_t size = serializeJson(config, mMsgPayload);
    if (size > MQTT_MSG_PAYLOAD_MAX_SIZE) {
      Log.error("Buffer payload is too small, need: %d", size);
    }
    publishMessage(mMqttTopicSwitchSunriseConfig.c_str(), mMsgPayload, true);
  }

  void publishMessageLightConfig() {
    StaticJsonDocument<MQTT_MSG_PAYLOAD_MAX_SIZE> config;
    config["name"] = "Light";
    config["unique_id"] = "id_led_light_" + mRoomName + "_" + mSerialNumber;
    config["platform"] = "light";
    config["availability_topic"] = getLedTopic(MQTT_TOPIC_LED_SUFFIX_AVAILABILITY);
    config["command_topic"] = getLedTopic(MQTT_TOPIC_LED_SUFFIX_STATE_SET);
    config["state_topic"] = getLedTopic(MQTT_TOPIC_LED_SUFFIX_STATE);
    config["rgb_command_topic"] = getLedTopic(MQTT_TOPIC_LED_SUFFIX_RGB_SET);
    config["rgb_state_topic"] = getLedTopic(MQTT_TOPIC_LED_SUFFIX_RGB);
    config["retain"] = true;
    addDeviceJson(config);
    size_t size = serializeJson(config, mMsgPayload);
    if (size > MQTT_MSG_PAYLOAD_MAX_SIZE) {
      Log.error("Buffer payload is too small, need: %d", size);
    }
    publishMessage(mMqttTopicLightConfig.c_str(), mMsgPayload, true);
  }

  void publishMessageUpdateConfig() {
    StaticJsonDocument<MQTT_MSG_PAYLOAD_MAX_SIZE> config;
    config["name"] = "Firmware";
    config["unique_id"] = "id_led_update_" + mRoomName + "_" + mSerialNumber;
    config["platform"] = "update";
    config["state_topic"] = getLedTopic(MQTT_TOPIC_LED_SUFFIX_UPDATE_STATE);
    config["command_topic"] = getLedTopic(MQTT_TOPIC_LED_SUFFIX_UPDATE_COMMAND);
    config["availability_topic"] = getLedTopic(MQTT_TOPIC_LED_SUFFIX_AVAILABILITY);
    config["entity_category"] = MQTT_PAYLOAD_CATEGORY_DIAGNOSTIC;
    config["retain"] = true;
    config["payload_install"] = "INSTALL";
    addDeviceJson(config);
    size_t size = serializeJson(config, mMsgPayload);
    if (size > MQTT_MSG_PAYLOAD_MAX_SIZE) {
      Log.error("Buffer payload is too small, need: %d", size);
    }
    publishMessage(mMqttTopicUpdateConfig.c_str(), mMsgPayload, true);
  }

  void publishMessageSensorRssiConfig() {
    StaticJsonDocument<MQTT_MSG_PAYLOAD_MAX_SIZE> config;
    config["name"] = "RSSI";
    config["unique_id"] = "id_led_rssi_" + mRoomName + "_" + mSerialNumber;
    config["platform"] = "sensor";
    config["device_class"] = "signal_strength";
    config["unit_of_measurement"] = "dBm";
    config["state_topic"] = getLedTopic(MQTT_TOPIC_LED_SUFFIX_SENSOR_RSSI);
    config["availability_topic"] = getLedTopic(MQTT_TOPIC_LED_SUFFIX_AVAILABILITY);
    config["entity_category"] = MQTT_PAYLOAD_CATEGORY_DIAGNOSTIC;
    addDeviceJson(config);
    size_t size = serializeJson(config, mMsgPayload);
    if (size > MQTT_MSG_PAYLOAD_MAX_SIZE) {
      Log.error("Buffer payload is too small, need: %d", size);
    }
    publishMessage(mMqttTopicSensorRssiConfig.c_str(), mMsgPayload, true);
  }

  void publishMessageUpdateState(const char* latest_version, bool in_progress = false) {
    StaticJsonDocument<MQTT_MSG_PAYLOAD_MAX_SIZE> state;
    state["installed_version"] = mVersion;
    state["latest_version"] = latest_version;
    state["in_progress"] = in_progress;
    size_t size = serializeJson(state, mMsgPayload);
    if (size > MQTT_MSG_PAYLOAD_MAX_SIZE) {
      Log.error("Buffer payload is too small, need: %d", size);
    }
    publishMessage(getLedTopic(MQTT_TOPIC_LED_SUFFIX_UPDATE_STATE), mMsgPayload, true);
  }

  void deleteMessageUpdateCommand() {
    publishMessage(getLedTopic(MQTT_TOPIC_LED_SUFFIX_UPDATE_COMMAND), "", true);
  }

private:
  void addDeviceJson(StaticJsonDocument<MQTT_MSG_PAYLOAD_MAX_SIZE> &config) {
      JsonObject device = config.createNestedObject("device");
      device["name"] = "Led Strip " + mRoomName;
      JsonArray identifiers = device.createNestedArray("identifiers");
      identifiers.add("id_led_" + mRoomName + "_" + String(mSerialNumber));
      device["model"] = "Led Strip Light 2";
      device["manufacturer"] = "Seb";
      device["sw_version"] = mVersion;
      device["serial_number"] = mSerialNumber;
      JsonArray connections = device.createNestedArray("connections");
      JsonArray mac = connections.createNestedArray();
      mac.add("mac");
      mac.add(mMacWifi);
  }

  String mMqttTopicLedPrefix = "";
  String mMqttTopicSwitchSunriseConfig = "";
  String mMqttTopicLightConfig = "";
  String mMqttTopicUpdateConfig = "";
  String mMqttTopicSensorRssiConfig = "";
  PubSubClient &mClient;
  String mVersion = "";
  String mRoomName = "";
  String mMacWifi = "";
  int mSerialNumber = 0;
  char mMsgPayload[MQTT_MSG_PAYLOAD_MAX_SIZE] = {};
};
