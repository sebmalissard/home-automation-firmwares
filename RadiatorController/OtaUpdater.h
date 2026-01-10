#pragma once

#include <BearSSLHelpers.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>

#include <OtaPublicKey.h>
#include <OtaImageFormat.h>

#define BUFFER_SIZE 1024

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

class OtaUpdater {
public:
  OtaUpdater(const char *device, const char *current_version) {
    mDevice = device;
    mCurrentVersion = current_version;
#ifdef ESP8266
    mChip = "ESP8266";
#endif
  }

  int checkUpdate(const char *server_url) {
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    HTTPClient http;
    String payload;
    StaticJsonDocument<256> json;

    // HTTPS unsecure
    client->setInsecure();

    // Establish HTTP connection
    if (!http.begin(*client, server_url)) {
      Serial.println("ERROR: Json HTTP client begin failed.");
      return -1;
    }

    // Request the json file
    int httpCode = http.GET();
    if (httpCode < 0) {
      Serial.printf("HTTP GET fail: %s\n", http.errorToString(httpCode).c_str());
      return -1;
    }
    if (httpCode != HTTP_CODE_OK) {
      Serial.printf("HTTP error code: %d\n", httpCode);
      return -1;
    }
  
    // Get json file
    payload = http.getString();

    // Deserialize json
    DeserializationError error = deserializeJson(json, payload);
    if (error) {
      Serial.printf("ERROR: Fail to parse JSON: %s\n", error.c_str());
      return -1;
    }

    // Parse json
    const char* serverUrl = json["Server URL"];
    JsonArray firmwares = json["Firmwares"].as<JsonArray>();
    if (firmwares.isNull()) {
      Serial.println("No firmwares array");
      return -1;
    }
    for (JsonObject fw : firmwares) {
      const char* device  = fw["Device"];
      const char* chip    = fw["Chip"];
      const char* version = fw["Version"];
      const char* file    = fw["File"];
      Serial.println("---- Firmware ----");
      Serial.printf("Device  : %s\n", device);
      Serial.printf("Chip    : %s\n", chip);
      Serial.printf("Version : %s\n", version);
      Serial.printf("File    : %s\n", file);
      if (mDevice.equals(device) && mChip.equals(chip)) {
        Serial.println("Valide firmware canditate found");
        mExpectedVersion = version;
        mFirmwareUrl = serverUrl;
        mFirmwareUrl += file;
        if (mCurrentVersion.equals(version)) {
          Serial.printf("Already up to date to %s\n", mCurrentVersion.c_str());
          return 0;
        }
        Serial.printf("New update version available %s (current %s)\n", mExpectedVersion.c_str(), mCurrentVersion.c_str());
        return 1;
      }
    }
    
    Serial.println("No candiate firmware found");
    return -1;
  }

  const String getExpectedVersion() {
    return mExpectedVersion;
  }

