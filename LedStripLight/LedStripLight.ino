#include <Matter.h>
#include <MatterLightbulb.h>
#include <ezWS2812.h>

// Firmware version
#define VERSION "0.1.0"

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

MatterColorLightbulb matterDevice;
ezWS2812 leds(120);

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

void setLedColorRGB(uint8_t red, uint8_t green, uint8_t blue) {
  static uint8_t prevRed = 255;
  static uint8_t prevGreen = 255;
  static uint8_t prevBlue = 255;

  if (prevRed != red || prevGreen != green || prevBlue != blue) {
    Serial.printf("Setting LED color to > r: %u  g: %u  b: %u\n", red, green, blue);

    setLedColorDebug(red, green, blue);

    noInterrupts();
    leds.set_all(red, green, blue); // Use SPI MOSI D11
    interrupts();

    prevRed = red;
    prevGreen = green;
    prevBlue = blue;
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
  while (!matterDevice.is_online()) {
    delay(200);
  }
  Serial.println("Matter device is now online");
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
  static bool prevState = OFF;
  bool state = OFF;
  uint8_t red = 0;
  uint8_t green = 0;
  uint8_t blue = 0;

  state = matterDevice.get_onoff();
  if (prevState != state) {
    Serial.printf("LED %s\n", state == ON ? "ON" : "OFF");
  }

  if (state == ON) {
    matterDevice.get_rgb(&red, &green, &blue);
  }

  setLedColorRGB(red, green, blue);

  prevState = state;
}

void loop() {

  buttonLoop();

  ledColorLoop();

  delay(200);
}
