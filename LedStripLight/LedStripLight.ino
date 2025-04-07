#include <Matter.h>
#include <MatterLightbulb.h>
#include <MatterOnOffPluginUnit.h>
#include <ezWS2812.h>

#include "CommandHandler.h"

// Firmware version
#define VERSION "0.3.0"

// Settings
#define LED_NUM               330
#define MODE_LOW_BRIGHTNESS   0

// Matter description
#define DEVICE_NAME   "Bedroom Led Strip Light"
#define VENDOR_NAME   "Seb"
#define PRODUCT_NAME  "LedStripLight v" VERSION
#define SERIAL_NUMBER "10"

#define BUTTON_DEBOUNCE_TIME_MS     50
#define BUTTON_PIN                  BTN_BUILTIN
#define LED_R                       LED_BUILTIN
#define LED_G                       LED_BUILTIN_1
#define LED_B                       LED_BUILTIN_2
#define OFF                         0
#define ON                          1

#define MAX(a, b) (a > b ? a : b)
#define MAX3(a, b, c) MAX(MAX(a, b), c)

void MoveToLevelWithOnOffCallback(chip::app::Clusters::LevelControl::Commands::MoveToLevelWithOnOff::DecodableType&);

// Devices
MatterColorLightbulb matterDevice;
MatterOnOffPluginUnit matterSwitchSunrise;
ezWS2812 leds(LED_NUM); // Use SPI MOSI D11
CommandHandler gCommandHandler(MoveToLevelWithOnOffCallback);

// Sunrise Mode
bool modeSunriseEnable = false;
bool modeSunriseActive = false;
unsigned long startSunriseTimeMs = 0;
unsigned long durationSunriseTimeMs = 0;

// MoveToLevelWithOnOff
uint8_t ledBrightness = 0;
bool pendingOnRequest = false;

void setLedColorDebug(uint8_t red, uint8_t green, uint8_t blue) {
    if (LED_BUILTIN_ACTIVE == LOW) {
      analogWrite(LED_R, 255 - red);
      analogWrite(LED_G, 255 - green);
      analogWrite(LED_B, 255 - blue);
    } else {
      analogWrite(LED_R, red);
      analogWrite(LED_G, green);
      analogWrite(LED_B, blue);
    }
}

void setLedSunrise() {
  unsigned long currentTime = millis();
  unsigned long progress = (currentTime-startSunriseTimeMs) * 10000 / durationSunriseTimeMs;
  uint32_t active_leds = 0;
  uint8_t brightness_value = 0;

  if (progress < 2000) { // 0% - <20%
    active_leds = progress * LED_NUM / 2000;
    brightness_value = 2;

  } else if (progress < 3000) { // 20% - 30%
    active_leds = (progress-2000) * LED_NUM / 1000;
    brightness_value = 4;
  
  } else if (progress < 3500) { // 30% - 35%
    active_leds = (progress-3000) * LED_NUM / 500;
    brightness_value = 6;

  } else if (progress < 4000) { // 35% - 40%
    active_leds = (progress-3500) * LED_NUM / 500;
    brightness_value = 9;

  } else if (progress < 4400) { // 40 % - 44%
    active_leds = (progress-4000) * LED_NUM / 400;
    brightness_value = 12;

  } else if (progress < 4700) { // 44% - 47%
    active_leds = (progress-4400) * LED_NUM / 300;
    brightness_value = 15;

  } else if (progress < 5000) { // 47% - 50%
    active_leds = (progress-4700) * LED_NUM / 300;
    brightness_value = 20;

  } else if (progress < 10000) {  // 50% - 100%
    active_leds = LED_NUM;
    if (ledBrightness <= 20) {
      brightness_value = 20; // Minimal value required
    } else {
      brightness_value = 20 + (uint32_t)(((progress-5000) * (ledBrightness-20)) / (10000-20)); // Remap [5000,10000] -> [20,ledBrightness]
    }

  } else { // End
    active_leds = LED_NUM;
    if (ledBrightness <= 20) {
      brightness_value = 20; // Minimal value required
    } else {
      brightness_value = ledBrightness;
    }
    setModeSunriseActive(false);
    matterSwitchSunrise.set_onoff(OFF);
  }

  matterDevice.set_brightness_percent(brightness_value);

  noInterrupts();
  leds.set_pixel(active_leds, brightness_value, brightness_value, brightness_value, 100, true);
  interrupts();

}

