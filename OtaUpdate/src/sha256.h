#pragma once

#include <stdint.h>

#define SHA256_SIZE 32

int sha256_file(const char *file_path, uint8_t sha256[SHA256_SIZE]);