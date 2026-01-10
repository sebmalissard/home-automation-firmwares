#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <OtaImageFormat.h>

#include "sha256.h"
#include "sign_rsa2048.h"

#define BUFFER_SIZE 4096

static struct option long_options[] = {
    {"help",        no_argument,       NULL, 'h'},
    {"chip",        required_argument, NULL, 'c'},
    {"device",      required_argument, NULL, 'd'},
    {"version",     required_argument, NULL, 'v'},
    {"fw",          required_argument, NULL, 'f'},
    {"private-key", required_argument, NULL, 'p'},
    {"out",         required_argument, NULL, 'o'},
    {NULL, 0, NULL, 0}
};

void print_help() {
    printf("\n");
    printf("OTA Generator Image\n");
    printf("Usage: ota_gen_image [options]\n");
    printf("Options:\n");
    printf("  -h, --help                Show this help message\n");
    printf("  -c, --chip <CHIP>         Specify the target chip (e.g., \"ESP8266\")\n");
    printf("  -d, --device <DEVICE>     Specify the target device (e.g., \"RadiatorController\")\n");
    printf("  -v, --version <VERSION>   Specify the firmware version (e.g., \"0.0.1\")\n");
    printf("  -f, --fw <FIRMWARE>       Specify the firmware file path\n");
    printf("  -p, --private-key <KEY>   Specify the RSA 2048 private key file path\n");
    printf("  -o, --out <OUTPUT>        Specify the output OTA image file path\n");
    printf("Example:\n");
    printf("  ./gen_ota_image -c \"ESP8266\" -m \"Radiator Controller\" -v \"3.0.0\" -f RadiatorController.ino.bin -p private_key.pem -o ota_firmware.bin\n");
    printf("\n");
}

void check_arg(bool test, const char *name) {
    if (test) {
        print_help();
        fprintf(stderr, "ERROR: Option '%s' is required.\n", name);
        exit(EXIT_FAILURE);
    }
}

uint32_t get_file_size(FILE *fp) {
    long current_pos;
    long file_size;

    current_pos = ftell(fp);
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, current_pos, SEEK_SET);

    return (uint32_t) file_size;
}

int main(int argc, char *argv[]) {
    OtaHeader header = {};
    OtaSignature signature = {};
    char firmware_path[256] = {};
    char private_key_path[256] = {};
    char output_path[256] = {};
    int opt_idx = 0;
    int c;
    FILE *fw_fp = NULL;
    FILE *ota_fp = NULL;
    int res;
    size_t bytes_read;
    size_t total_bytes_read;
    uint8_t buffer[BUFFER_SIZE];
    int ret = EXIT_FAILURE;

    // Parse arguments
    while ((c = getopt_long(argc, argv, "hc:d:v:f:p:o:", long_options, &opt_idx)) != -1) {
        switch (c) {
            case 'h':
                print_help();
                return 0;
            case 'c':
                strncpy((char*)header.chip, optarg, sizeof(header.chip));
                break;
            case 'd':
                strncpy((char*)header.device, optarg, sizeof(header.device));
                break;
            case 'v':
                strncpy((char*)header.firmware_version, optarg, sizeof(header.firmware_version));
                break;
            case 'f':
                strncpy(firmware_path, optarg, sizeof(firmware_path));
                break;
            case 'p':
                strncpy(private_key_path, optarg, sizeof(private_key_path));
                break;
            case 'o':
                strncpy(output_path, optarg, sizeof(output_path));
                break;
            default:
                print_help();
                fprintf(stderr, "ERROR: Invalid option.\n");
                return -1;
        }
    }

    // Validate required arguments
    check_arg(header.chip[0] == '\0', "--chip");
    check_arg(header.device[0] == '\0', "--device");
    check_arg(header.firmware_version[0] == '\0', "--version");
    check_arg(firmware_path[0] == 0, "--fw");
    check_arg(private_key_path[0] == 0, "--private-key");

    // Fill magic
    memcpy(header.magic, OTA_HEADER_MAGIC, sizeof(header.magic));

    // Compute firmware SHA256
    res = sha256_file(firmware_path, header.firmware_sha256);

    // Fill firmware size
    fw_fp = fopen(firmware_path, "rb");
    if (!fw_fp) {
        fprintf(stderr, "ERROR: Cannot open firmware file '%s', error: %d (%s).\n", firmware_path, errno, strerror(errno));
        goto exit;
    }
    header.firmware_size = get_file_size(fw_fp);

    // Sign header
    res = sign_rsa2048((uint8_t*)&header, sizeof(header), private_key_path, signature.rsa2048);
    if (res != 0) {
        fprintf(stderr, "ERROR: Failed to sign OTA header.\n");
        goto exit;
    }

    // Create OTA image file
    ota_fp = fopen(output_path, "wb");
    if (!ota_fp) {
        fprintf(stderr, "ERROR: Cannot create output file '%s'.\n", output_path);
        goto exit;
    }

    // Write header
    if (fwrite(&header, 1, OTA_HEADER_SIZE, ota_fp) != OTA_HEADER_SIZE) {
        fprintf(stderr, "ERROR: Failed to write header, error: %d (%s)\n", errno, strerror(errno));
        goto exit;
    }

    // Write signature
    if (fwrite(signature.rsa2048, 1, OTA_SIGNATURE_SIZE, ota_fp) != OTA_SIGNATURE_SIZE) {
        fprintf(stderr, "ERROR: Failed to write signature, error: %d (%s)\n", errno, strerror(errno));
        goto exit;
    }

    // Write firmware
    total_bytes_read = 0;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fw_fp)) > 0) {
        total_bytes_read += bytes_read;
        if (fwrite(buffer, 1, bytes_read, ota_fp) != bytes_read) {
            fprintf(stderr, "ERROR: Fail to write firmware, error: %d (%s)\n", errno, strerror(errno));
            goto exit;
        }
    }

    // Check total bytes written
    if (total_bytes_read != header.firmware_size) {
        fprintf(stderr, "ERROR: Mismatch in firmware size, expected %u bytes, wrote %zu bytes.\n", header.firmware_size, total_bytes_read);
        goto exit;
    }

    printf("OTA image '%s' generated successfully.\n", output_path);
    ret = EXIT_SUCCESS;

exit:
    if (fw_fp) {
        fclose(fw_fp);
    }
    if (ota_fp) {
        fclose(ota_fp);
    }

    return ret;
}