void setLedWS2812(uint8_t r, uint8_t g, uint8_t b) {
#if MODE_LOW_BRIGHTNESS
  uint8_t v = MAX3(r, g, b);

  noInterrupts();

  if (v == 0) { // 0%
    leds.set_pixel(LED_NUM, 0, 0, 0, 0, false); 
  } if (v <= 25) { // <10% -> x LED 1%
    uint16_t active_leds = v*10/25;
    uint8_t r_low = r * 4 / v;  // v=4 -> 1%
    uint8_t g_low = g * 4 / v;
    uint8_t b_low = b * 4 / v;
    for (uint16_t i=0; i<LED_NUM; i++) {
        if (i % 10 < active_leds) {
            leds.set_pixel(1, r_low, g_low, b_low, 100, false); // ON
        } else {
            leds.set_pixel(1, 0, 0, 0, 0, false); // OFF
        }
    }
  } else { // >10% -> all LED x%
      uint8_t v_scaled = 4 + ((v - 27) * (255 - 4)) / (255 - 27); // Remap [27,255] -> [4,255]
      uint8_t r_adj = (uint16_t)r * v_scaled / v;
      uint8_t g_adj = (uint16_t)g * v_scaled / v;
      uint8_t b_adj = (uint16_t)b * v_scaled / v;
      leds.set_pixel(LED_NUM, r_adj, g_adj, b_adj, 100, false); 
  }
  
  leds.end_transfer();
  interrupts();

#else
  noInterrupts();
  leds.set_all(r, g, b);
  interrupts();
#endif
}

void setLedColorRGB(uint8_t red, uint8_t green, uint8_t blue) {
  static uint8_t prevRed = 255;
  static uint8_t prevGreen = 255;
  static uint8_t prevBlue = 255;

  if (prevRed != red || prevGreen != green || prevBlue != blue) {
    Serial.printf("Setting LED color to > r: %u  g: %u  b: %u\n", red, green, blue);

    setLedColorDebug(red, green, blue);
    setLedWS2812(red, green, blue);

    prevRed = red;
    prevGreen = green;
    prevBlue = blue;
  }
}

void setModeSunriseActive(bool active) {
  static bool prev = false;
  if (prev != active) {
    Serial.printf("Set Sunrise mode to %s\n", active ? "ACTIVE" : "INACTIVE");
    modeSunriseActive = active;
    prev = active;
    if (!active) { // Force power LED
      setLedWS2812(0, 0, 0);
    }
  }
}


void printThreadIPv6Addr() {
  chip::Inet::IPAddress ipAddr;
  if (ThreadStackMgrImpl().GetExternalIPv6Address(ipAddr) == CHIP_NO_ERROR) {
    char ipAddrStrBuf[64] = {};
    ipAddr.ToString(ipAddrStrBuf, sizeof(ipAddrStrBuf));
    Serial.print("Thread IPv6 address: ");
    Serial.println(ipAddrStrBuf);
  }
}

void MoveToLevelWithOnOffCallback(chip::app::Clusters::LevelControl::Commands::MoveToLevelWithOnOff::DecodableType& cmd) {
  Serial.printf("Override command MoveToLevelWithOnOff: Level: %d, TransitionTime: %d\n", cmd.level, cmd.transitionTime.IsNull() ? 0 : cmd.transitionTime.Value());

  if (cmd.level == 0 || cmd.level == 255) { // Always start from 0, so only an increase is supported
    Serial.printf("Backward mode: No valid level\n");
    ledBrightness = 0;
    setModeSunriseActive(false);
    
  } else if (modeSunriseEnable && !cmd.transitionTime.IsNull() && cmd.transitionTime.Value() != 0) {
    Serial.printf("Setup mode sunrise\n");
    ledBrightness = cmd.level;
    durationSunriseTimeMs = cmd.transitionTime.Value() * 100; // transitionTime in 1/10 seconds
    startSunriseTimeMs = millis();
    setModeSunriseActive(true);

  } else { // Backward default mode
    Serial.printf("Normal mode\n");
    ledBrightness = cmd.level;
    pendingOnRequest = true;
    setModeSunriseActive(false);
  }
}

