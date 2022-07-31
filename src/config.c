/* ------------------------------------------------------------------
 * Net Talk - Config Loading
 * ------------------------------------------------------------------ */

#include "nettalk.h"

/**
 * Extract single property
 */
static int props_get ( const char *props, const char *name, char *value, size_t len )
{
    size_t name_len;
    size_t copy_len;
    const char *start_ptr;
    const char *end_ptr;

    if ( strchr ( name, '#' ) || strchr ( name, '=' ) || strchr ( name, '\n' ) )
    {
        return -1;
    }

    name_len = strlen ( name );

    start_ptr = props;
    while ( ( start_ptr = strstr ( start_ptr, name ) ) )
    {
        if ( start_ptr[name_len] == '=' )
        {
            if ( start_ptr == props || start_ptr[-1] == '\n' )
            {
                break;
            }
        }

        start_ptr += name_len;
    }

    if ( !start_ptr )
    {
        return -1;
    }

    start_ptr += name_len + 1;

    end_ptr = start_ptr;

    while ( *end_ptr && *end_ptr != '\r' && *end_ptr != '\n' )
    {
        end_ptr++;
    }

    if ( ( copy_len = end_ptr - start_ptr ) >= len )
    {
        return -1;
    }

    memcpy ( value, start_ptr, copy_len );
    value[copy_len] = '\0';

    return 0;
}

/**
 * Decode RSA key from properties
 */
static void decode_rsa_key ( char *buffer, size_t limit )
{
    char c;
    size_t i;
    size_t len;

    len = strlen ( buffer );

    for ( i = 0; i < len && i < limit; i++ )
    {
        c = buffer[i];

        if ( c == ',' )
        {
            buffer[i] = '\r';

        } else if ( c == '.' )
        {
            buffer[i] = '\n';
        }
    }
}

/**
 * Load config internal
 */
static int load_config_in ( struct nettalk_context_t *context, const char *props, char *buffer,
    size_t bufsize )
{
    unsigned int lport;
    struct nettalk_config_t *config;

    config = &context->config;

    if ( props_get ( props, "host", config->hostname, sizeof ( config->hostname ) ) < 0 )
    {
        return -1;
    }

    if ( props_get ( props, "port", buffer, bufsize ) < 0 )
    {
        return -1;
    }

    if ( sscanf ( buffer, "%u", &lport ) <= 0 )
    {
        return -1;
    }

    if ( lport >= 65536 )
    {
        return -1;
    }

    config->port = lport;

    if ( props_get ( props, "chan", config->channel, sizeof ( config->channel ) ) < 0 )
    {
        return -1;
    }

    if ( props_get ( props, "self", buffer, bufsize ) < 0 )
    {
        return -1;
    }

    decode_rsa_key ( buffer, bufsize );

    if ( mbedtls_pk_parse_key ( &config->self_rsa_priv_key, ( const unsigned char * ) buffer,
            strlen ( buffer ) + 1, NULL, 0 ) != 0 )
    {
        return -1;
    }

    nettalk_info ( context, "parsed config self rsa private key" );

    if ( props_get ( props, "cert", buffer, bufsize ) < 0 )
    {
        return -1;
    }

    decode_rsa_key ( buffer, bufsize );

    if ( mbedtls_pk_parse_public_key ( &config->self_rsa_pub_key, ( const unsigned char * ) buffer,
            strlen ( buffer ) + 1 ) != 0 )
    {
        mbedtls_pk_free ( &config->self_rsa_priv_key );
        return -1;
    }

    nettalk_info ( context, "parsed config self rsa public key" );

    if ( props_get ( props, "peer", buffer, bufsize ) < 0 )
    {
        mbedtls_pk_free ( &config->self_rsa_priv_key );
        return -1;
    }

    decode_rsa_key ( buffer, bufsize );

    if ( mbedtls_pk_parse_public_key ( &config->peer_rsa_pub_key, ( const unsigned char * ) buffer,
            strlen ( buffer ) + 1 ) != 0 )
    {
        mbedtls_pk_free ( &config->self_rsa_priv_key );
        mbedtls_pk_free ( &config->self_rsa_pub_key );
        return -1;
    }

    nettalk_info ( context, "parsed config peer rsa public key" );

    return 0;
}

/**
 * Load config from file
 */
int load_config ( struct nettalk_context_t *context, const char *path, const char *password )
{
    int ret;
    size_t len;
    char *props;
    struct fxcrypt_context_t fxctx;
    char buffer[8192];

    memset ( &context->config, '\0', sizeof ( struct nettalk_config_t ) );

    if ( fxcrypt_init ( &fxctx, DERIVE_N_ROUNDS ) < 0 )
    {
        return -1;
    }

    if ( fxcrypt_decrypt_file2mem ( &fxctx, password, path, ( uint8_t ** ) & props, &len ) < 0 )
    {
        fxcrypt_free ( &fxctx );
        return -1;
    }

    fxcrypt_free ( &fxctx );
    props[len] = '\0';

    nettalk_info ( context, "config decrypted successfully" );

    ret = load_config_in ( context, props, buffer, sizeof ( buffer ) );

    memset ( buffer, '\0', sizeof ( buffer ) );
    secure_free_mem ( props, len );

    return ret;
}
