#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#define BUFFER_SIZE 4096

int sha256_file(const char *file_path, uint8_t sha256[SHA256_DIGEST_LENGTH]) {
    EVP_MD_CTX *md_ctx = NULL;
    FILE *fp = NULL;
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    int res;
    int ret = -1;

    memset(sha256, 0, SHA256_DIGEST_LENGTH);

    // Open file
    fp = fopen(file_path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: Fail to open file, error: %d (%s)\n", errno, strerror(errno));
        goto exit;
    }

    // Initialize context
    md_ctx = EVP_MD_CTX_new();
    if (md_ctx == NULL) {
        fprintf(stderr, "ERROR: Fail to create EVP context.\n");
        goto exit;
    }

    // Initialize for SHA256 computation
    res = EVP_DigestInit_ex(md_ctx, EVP_sha256(), NULL);
    if (res != 1) {
        fprintf(stderr, "ERROR: Fail to initialize SHA256 digest, error: %lu\n", ERR_get_error());
        ERR_print_errors_fp(stderr);
        goto exit;
    }

    // Read file by block and update hash
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        res = EVP_DigestUpdate(md_ctx, buffer, bytes_read);
        if (res != 1) {
            fprintf(stderr, "ERROR: Fail to update SHA256 digest, error: %lu\n", ERR_get_error());
            ERR_print_errors_fp(stderr);
            goto exit;
        }
    }

    // Finalize hash computation
    res = EVP_DigestFinal_ex(md_ctx, sha256, NULL);
    if (res != 1) {
        fprintf(stderr, "ERROR: Fail to compute final SHA256 digest, error: %lu.\n", ERR_get_error());
        ERR_print_errors_fp(stderr);
        goto exit;
    }

    printf("SHA256 hash computed successfully on file '%s'.\n", file_path);
    ret = 0;

exit:
    if (fp) {
        fclose(fp);
    }

    if (md_ctx) {
        EVP_MD_CTX_free(md_ctx);
    }

    return ret;
}