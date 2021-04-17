/* ------------------------------------------------------------------
 * Net Talk - Handshake Module
 * ------------------------------------------------------------------ */

#include "nettalk.h"

/**
 * Xor two partial-keys into session key
 */
static void xor_partial_keys ( const uint8_t * self, const uint8_t * peer, uint8_t * key,
    size_t keylen )
{
    size_t i;

    for ( i = 0; i < keylen; i++ )
    {
        key[i] = self[i] ^ peer[i];
    }
}

/**
 * HMAC data with SHA-256
 */
static int hmac_sha256 ( const uint8_t * key, size_t key_len, const uint8_t * input, size_t length,
    uint8_t * hash )
{
    int ret;
    mbedtls_md_context_t md_ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

    mbedtls_md_init ( &md_ctx );

    if ( ( ret = mbedtls_md_setup ( &md_ctx, mbedtls_md_info_from_type ( md_type ), TRUE ) ) != 0 )
    {
        mbedtls_md_free ( &md_ctx );
        return ret;
    }

    if ( ( ret = mbedtls_md_hmac_starts ( &md_ctx, key, key_len ) ) != 0 )
    {
        mbedtls_md_free ( &md_ctx );
        return ret;
    }

    if ( ( ret = mbedtls_md_hmac_update ( &md_ctx, input, length ) ) != 0 )
    {
        mbedtls_md_free ( &md_ctx );
        return ret;
    }

    if ( ( ret = mbedtls_md_hmac_finish ( &md_ctx, hash ) ) != 0 )
    {
        mbedtls_md_free ( &md_ctx );
        return ret;
    }

    mbedtls_md_free ( &md_ctx );
    return 0;
}

/**
 * Connect with remote peer
 */
