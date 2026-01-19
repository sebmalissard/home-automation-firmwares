#include <EEPROM.h>

// Define  NVM config to write
#define NVM_CONFIG_ID 10

#pragma pack(1)
struct NVMConfig {
  float     sensorTemperatureOffset;
  float     sensorHumidityOffset;
  uint32_t  deviceSerialNumber;
  char      roomName[32];
};
static_assert(sizeof(struct NVMConfig) == 4+4+4+32, "EEPROM config structure size is incorrect");

struct NVMConfig devices[100];
static_assert(sizeof(struct NVMConfig) * 100 == sizeof(devices), "devices structure size is incorrect");

void init_device(float sensorTemperatureOffset,
                 float sensorHumidityOffset,
                 uint32_t deviceSerialNumber,
                 const char* roomName) {
  int idx = deviceSerialNumber;
  devices[idx].sensorTemperatureOffset = sensorTemperatureOffset;
  devices[idx].sensorHumidityOffset = sensorHumidityOffset;
  devices[idx].deviceSerialNumber = deviceSerialNumber;
  strncpy(devices[idx].roomName, roomName, 32);
}

void init_devices() {
  // RadiatorController
  init_device(0., 0., 1, "bedroom");
  init_device(0., 0., 2, "bathroom");
  init_device(0., 0., 3, "kitchen");
  init_device(0., 0., 4, "livingroom");
  init_device(0., 0., 5, "office");

  // LedStripLight2
  init_device(0., 0., 10, "bedroom");

  // Test
  init_device(0., 0., 99, "test");
}

void write_nvm_config(int id) {
  Serial.println("WRITE NVM CONFIG:");

  EEPROM.begin(sizeof(NVMConfig));

  Serial.printf(" - Sensor Temperature Offset: %f\n", devices[id].sensorTemperatureOffset);
  EEPROM.put(offsetof(NVMConfig, sensorTemperatureOffset), devices[id].sensorTemperatureOffset);

  Serial.printf(" - Sensor Humidity Offset: %f\n", devices[id].sensorHumidityOffset);
  EEPROM.put(offsetof(NVMConfig, sensorHumidityOffset), devices[id].sensorHumidityOffset);

  Serial.printf(" - Device Serial Number: %d\n", devices[id].deviceSerialNumber);
  EEPROM.put(offsetof(NVMConfig, deviceSerialNumber), devices[id].deviceSerialNumber);

  Serial.printf(" - Room Name: %s\n", devices[id].roomName);
  EEPROM.put(offsetof(NVMConfig, roomName), devices[id].roomName);

  EEPROM.commit();
  EEPROM.end();
}

void setup() {
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);

  // For serial, don't miss any message
  delay(2000);

  Serial.println("----------------");
  Serial.println("Starting...");

  init_devices();

  write_nvm_config(NVM_CONFIG_ID);

  Serial.println("Finish.");
}

void loop() {
  delay(99999999);
}



