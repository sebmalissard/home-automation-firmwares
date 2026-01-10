/*
 * Brief: Convert a PEM key to a DER encoded C header file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>

#define KEY_FILE argv[1]

int main(int argc, char **argv) {
    FILE *fp = NULL;
    EVP_PKEY *pkey = NULL;
    int ret = -1;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s public_key.pem > OtaPublicKey.h\n", argv[0]);
        goto exit;
    }

    fp = fopen(KEY_FILE, "r");
    if (!fp) {
        perror("fopen");
        goto exit;
    }

    pkey = PEM_read_PUBKEY(fp, NULL, NULL, NULL);
    if (!pkey) {
        fprintf(stderr, "Error: Fail to read PEM key.\n");
        goto exit;
    }

    // Convert to DER format
    int len = i2d_PUBKEY(pkey, NULL);
    if (len <= 0) {
        fprintf(stderr, "Error: Fail to convert to DER format.\n");
        goto exit;
    }

    unsigned char *der = malloc(len);
    unsigned char *p = der;
    i2d_PUBKEY(pkey, &p);

    printf("/* Generated from %s */\n", KEY_FILE);
    printf("/* DER encoded public key */\n");
    printf("\n");
    printf("#pragma once\n");
    printf("\n");
    printf("#include <stdint.h>\n");
    printf("\n");
    printf("const uint8_t OTA_PUBLIC_KEY[] = {\n");
    for (int i = 0; i < len; ++i) {
        if (i % 12 == 0) printf("    ");
        printf("0x%02X", der[i]);
        if (i + 1 < len) printf(", ");
        if ((i + 1) % 12 == 0) printf("\n");
    }
    if (len % 12 != 0) printf("\n");
    printf("};\n");
    printf("#define OTA_PUBLIC_KEY_DER_LEN %d\n", len);

    ret = 0;

exit:
    if (der) {
        free(der);
    }
    if (pkey) {
        EVP_PKEY_free(pkey);
    }
    if (fp) {
        fclose(fp);
    }

    return ret;
}