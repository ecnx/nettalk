/* ------------------------------------------------------------------
 * Net Talk - Random Generator Wrapper
 * ------------------------------------------------------------------ */

#include "nettalk.h"

/**
 * Initialize random generator wrapper
 */
int nettalk_random_init ( struct nettalk_random_t *random )
{
    random->initialized = FALSE;
    mbedtls_entropy_init ( &random->entropy );
    mbedtls_ctr_drbg_init ( &random->ctr_drbg );

    if ( mbedtls_ctr_drbg_seed ( &random->ctr_drbg, mbedtls_entropy_func, &random->entropy,
            ( unsigned char * ) NETTALK_PERS_STRING, strlen ( NETTALK_PERS_STRING ) ) != 0 )
    {
        mbedtls_ctr_drbg_free ( &random->ctr_drbg );
        mbedtls_entropy_free ( &random->entropy );
        return -1;
    }

    random->initialized = TRUE;
    return 0;
}

/**
 * Generate random bytes with random generator wrapper
 */
int nettalk_random_bytes ( struct nettalk_random_t *random, void *buf, size_t len )
{
    if ( !random->initialized )
    {
        return -1;
    }

    if ( mbedtls_ctr_drbg_random ( &random->ctr_drbg, ( uint8_t * ) buf, len ) != 0 )
    {
        return -1;
    }

    return 0;
}

/**
 * Uninitialize random generator wrapper
 */
void nettalk_random_free ( struct nettalk_random_t *random )
{
    if ( random->initialized )
    {
        mbedtls_ctr_drbg_free ( &random->ctr_drbg );
        mbedtls_entropy_free ( &random->entropy );
        random->initialized = FALSE;
    }
}
