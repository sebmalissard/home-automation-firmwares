#pragma once

#include <ArduinoJson.h>
#include <PubSubClient.h>

constexpr size_t MQTT_MSG_TOPIC_MAX_SIZE  = 64;
constexpr size_t MQTT_MSG_PAYLOAD_MAX_SIZE = 4096;

/* EXTERNAL MQTT TOPIC */
// Home Assitant
constexpr const char* MQTT_TOPIC_HOMEASSISTANT_STATUS                = "homeassistant/status";  // ["online", "offline"]
constexpr const char* MQTT_TOPIC_HOMEASSISTANT_SWITCH_CONFIG         = "homeassistant/switch/radiator_switch_%s_%d/config"; // %s replaced by ROOM_NAME, %d replaced by SERIAL_NUMBER
constexpr const char* MQTT_TOPIC_HOMEASSISTANT_CLIMATE_CONFIG        = "homeassistant/climate/radiator_climate_%s_%d/config"; // %s replaced by ROOM_NAME, %d replaced by SERIAL_NUMBER
constexpr const char* MQTT_TOPIC_HOMEASSISTANT_UPDATE_CONFIG         = "homeassistant/update/radiator_climate_%s_%d/config"; // %s replaced by ROOM_NAME, %d replaced by SERIAL_NUMBER
// OTA firmware server
constexpr const char* MQTT_TOPIC_OTA_CHECK_UPDATE                    = "home/ota/check_update";

