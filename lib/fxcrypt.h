/* ------------------------------------------------------------------
 * FxCrypt - Library Source
 * ------------------------------------------------------------------ */

#ifndef FXCRYPT_LIB_H
#define FXCRYPT_LIB_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#include <mbedtls/pkcs5.h>

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#define AES256_KEYLEN 32
#define AES256_KEYLEN_BITS (AES256_KEYLEN * 8)
#define AES256_BLOCKLEN 16
#define SHA256_BLOCKLEN 32
#define PERS_STRING "FxCrypt"
#define FS_BLOCKLEN 4096

/**
 * FxCrypt random generator
 */
struct fxcrypt_random_t
{
    int initialized;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
};

/**
 * FxCrypt context structure
 */
struct fxcrypt_context_t
{
    int initialized;
    int derive_n_rounds;
    struct fxcrypt_random_t random;
};

/**
 * Internal buffer structure
 */
struct fxcrypt_buffer_t
{
    int fd;
    uint8_t *mem;
    size_t pos;
    size_t tot;
};

/**
 * Initialize FxCrypt random generator wrapper
 */
extern int fxcrypt_random_init ( struct fxcrypt_random_t *random );

/**
 * Generate random bytes with FxCrypt random generator wrapper
 */
extern int fxcrypt_random_bytes ( struct fxcrypt_random_t *random, uint8_t * buf, size_t len );

/**
 * Uninitialize FxCrypt random generator wrapper
 */
extern void fxcrypt_random_free ( struct fxcrypt_random_t *random );

/**
 * Initialize FxCrypt context
 */
extern int fxcrypt_init ( struct fxcrypt_context_t *context, int derive_n_rounds );

/**
 * Uninitialize FxCrypt context
 */
extern void fxcrypt_free ( struct fxcrypt_context_t *context );

/**
 * Encrypt data file-to-file
 */
extern int fxcrypt_encrypt_file2file ( struct fxcrypt_context_t *context, const char *password,
    const char *ipath, const char *opath );

/**
 * Encrypt data file-to-mem
 */
extern int fxcrypt_encrypt_file2mem ( struct fxcrypt_context_t *context, const char *password,
    const char *ipath, uint8_t ** omem, size_t *olen );

/**
 * Encrypt data mem-to-file
 */
extern int fxcrypt_encrypt_mem2file ( struct fxcrypt_context_t *context, const char *password,
    const uint8_t * imem, size_t ilen, const char *opath );

/**
 * Encrypt data mem-to-mem
 */
extern int fxcrypt_encrypt_mem2mem ( struct fxcrypt_context_t *context, const char *password,
    const uint8_t * imem, size_t ilen, uint8_t ** omem, size_t *olen );

/**
 * Decrypt data file-to-file
 */
extern int fxcrypt_decrypt_file2file ( struct fxcrypt_context_t *context, const char *password,
    const char *ipath, const char *opath );

/**
 * Decrypt data file-to-mem
 */
extern int fxcrypt_decrypt_file2mem ( struct fxcrypt_context_t *context, const char *password,
    const char *ipath, uint8_t ** omem, size_t *olen );

/**
 * Decrypt data mem-to-file
 */
extern int fxcrypt_decrypt_mem2file ( struct fxcrypt_context_t *context, const char *password,
    const uint8_t * imem, size_t ilen, const char *opath );

/**
 * Decrypt data mem-to-mem
 */
extern int fxcrypt_decrypt_mem2mem ( struct fxcrypt_context_t *context, const char *password,
    const uint8_t * imem, size_t ilen, uint8_t ** omem, size_t *olen );

/**
 * Verify file data
 */
extern int fxcrypt_verify_file ( struct fxcrypt_context_t *context, const char *password,
    const char *ipath );

/**
 * Verify memory data
 */
extern int fxcrypt_verify_mem ( struct fxcrypt_context_t *context, const char *password,
    const uint8_t * imem, size_t ilen );

#endif
