#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>

#include "sign_rsa2048.h"

static EVP_PKEY* _load_private_key(const char *key_path) {
    FILE *fp = NULL;
    EVP_PKEY *pkey = NULL;
    int key_size_bytes = 0;

    // Open private key file
    fp = fopen(key_path, "r");
    if (!fp) {
        fprintf(stderr, "ERROR: Fail to open private key: '%s'.\n", key_path);
        goto error;
    }

    // Read private key
    pkey = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
    if (!pkey) {
        fprintf(stderr, "ERROR: Fail to read private key, error: %lu)\n", ERR_get_error());
        ERR_print_errors_fp(stderr);
        goto error;
    }

    // Check key type
    if (EVP_PKEY_id(pkey) != EVP_PKEY_RSA) {
        fprintf(stderr, "ERROR: Key is not RSA.\n");
        goto error;
    }

    // Check key size
    key_size_bytes = EVP_PKEY_get_size(pkey);
    if (key_size_bytes != RSA_2048_SIZE) {
        fprintf(stderr, "ERROR: The loaded private key size is not 2048 bits (%d octets).\n", key_size_bytes);
        goto error;
    }

    goto exit;

error:
    if (pkey) {
        EVP_PKEY_free(pkey);
        pkey = NULL;
    }

exit:
    if (fp) {
        fclose(fp);
    }

    return pkey;
}

int sign_rsa2048(const uint8_t *data, size_t data_length, const char *key_path, unsigned char sign[RSA_2048_SIZE]) {
    EVP_PKEY* pkey = NULL;
    EVP_MD_CTX *md_ctx = NULL;
    size_t sig_len = 0;
    int ret = -1;
    int res;

    // Load private RSA 2048 bits key
    pkey = _load_private_key(key_path);
    if (!pkey) {
        goto exit;
    }

    // Setup Signing Context
    md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        fprintf(stderr, "ERROR: Fail to create EVP_MD_CTX.\n");
        goto exit;
    }

    // Initialize signature
    res = EVP_DigestSignInit(md_ctx, NULL, EVP_sha256(), NULL, pkey);
    if (res != 1) {
        fprintf(stderr, "ERROR: Fail to initialize EVP_DigestSignInit, error: %lu\n", ERR_get_error());
        ERR_print_errors_fp(stderr);
        goto exit;
    }

    // Update the hash to be signed
    res = EVP_DigestSignUpdate(md_ctx, data, data_length);
    if (res != 1) {
        fprintf(stderr, "ERROR: Fail to update EVP_DigestSignUpdate, error: %lu\n", ERR_get_error());
        ERR_print_errors_fp(stderr);
        goto exit;
    }
    
    // Finalize signature
    sig_len = RSA_2048_SIZE;
    res = EVP_DigestSignFinal(md_ctx, sign, &sig_len);
    if (res != 1) {
        fprintf(stderr, "ERROR: Fail to finalize signature, error: %lu\n", ERR_get_error());
        ERR_print_errors_fp(stderr);
        goto exit;
    }

    printf("Success to sign data\n");
    ret = 0;

exit:
    if (md_ctx) {
        EVP_MD_CTX_free(md_ctx);
    }

    if (pkey) {
        EVP_PKEY_free(pkey);
    }

    return ret;
}