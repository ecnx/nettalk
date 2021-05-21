/* ------------------------------------------------------------------
 * Net Talk - Connectivity Module
 * ------------------------------------------------------------------ */

#include "nettalk.h"

/**
 * Resolve hostname into IPv4
 */
int resolve_ipv4 ( const char *hostname, unsigned int *addr )
{
#ifdef SYSTEM_RESOLVER
    struct hostent *he;
    struct in_addr **addr_list;

    /* Directly parse address */
    if ( inet_pton ( AF_INET, hostname, addr ) > 0 )
    {
        return 0;
    }

    /* Query host addess */
    if ( !( he = gethostbyname ( hostname ) ) )
    {
        return -1;
    }

    /* Assign list pointer */
    addr_list = ( struct in_addr ** ) he->h_addr_list;

    /* At least one address required */
    if ( !addr_list[0] )
    {
        errno = ENODATA;
        return -1;
    }

    /* Assign host address */
    *addr = ( *addr_list )[0].s_addr;

    return 0;
#else

    /* Directly parse address */
    if ( inet_pton ( AF_INET, hostname, addr ) > 0 )
    {
        return 0;
    }

    return nsaddr ( hostname, addr );
#endif
}

/**
 * Connect with remote peer
 */
int nettalk_connect ( struct nettalk_context_t *context )
{
    unsigned int addr;
    struct sockaddr_in saddr;
    char channel[CHANLEN + 1];

    nettalk_info ( context, "resolved server hostname" );

    memset ( &saddr, '\0', sizeof ( saddr ) );
    saddr.sin_family = AF_INET;
    if ( context->socks5_enabled )
    {
        saddr.sin_addr.s_addr = context->socks5_addr;
        saddr.sin_port = htons ( context->socks5_port );

    } else
    {
        if ( resolve_ipv4 ( context->config.hostname, &addr ) < 0 )
        {
            nettalk_errcode ( context, "server dns lookup failed", errno );
            return -1;
        }

        saddr.sin_addr.s_addr = addr;
        saddr.sin_port = htons ( context->config.port );
    }

    if ( ( context->session.sock = socket ( AF_INET, SOCK_STREAM, 0 ) ) < 0 )
    {
        nettalk_errcode ( context, "failed to create socket", errno );
        return -1;
    }

    if ( connect_timeout ( context->session.sock, ( struct sockaddr * ) &saddr,
            sizeof ( struct sockaddr_in ), NETTALK_CONN_TIMEOUT ) < 0 )
    {
        if ( context->socks5_enabled )
        {
            nettalk_errcode ( context, "failed to connect proxy", errno );

        } else
        {
            nettalk_errcode ( context, "failed to connect server", errno );
        }
        shutdown_then_close ( context->session.sock );
        return -1;
    }

    if ( context->socks5_enabled )
    {
        nettalk_info ( context, "connected with proxy" );

    } else
    {
        nettalk_info ( context, "connected with server" );
    }

    if ( context->socks5_enabled )
    {
        if ( socks5_handshake ( context, context->session.sock ) < 0 )
        {
            nettalk_errcode ( context, "socks-5 handshake failed", errno );
            shutdown_then_close ( context->session.sock );
            return -1;
        }

        nettalk_info ( context, "socks-5 handshake passed" );

        if ( socks5_request_hostname ( context, context->session.sock, context->config.hostname,
                context->config.port ) < 0 )
        {
            nettalk_errcode ( context, "socks-5 request failed", errno );
            shutdown_then_close ( context->session.sock );
            return -1;
        }

        nettalk_info ( context, "socks-5 request passed" );
    }

    if ( send_complete_with_reset ( context, context->session.sock, context->config.channel,
            strlen ( context->config.channel ), NETTALK_SEND_TIMEOUT ) < 0 )
    {
        shutdown_then_close ( context->session.sock );
        return -1;
    }

    nettalk_info ( context, "broadcasted channel id" );
    nettalk_info ( context, "waiting for remote peer..." );

    if ( recv_complete_with_reset ( context, context->session.sock, channel, CHANLEN,
            NETTALK_WAIT_TIMEOUT ) < 0 )
    {
        if ( errno == ETIMEDOUT )
        {
            nettalk_info ( context, "reconnecting with server..." );
        } else
        {
            nettalk_errcode ( context, "connection shutdown", errno );
        }
        shutdown_then_close ( context->session.sock );
        return -1;
    }

    channel[CHANLEN] = '\0';

    if ( strcmp ( context->config.channel, channel ) )
    {
        nettalk_error ( context, "bound to wrong channel" );
        shutdown_then_close ( context->session.sock );
        return -1;
    }

    nettalk_info ( context, "remote peer is online" );

    return 0;
}