/* RAD MQTT TOPIC */
constexpr const char* MQTT_TOPIC_RAD_PREFIX                          = "home/%s/radiator";      // %s replaced by ROOM_NAME
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
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_AVAILABILITY             = "/availibility";         // ["online", "offline"]
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_ACTION                   = "/action";               // ["off", "heating", "idle"]
// Home Assistant update topics
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_UPDATE_STATE             = "/update/state";         // {installed_version, in_progress }
constexpr const char* MQTT_TOPIC_RAD_SUFFIX_UPDATE_COMMAND           = "/update/command";
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
// Action
constexpr const char* MQTT_PAYLOAD_ACTION_UNKNOWN = "unknown";
constexpr const char* MQTT_PAYLOAD_ACTION_OFF     = "off";
constexpr const char* MQTT_PAYLOAD_ACTION_HEATING = "heating";
constexpr const char* MQTT_PAYLOAD_ACTION_IDLE    = "idle";
enum Action {
  ACTION_UNKNOWN,
  ACTION_OFF,
  ACTION_HEATING,
  ACTION_IDLE,
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

const char* getMqttPayload(enum Action action) {
  switch (action) {
    case ACTION_OFF: return MQTT_PAYLOAD_ACTION_OFF;
    case ACTION_HEATING: return MQTT_PAYLOAD_ACTION_HEATING;
    case ACTION_IDLE: return MQTT_PAYLOAD_ACTION_IDLE;
    default: return MQTT_PAYLOAD_ACTION_UNKNOWN;
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
    client.setBufferSize(MQTT_MSG_PAYLOAD_MAX_SIZE);
  }

  void setup(const char *roomName, int serialNumber, const char* version, const char* macWifi) {
    mRoomName = roomName;
    mSerialNumber = serialNumber;
    mVersion = version;
    mMacWifi = macWifi;

    mMqttTopicRadPrefix = MQTT_TOPIC_RAD_PREFIX;
    mMqttTopicRadPrefix.replace("%s", roomName);
    Serial.print("Mqtt topic radiator prefix: ");
    Serial.println(mMqttTopicRadPrefix);

    mMqttTopicSwitchConfig = MQTT_TOPIC_HOMEASSISTANT_SWITCH_CONFIG;
    mMqttTopicSwitchConfig.replace("%s", roomName);
    mMqttTopicSwitchConfig.replace("%d", String(serialNumber));

    mMqttTopicClimateConfig = MQTT_TOPIC_HOMEASSISTANT_CLIMATE_CONFIG;
    mMqttTopicClimateConfig.replace("%s", roomName);
    mMqttTopicClimateConfig.replace("%d", String(serialNumber));

    mMqttTopicUpdateConfig = MQTT_TOPIC_HOMEASSISTANT_UPDATE_CONFIG;
    mMqttTopicUpdateConfig.replace("%s", roomName);
    mMqttTopicUpdateConfig.replace("%d", String(serialNumber));
  }

  char* getRadTopic(const char* topicSuffix) {
    static char buf[MQTT_MSG_TOPIC_MAX_SIZE] = {}; 
    snprintf(buf, MQTT_MSG_TOPIC_MAX_SIZE-1, "%s%s", mMqttTopicRadPrefix.c_str(), topicSuffix);
    return buf;
  }

  void publishMessage(const char* topic, const char* payload, bool retain = false) {
    Serial.print("Publish message [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.println(payload);
    if (!mClient.publish(topic, payload, retain)) {
      Serial.println("ERROR: Fail to publish message");
    }
  }

  void publishMessage(const char* topic, const float value) {
    snprintf (mMsgPayload, MQTT_MSG_PAYLOAD_MAX_SIZE, "%.1f", value);
    publishMessage(topic, mMsgPayload);
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

  void publishMessage(enum Action action) {
    publishMessage(getRadTopic(MQTT_TOPIC_RAD_SUFFIX_ACTION), getMqttPayload(action));
  }

  void publishMessageSwitchConfig() {
    StaticJsonDocument<MQTT_MSG_PAYLOAD_MAX_SIZE> config;
    config["name"] = "Switch";
    config["unique_id"] = "id_radiator_switch_" + mRoomName + "_" + mSerialNumber;
    config["command_topic"] = getRadTopic(MQTT_TOPIC_RAD_SUFFIX_POWER_SET);
    config["state_topic"] = getRadTopic(MQTT_TOPIC_RAD_SUFFIX_POWER);
    config["retain"] = true;
    addDeviceJson(config);
    size_t size = serializeJson(config, mMsgPayload);
    if (size > MQTT_MSG_PAYLOAD_MAX_SIZE) {
      Serial.print("ERROR: Buffer payload is too small, need: ");
      Serial.println(size);
    }
    publishMessage(mMqttTopicSwitchConfig.c_str(), mMsgPayload, true);
  }

  void publishMessageClimateConfig() {
    StaticJsonDocument<MQTT_MSG_PAYLOAD_MAX_SIZE> config;
    config["name"] = "Thermostat";
    config["unique_id"] = "id_radiator_thermostat_" + mRoomName + "_" + mSerialNumber;
    JsonArray modes = config.createNestedArray("modes");
    modes.add(MQTT_PAYLOAD_MODE_OFF);
    modes.add(MQTT_PAYLOAD_MODE_HEAT);
    modes.add(MQTT_PAYLOAD_MODE_AUTO);
    JsonArray preset_modes = config.createNestedArray("preset_modes");
    preset_modes.add(MQTT_PAYLOAD_PRESET_MODE_COMFORT);
    preset_modes.add(MQTT_PAYLOAD_PRESET_MODE_ECO);
    preset_modes.add(MQTT_PAYLOAD_PRESET_MODE_AWAY);
    config["mode_command_topic"] = getRadTopic(MQTT_TOPIC_RAD_SUFFIX_MODE_SET);
    config["mode_state_topic"] = getRadTopic(MQTT_TOPIC_RAD_SUFFIX_MODE);
    config["preset_mode_command_topic"] = getRadTopic(MQTT_TOPIC_RAD_SUFFIX_PRESET_MODE_SET);
    config["preset_mode_state_topic"] = getRadTopic(MQTT_TOPIC_RAD_SUFFIX_PRESET_MODE);
    config["current_temperature_topic"] = getRadTopic(MQTT_TOPIC_RAD_SUFFIX_SENSOR_TEMPERATURE);
    config["current_humidity_topic"] = getRadTopic(MQTT_TOPIC_RAD_SUFFIX_SENSOR_HUMIDITY);
    config["availability_topic"] = getRadTopic(MQTT_TOPIC_RAD_SUFFIX_AVAILABILITY);
    config["action_topic"] = getRadTopic(MQTT_TOPIC_RAD_SUFFIX_ACTION);
    config["retain"] = true;
    addDeviceJson(config);
    size_t size = serializeJson(config, mMsgPayload);
    if (size > MQTT_MSG_PAYLOAD_MAX_SIZE) {
      Serial.print("ERROR: Buffer payload is too small, need: ");
      Serial.println(size);
    }
    publishMessage(mMqttTopicClimateConfig.c_str(), mMsgPayload, true);
  }

  void publishMessageUpdateConfig() {
    StaticJsonDocument<MQTT_MSG_PAYLOAD_MAX_SIZE> config;
    config["name"] = "Firmware";
    config["unique_id"] = "id_radiator_update_" + mRoomName + "_" + mSerialNumber;
    config["platform"] = "update";
    config["state_topic"] = getRadTopic(MQTT_TOPIC_RAD_SUFFIX_UPDATE_STATE);
    config["command_topic"] = getRadTopic(MQTT_TOPIC_RAD_SUFFIX_UPDATE_COMMAND);
    config["entity_category"] = MQTT_PAYLOAD_CATEGORY_DIAGNOSTIC;
    config["retain"] = true;
    config["payload_install"] = "INSTALL";
    addDeviceJson(config);
    size_t size = serializeJson(config, mMsgPayload);
    if (size > MQTT_MSG_PAYLOAD_MAX_SIZE) {
      Serial.print("ERROR: Buffer payload is too small, need: ");
      Serial.println(size);
    }
    publishMessage(mMqttTopicUpdateConfig.c_str(), mMsgPayload, true);
  }

  void publishMessageUpdateState(const char* latest_version, bool in_progress = false) {
    StaticJsonDocument<MQTT_MSG_PAYLOAD_MAX_SIZE> state;
    state["installed_version"] = mVersion;
    state["latest_version"] = latest_version;
    state["in_progress"] = in_progress;
    size_t size = serializeJson(state, mMsgPayload);
    if (size > MQTT_MSG_PAYLOAD_MAX_SIZE) {
      Serial.print("ERROR: Buffer payload is too small, need: ");
      Serial.println(size);
    }
    publishMessage(getRadTopic(MQTT_TOPIC_RAD_SUFFIX_UPDATE_STATE), mMsgPayload, true);
  }

  void deleteMessageUpdateCommand() {
    publishMessage(getRadTopic(MQTT_TOPIC_RAD_SUFFIX_UPDATE_COMMAND), "", true);
  }

private:
  void addDeviceJson(StaticJsonDocument<MQTT_MSG_PAYLOAD_MAX_SIZE> &config) {
      JsonObject device = config.createNestedObject("device");
      device["name"] = "Radiator " + mRoomName;
      device["identifiers"] = "id_radiator_" + mRoomName + "_" + mSerialNumber;
      device["model"] = "Radiator Controller";
      device["manufacturer"] = "Seb";
      device["sw_version"] = mVersion;
      device["serial_number"] = mSerialNumber;
      JsonArray connections = device.createNestedArray("connections");
      JsonArray mac = connections.createNestedArray();
      mac.add("mac");
      mac.add(mMacWifi);
  }

  String mMqttTopicRadPrefix = "";
  String mMqttTopicSwitchConfig = "";
  String mMqttTopicClimateConfig = "";
  String mMqttTopicUpdateConfig = "";
  PubSubClient &mClient;
  String mVersion = "";
  String mRoomName = "";
  String mMacWifi = "";
  int mSerialNumber = 0;
  char mMsgPayload[MQTT_MSG_PAYLOAD_MAX_SIZE] = {};
};