int nettalk_handshake ( struct nettalk_context_t *context )
{
    int ret;
    size_t len;
    uint8_t arr[MBEDTLS_MPI_MAX_SIZE];
    uint8_t self_partial_key[AES256_KEYLEN];
    uint8_t peer_partial_key[AES256_KEYLEN];
    unsigned char aeskey[AES256_KEYLEN];
    union
    {
        struct
        {
            uint8_t nonce[AES256_BLOCKLEN];
            uint8_t hmac[SHA256_BLOCKLEN];
        } s;
        uint8_t bytes[AES256_BLOCKLEN + SHA256_BLOCKLEN];
    } tx;
    union
    {
        struct
        {
            uint8_t nonce[AES256_BLOCKLEN];
            uint8_t hmac[SHA256_BLOCKLEN];
        } s;
        uint8_t bytes[AES256_BLOCKLEN + SHA256_BLOCKLEN];
    } rx;
    uint8_t calc_hmac[SHA256_BLOCKLEN];

    if ( nettalk_random_bytes ( &context->random, self_partial_key, sizeof ( self_partial_key ) ) )
    {
        nettalk_error ( context, "failed to get random bytes" );
        return -1;
    }

    context->session.tx_nleft = 0;
    context->session.rx_nleft = 0;

    nettalk_info ( context, "generated self session partial-key" );

    if ( ( ret =
            mbedtls_pk_encrypt ( &context->config.peer_rsa_pub_key, self_partial_key,
                sizeof ( self_partial_key ), arr, &len, sizeof ( arr ), mbedtls_ctr_drbg_random,
                &context->random.ctr_drbg ) ) != 0 )
    {
        nettalk_errcode ( context, "failed to encrypt partial-key", ret );
        memset ( self_partial_key, '\0', sizeof ( self_partial_key ) );
        return -1;
    }

    nettalk_info ( context, "encrypted self session partial-key" );

    if ( send_complete_with_reset ( context, context->session.sock, arr, len,
            NETTALK_SEND_TIMEOUT ) < 0 )
    {
        nettalk_errcode ( context, "failed to send partial-key", errno );
        memset ( self_partial_key, '\0', sizeof ( self_partial_key ) );
        return -1;
    }

    nettalk_info ( context, "sent self session partial-key" );

    if ( recv_complete_with_reset ( context, context->session.sock, arr, len,
            NETTALK_RECV_TIMEOUT ) < 0 )
    {
        nettalk_errcode ( context, "failed to receive peer partial-key", errno );
        memset ( self_partial_key, '\0', sizeof ( self_partial_key ) );
        return -1;
    }

    nettalk_info ( context, "received peer session partial-key" );

    if ( ( ret =
            mbedtls_pk_decrypt ( &context->config.self_rsa_priv_key, arr, len, peer_partial_key,
                &len, sizeof ( peer_partial_key ), mbedtls_ctr_drbg_random,
                &context->random.ctr_drbg ) ) != 0 )
    {
        nettalk_errcode ( context, "failed to decrypt peer partial-key", ret );
        memset ( self_partial_key, '\0', sizeof ( self_partial_key ) );
        memset ( peer_partial_key, '\0', sizeof ( peer_partial_key ) );
        return -1;
    }

    nettalk_info ( context, "decrypted peer session partial-key" );

    xor_partial_keys ( self_partial_key, peer_partial_key, aeskey, AES256_KEYLEN );

    memset ( self_partial_key, '\0', sizeof ( self_partial_key ) );
    memset ( peer_partial_key, '\0', sizeof ( peer_partial_key ) );

    if ( nettalk_random_bytes ( &context->random, tx.s.nonce, sizeof ( tx.s.nonce ) ) )
    {
        nettalk_error ( context, "failed to get random bytes" );
        memset ( aeskey, '\0', sizeof ( aeskey ) );
        return -1;
    }

    nettalk_info ( context, "generated auth tx nonce" );

    if ( ( ret =
            hmac_sha256 ( aeskey, sizeof ( aeskey ), tx.s.nonce, sizeof ( tx.s.nonce ),
                tx.s.hmac ) ) != 0 )
    {
        nettalk_errcode ( context, "failed to sign tx nonce", ret );
        memset ( aeskey, '\0', sizeof ( aeskey ) );
        return -1;
    }

    nettalk_info ( context, "signed auth tx nonce with hmac" );

    if ( send_complete_with_reset ( context, context->session.sock, tx.bytes,
            sizeof ( tx.bytes ), NETTALK_SEND_TIMEOUT ) < 0 )
    {
        nettalk_errcode ( context, "failed to send tx nonce", errno );
        memset ( aeskey, '\0', sizeof ( aeskey ) );
        return -1;
    }

    nettalk_info ( context, "sent auth tx signed nonce" );

    if ( recv_complete_with_reset ( context, context->session.sock, rx.bytes,
            sizeof ( rx.bytes ), NETTALK_RECV_TIMEOUT ) < 0 )
    {
        nettalk_errcode ( context, "failed to receive rx nonce", errno );
        memset ( aeskey, '\0', sizeof ( aeskey ) );
        return -1;
    }

    nettalk_info ( context, "received auth rx signed nonce" );

    if ( ( ret =
            hmac_sha256 ( aeskey, sizeof ( aeskey ), rx.s.nonce, sizeof ( rx.s.nonce ),
                calc_hmac ) ) != 0 )
    {
        nettalk_errcode ( context, "nonce hmac recalculation failed", ret );
        memset ( aeskey, '\0', sizeof ( aeskey ) );
        return -1;
    }

    nettalk_info ( context, "recalculated rx nonce signature" );

    if ( memcmp ( calc_hmac, rx.s.hmac, SHA256_BLOCKLEN ) )
    {
        memset ( aeskey, '\0', sizeof ( aeskey ) );
        nettalk_error ( context, "remote peer unauthorized" );
        return -1;
    }

    memcpy ( context->session.tx_nonce, tx.s.nonce, sizeof ( context->session.tx_nonce ) );
    memcpy ( context->session.rx_nonce, rx.s.nonce, sizeof ( context->session.rx_nonce ) );

    nettalk_info ( context, "remote peer has been authorized" );

    mbedtls_gcm_init ( &context->session.tx_aes );
    mbedtls_gcm_init ( &context->session.rx_aes );

    if ( ( ret =
            mbedtls_gcm_setkey ( &context->session.tx_aes, MBEDTLS_CIPHER_ID_AES, aeskey,
                256 ) ) != 0 )
    {
        nettalk_errcode ( context, "gcm set tx key failed", ret );
        memset ( aeskey, '\0', sizeof ( aeskey ) );
        mbedtls_gcm_free ( &context->session.tx_aes );
        return -1;
    }

    if ( ( ret =
            mbedtls_gcm_setkey ( &context->session.rx_aes, MBEDTLS_CIPHER_ID_AES, aeskey,
                256 ) ) != 0 )
    {
        nettalk_errcode ( context, "gcm set rx key failed", ret );
        memset ( aeskey, '\0', sizeof ( aeskey ) );
        mbedtls_gcm_free ( &context->session.tx_aes );
        mbedtls_gcm_free ( &context->session.rx_aes );
        return -1;
    }

    memset ( aeskey, '\0', sizeof ( aeskey ) );

    if ( ( ret =
            mbedtls_gcm_starts ( &context->session.tx_aes, MBEDTLS_GCM_ENCRYPT,
                context->session.tx_nonce, sizeof ( context->session.tx_nonce ), NULL, 0 ) ) != 0 )
    {
        nettalk_errcode ( context, "gcm tx start failed", ret );
        mbedtls_gcm_free ( &context->session.tx_aes );
        mbedtls_gcm_free ( &context->session.rx_aes );
        return -1;
    }

    if ( ( ret =
            mbedtls_gcm_starts ( &context->session.rx_aes, MBEDTLS_GCM_DECRYPT,
                context->session.rx_nonce, sizeof ( context->session.rx_nonce ), NULL, 0 ) ) != 0 )
    {
        nettalk_errcode ( context, "gcm rx start failed", ret );
        mbedtls_gcm_free ( &context->session.tx_aes );
        mbedtls_gcm_free ( &context->session.rx_aes );
        return -1;
    }

    nettalk_success ( context, "you are connected with peer" );

    return 0;
}