void setup() {

  Serial.begin(115200);

  Serial.println("Matter device starting...");
  Serial.printf("Device unique ID: %s\n", getDeviceUniqueIdStr().c_str());

  Matter.begin();
  Matter.disableBridgeEndpoint();

  // Matter device
  matterDevice.begin();
  matterDevice.set_device_name(DEVICE_NAME);
  matterDevice.set_vendor_name(VENDOR_NAME);
  matterDevice.set_product_name(PRODUCT_NAME);
  matterDevice.set_serial_number(SERIAL_NUMBER);

  // Matter Switch
  matterSwitchSunrise.begin();

  // Register command handler
  chip::app::InteractionModelEngine::GetInstance()->RegisterCommandHandler(&gCommandHandler);

  // Button On/Off
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // LED
  leds.begin();
  setLedColorRGB(0, 0, 0);
  setLedColorDebug(0, 255, 0);

  // Commissioning
  if (!Matter.isDeviceCommissioned()) {
    Serial.printf("Matter device is not commissioned\n");
    Serial.printf("Commission it to your Matter hub with the manual pairing code or QR code\n");
    Serial.printf("Manual pairing code: %s\n", Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\n", Matter.getOnboardingQRCodeUrl().c_str());
  }
  while (!Matter.isDeviceCommissioned()) {
    delay(200);
  }

  // Thread Network
  Serial.println("Waiting for Thread network...");
  while (!Matter.isDeviceThreadConnected()) {
    delay(200);
  }
  Serial.println("Connected to Thread network");
  delay(200); // Required in some cases before get IP
  printThreadIPv6Addr();
  setLedColorDebug(255, 0, 0);

  // Matter controller
  Serial.println("Waiting for Matter device discovery...");
  while (!matterDevice.is_online() && !matterSwitchSunrise.is_online()) {
    delay(200);
  }
  Serial.println("Matter device is now online");
  matterSwitchSunrise.set_onoff(modeSunriseEnable);
  setLedColorRGB(0, 0, 0);
}

void buttonLoop() {
  static unsigned long lastToggleTime = 0;
  static PinStatus lastState = HIGH;

  PinStatus currentState = digitalRead(BUTTON_PIN);
  if (currentState != lastState) {
    unsigned long currentToggleTime = millis();

    if (currentToggleTime < lastToggleTime + BUTTON_DEBOUNCE_TIME_MS) {
      return;
    }
  
    if (currentState == HIGH) {
      Serial.println("Button released");
    } else {
      Serial.println("Button pressed");
      matterDevice.toggle();
    }

    lastToggleTime = currentToggleTime;
    lastState = currentState;
  }
}

void ledColorLoop() {
  static bool prevModeSunriseActive = false;
  static bool prevState = OFF;
  bool state = OFF;
  uint8_t red = 0;
  uint8_t green = 0;
  uint8_t blue = 0;

  if (pendingOnRequest) {
    matterDevice.set_onoff(true);
    pendingOnRequest = false;
  }

  state = matterDevice.get_onoff();
  if (prevState != state) {
    Serial.printf("LED %s\n", state == ON ? "ON" : "OFF");
    if (state == OFF) {
      setModeSunriseActive(false);
      matterSwitchSunrise.set_onoff(false);
    }
  }

  if (modeSunriseActive) {
    if (prevModeSunriseActive == false) { // Sunrise started
      Serial.printf("Sunrise mode started\n");
      matterDevice.set_onoff(ON);
      matterDevice.set_hue(0);
      matterDevice.set_saturation(0);
      matterDevice.set_brightness(1);
      setLedColorRGB(0, 0, 0);
      prevModeSunriseActive = true;
    } else { // Mode surise active
      setLedSunrise();
    }
  } else {
    if (ledBrightness) {
      matterDevice.set_brightness(ledBrightness);
    }
    if (state == ON) {
      matterDevice.get_rgb(&red, &green, &blue);
    }
    setLedColorRGB(red, green, blue);
  }

  prevState = state;
  prevModeSunriseActive = modeSunriseActive;
}

void switchSunriseLoop() {
  static bool prevState = OFF;
  bool state = OFF;

  state = matterSwitchSunrise.get_onoff();
  if (prevState != state) {
    Serial.printf("Switch Sunride Mode %s\n", state == ON ? "ON" : "OFF");
    modeSunriseEnable = state;
    prevState = state;
  }
}

void loop() {

  buttonLoop();

  ledColorLoop();

  switchSunriseLoop();
}