  int doUpdate() {
    static uint8_t buf[1024];
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    HTTPClient http;
    OtaHeader header = {};
    OtaSignature signature = {};
    WiFiClient* stream = nullptr;
    BearSSL::HashSHA256 hash;
    int http_code;
    int total_payload_size = 0;
    size_t download_size = 0;
    size_t read_size = 0;
    int retry_cnt = 0;

    Serial.printf("Starting OTA firmware update (url=%s)...\n", mFirmwareUrl.c_str());

    // HTTPS unsecure
    client->setInsecure();

    // Establish HTTP connection
    if (!http.begin(*client, mFirmwareUrl.c_str())) {
      Serial.println("ERROR: HTTP client begin failed.");
      goto error;
    }

    // Request the full OTA image file
    http_code = http.GET();
    if (http_code != HTTP_CODE_OK) {
        Serial.printf("ERROR: HTTP GET failed, code: %d\n", http_code);
        goto error;
    }

    // Get total file size (Header + Signature + Firmware)
    total_payload_size = http.getSize();
    if (total_payload_size < (OTA_HEADER_SIZE + OTA_SIGNATURE_SIZE + 1)) {
        Serial.printf("ERROR: File size too small (%d bytes).\n", total_payload_size);
        goto error;
    }

    // Get TCP stream
    stream = http.getStreamPtr();

    // Read header
    if (stream->readBytes((char*)&header, OTA_HEADER_SIZE) != OTA_HEADER_SIZE) {
      Serial.println("ERROR: Failed to read Header over HTTP.");
      goto error;
    }
    
    // Read Signature
    if (stream->readBytes((char*)&signature, OTA_SIGNATURE_SIZE) != OTA_SIGNATURE_SIZE) {
      Serial.println("ERROR: Failed to read Signature over HTTP.");
      goto error;
    }

    // Check header
    if (verifyHeader(header, signature)) {
      Serial.println("ERROR: Failed to verify header");
      goto error;
    }

    // Check firmware version
    if (strncmp(mCurrentVersion.c_str(), (char*)header.firmware_version, sizeof(header.firmware_version)) == 0) {
      Serial.println("ERROR: Firmware is already flashed, nothing to do.");
      goto error;
    }

    // Check firmware size on server
    if (header.firmware_size != total_payload_size - OTA_HEADER_SIZE - OTA_SIGNATURE_SIZE) {
      Serial.println("ERROR: Incorrect firmware size.");
      goto error;
    }

    // Print info
    Serial.printf("Flash size: %u\n", ESP.getFlashChipSize());
    Serial.printf("Sketch size: %u\n", ESP.getSketchSize());
    Serial.printf("Free sketch space: %u\n", ESP.getFreeSketchSpace());

    // Check firmware size with flash
    if (header.firmware_size > (ESP.getSketchSize()+ESP.getFreeSketchSpace())/2) {
      Serial.println("ERROR: Firmware too large for flash.");
      goto error;
    }

    Serial.printf("OTA update on going...\n");

    // Start update by checking firmware size
    if (!Update.begin(header.firmware_size)) {
      Serial.println("ERROR: Start update fail.");
      goto error;
    };

    // Init hash
    hash.begin();

    // Write firmware in flash
    download_size = 0;
    while (stream->connected() && download_size < header.firmware_size) {
      read_size = stream->readBytes(buf, MIN(header.firmware_size - download_size, BUFFER_SIZE));
      if (read_size == 0) {
        retry_cnt++;
        if (retry_cnt > 3) {
          Serial.println("ERROR: HTTP read fail, abort update.");
          goto error;
        }
        delay(10);
        continue;
      }
      retry_cnt = 0;
      hash.add(buf, read_size);
      download_size += read_size;
      if (download_size == header.firmware_size) {
        hash.end();
        if (memcmp(hash.hash(), header.firmware_sha256, sizeof(header.firmware_sha256)) != 0) {
          Serial.println("ERROR: End firmware download, incorrect hash");
          goto error;
        }
        Serial.println("Firmware hash is correct.");
      }
      if (Update.write(buf, read_size) != read_size) {
        Serial.printf("ERROR: Flash write failed at %zu bytes.\n", download_size);
        goto error;
      }
    }

    // Finalize update
    if (!Update.end()) {
      Serial.printf("ERROR: End update failed, error %d\n", Update.getError());
      goto error;
    }

    http.end();
    Serial.println("OTA firmware update done with success, reboot...");
    Serial.flush();
    delay(1000);
    ESP.restart();

    return 0;

  error:
    Update.end(); // Abort update if incomplete
    http.end();
    return -1;
  }


private:
  int verifyField(const char* field, const char* read, const char* expected, size_t size) {
    if (strncmp(read, expected, size) != 0) {
        Serial.printf("ERROR: %s is incorrect\n", field);
        return -1;
    }
    Serial.printf("%s is valid\n", field);
    return 0;
  }

  int verifySignatureRsa2048(const OtaHeader& header, const OtaSignature& signature) {
    static BearSSL::PublicKey pubKey(OTA_PUBLIC_KEY_DER, OTA_PUBLIC_KEY_DER_LEN);
    BearSSL::HashSHA256 hash;
    hash.begin();
    hash.add(&header, sizeof(header));
    hash.end();
    BearSSL::SigningVerifier sign(&pubKey);
    if (!sign.verify(&hash, signature.rsa2048, sizeof(signature.rsa2048))) {
        Serial.println("ERROR: OTA header signature verify failed");
        return -1;
    }
    Serial.println("Header signature is valid");
    return 0;
  }

  int verifyHeader(const OtaHeader& header, const OtaSignature& signature) {
    if (verifyField("Magic", (char*) header.magic, OTA_HEADER_MAGIC, sizeof(header.magic))) return -1;
    if (verifySignatureRsa2048(header, signature)) return -1;
    if (verifyField("Chip", (char*) header.chip, mChip.c_str(), sizeof(header.chip))) return -1;
    if (verifyField("Device", (char*) header.device, mDevice.c_str(), sizeof(header.device))) return -1;
    Serial.println("Header fields are valid");
    return 0;
  }

  String mChip;
  String mDevice;
  String mCurrentVersion;
  String mFirmwareUrl;
  String mExpectedVersion;
};