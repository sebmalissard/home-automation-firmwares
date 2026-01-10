#pragma once

#include <assert.h>
#include <stdint.h>

// OTA image format:
//  - Header
//  - Signature
//  - Firmware

#define OTA_HEADER_MAGIC    "OTASEB00"

#define OTA_HEADER_SIZE     256
#define OTA_SIGNATURE_SIZE  512

typedef struct __attribute__((packed)) {
    uint8_t  magic[8];              // "OTASEB00" ; increment last two digits for versioning
    uint8_t  chip[32];              // "ESP8266"
    uint8_t  device[32];            // "RadiatorController"
    uint8_t  firmware_version[8];   // "0.0.1"
    uint32_t firmware_size;
    uint8_t  firmware_sha256[32];
    uint8_t  reserved[140];
} OtaHeader;
static_assert(sizeof(OtaHeader) == OTA_HEADER_SIZE, "OtaHeader size mismatch");

typedef struct __attribute__((packed)) {
    uint8_t rsa2048[256];
    uint8_t reserved[256]; // For future use
} OtaSignature;
static_assert(sizeof(OtaSignature) == OTA_SIGNATURE_SIZE, "OtaSignature size mismatch");