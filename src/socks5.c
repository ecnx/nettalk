/* ------------------------------------------------------------------
 * Net Talk - Socks5 Proxy Support
 * ------------------------------------------------------------------ */

#include "nettalk.h"

/*
 * Perform Socks5 handshake
 */
int socks5_handshake ( struct nettalk_context_t *context, int sock )
{
    ssize_t len;
    char buffer[32];

    /* Version 5, one method: no authentication */
    buffer[0] = 5;      /* socks version */
    buffer[1] = 1;      /* one method */
    buffer[2] = 0;      /* no auth */

    /* Estabilish session with proxy server */
    if ( send_complete_with_reset ( context, sock, buffer, 3, NETTALK_SEND_TIMEOUT ) < 0 )
    {
        return -1;
    }

    /* Receive operation status */
    if ( ( ssize_t ) ( len =
            recv_with_reset ( context, sock, buffer, sizeof ( buffer ),
                NETTALK_RECV_TIMEOUT ) ) <= 0 )
    {
        return -1;
    }

    /* Analyse received response */
    if ( len != 2 || buffer[0] != 5 || buffer[1] != 0 )
    {
        return -1;
    }

    /* Handshake success */
    return 0;
}

/**
 * Request new Socks5 connection
 */
int socks5_request_hostname ( struct nettalk_context_t *context, int sock, const char *hostname,
    unsigned short port )
{
    ssize_t len;
    size_t hostlen;
    char buffer[HOSTLEN + 32];

    /* Get hostname string length */
    if ( ( hostlen = strlen ( hostname ) ) > HOSTLEN )
    {
        return -1;
    }

    /* Version 5, one method: no authentication */
    buffer[0] = 5;      /* socks version */
    buffer[1] = 1;      /* connect */
    buffer[2] = 0;      /* reserved */
    buffer[3] = 3;      /* hostname */

    buffer[4] = hostlen;        /* hostname length */
    memcpy ( buffer + 5, hostname, hostlen );   /* hostname */
    buffer[5 + hostlen] = port >> 8;    /* port number 1'st byte */
    buffer[6 + hostlen] = port & 0xff;  /* port number 2'nd byte */

    /* Send request to SOCK5 proxy server */
    if ( send_complete_with_reset ( context, sock, buffer, hostlen + 7, NETTALK_SEND_TIMEOUT ) < 0 )
    {
        return -1;
    }

    /* Receive operation status */
    if ( ( ssize_t ) ( len =
            recv_with_reset ( context, sock, buffer, sizeof ( buffer ),
                NETTALK_RECV_TIMEOUT ) ) <= 0 )
    {
        return -1;
    }

    /* Analyse received response */
    if ( len < 2 || buffer[0] != 5 || buffer[1] != 0 )
    {
        return -1;
    }

    /* Request success */
    return 0;
}
