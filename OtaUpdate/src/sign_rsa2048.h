#pragma once

#include <stddef.h>
#include <stdint.h>

#define RSA_2048_SIZE 256

int sign_rsa2048(const uint8_t*data, size_t data_length, const char *key_path, unsigned char sign[RSA_2048_SIZE]);