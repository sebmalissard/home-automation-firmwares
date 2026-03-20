#pragma once

#include <stdio.h>
#include <stdarg.h>

#include <PubSubClient.h>

// %s = ROOM, %s = TYPE
constexpr const char* MQTT_TOPIC_LOG        = "home/%s/%s/log";         // [string]
constexpr const char* MQTT_TOPIC_LOG_SET    = "home/%s/%s/log/level";   // ["", "ERROR", "WARNING", "INFO", "DEBUG"]

class Logger {
  public:
    enum Level : uint8_t {
      NO_LOG = 0,
      ERROR = 1,
      WARNING = 2,
      INFO = 3,
      DEBUG = 4
    };

    Logger()  {
    }

    void setup(PubSubClient *client, const char *room, const char* type) {
      mClient = client;
      snprintf(mMqttTopicLog, sizeof(mMqttTopicLog), MQTT_TOPIC_LOG, room, type);
      snprintf(mMqttTopicLogLevel, sizeof(mMqttTopicLogLevel), MQTT_TOPIC_LOG_SET, room, type);
    }

    void setMqttLevel(const char* level, size_t len) {
      Serial.printf("PAYLOAD: '%s'\n", level);
      if      (strncmp(level, "DEBUG", len) == 0)   mMqttLevel = DEBUG;
      else if (strncmp(level, "INFO", len) == 0)    mMqttLevel = INFO;
      else if (strncmp(level, "WARNING", len) == 0) mMqttLevel = WARNING;
      else if (strncmp(level, "ERROR", len) == 0)   mMqttLevel = ERROR;
      else mMqttLevel = NO_LOG;
    }

    const char* getMqttTopicLevel() {
      return mMqttTopicLogLevel;
    }

    template<typename... Args>
    void error(const char* format, Args... args) {
      _print(ERROR, format, args...);
    }

    template<typename... Args>
    void warning(const char* format, Args... args) {
      _print(WARNING, format, args...);
    }

    template<typename... Args>
    void info(const char* format, Args... args) {
      _print(INFO, format, args...);
    }
  
    template<typename... Args>
    void debug(const char* format, Args... args) {
      _print(DEBUG, format, args...);
    }

    static std::string getString(uint8_t *data, size_t len) { 
      return std::string((char*)data, len);
    }
  
  private:
    template<typename... Args>
    void _print(Level level, const char *format, Args... args) {
      static const char* prefixes[] = { "", "[ERROR]  : ", "[WARNING]: ", "[INFO]   : ", "[DEBUG]  : " };
      const char* prefix = prefixes[static_cast<uint8_t>(level)];
      char buf[256];

      size_t offset = strlen(prefix);
      memcpy(buf, prefix, offset);
      snprintf(&buf[offset], sizeof(buf) - offset, format, args ...);

      Serial.println(buf);

      if (mClient && level <= mMqttLevel) {
        if (!mClient->publish(mMqttTopicLog, buf)) {
          Serial.println("[ERROR]: Fail to publish log");
        }
      }
    }

    char mMqttTopicLog[64] = {};
    char mMqttTopicLogLevel[64] = {};
    PubSubClient* mClient = nullptr;
    Level mMqttLevel = NO_LOG;
};

inline Logger Log;