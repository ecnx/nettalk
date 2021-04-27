/* ------------------------------------------------------------------
 * Net Talk - Secure Data Forwarding
 * ------------------------------------------------------------------ */

#include "nettalk.h"

enum
{
    POLL_NETWORK_SOCKET = 0,
    POLL_BRIDGE_SOCKET,
    POLL_RESET_PIPE,
    POLL_FDS_COUNT
};

/**
 * Encrypt outcoming data
 */
static int encrypt_data ( struct nettalk_context_t *context, size_t len, const uint8_t * src,
    uint8_t * dst )
{
    if ( mbedtls_gcm_update ( &context->session.tx_aes, len, src, dst ) != 0 )
    {
        return -1;
    }

    return 0;
}

/**
 * Decrypt incoming data
 */
static int decrypt_data ( struct nettalk_context_t *context, size_t len, const uint8_t * src,
    uint8_t * dst )
{
    if ( mbedtls_gcm_update ( &context->session.rx_aes, len, src, dst ) != 0 )
    {
        return -1;
    }

    return 0;
}

/**
 * Encrypt traffic internal
 */
static int encrypt_traffic_in ( struct nettalk_context_t *context, int srcfd, int dstfd )
{
    int len = FORWARD_CHUNK_LEN;
    size_t nleft;
    int recvlim;
    int sendlim;
    int sendwip;
    socklen_t optlen;
    unsigned char left[FORWARD_CHUNK_LEN];

    if ( ioctl ( srcfd, FIONREAD, &recvlim ) < 0 )
    {
        return -1;
    }

    if ( !recvlim )
    {
        return -1;
    }

    if ( recvlim < len )
    {
        len = recvlim;
    }

    if ( ioctl ( dstfd, TIOCOUTQ, &sendwip ) < 0 )
    {
        return -1;
    }

    optlen = sizeof ( sendlim );

    if ( getsockopt ( dstfd, SOL_SOCKET, SO_SNDBUF, &sendlim, &optlen ) < 0 )
    {
        return -1;
    }

    if ( optlen != sizeof ( sendlim ) )
    {
        return -1;
    }

    if ( sendwip > sendlim )
    {
        return -1;
    }

    sendlim -= sendwip;

    if ( !sendlim )
    {
        return -1;
    }

    if ( sendlim < len )
    {
        len = sendlim;
    }

    if ( !len )
    {
        return -1;
    }

    if ( !context->session.tx_nleft )
    {
        len = len - ( len % AES256_BLOCKLEN );

        if ( !len )
        {
            return 0;
        }

        if ( recv ( srcfd, left, len, 0 ) < len )
        {
            return -1;
        }

        if ( encrypt_data ( context, len, left, context->session.tx_left ) < 0 )
        {
            return -1;
        }

        context->session.tx_nleft = len;
    }

    if ( ( len =
            send ( dstfd, context->session.tx_left, context->session.tx_nleft,
                MSG_NOSIGNAL ) ) < 0 )
    {
        return -1;
    }

    if ( len )
    {
        nleft = context->session.tx_nleft - len;
        memcpy ( left, context->session.tx_left + len, nleft );
        memcpy ( context->session.tx_left, left, nleft );
        context->session.tx_nleft = nleft;
    }

    return context->session.tx_nleft == 0;
}

/**
 * Decrypt traffic internal
 */
static int decrypt_traffic_in ( struct nettalk_context_t *context, int srcfd, int dstfd )
{
    int len = FORWARD_CHUNK_LEN;
    size_t nleft;
    int recvlim;
    int sendlim;
    int sendwip;
    socklen_t optlen;
    unsigned char left[FORWARD_CHUNK_LEN];

    if ( ioctl ( srcfd, FIONREAD, &recvlim ) < 0 )
    {
        return -1;
    }

    if ( !recvlim )
    {
        return -1;
    }

    if ( recvlim < len )
    {
        len = recvlim;
    }

    if ( ioctl ( dstfd, TIOCOUTQ, &sendwip ) < 0 )
    {
        return -1;
    }

    optlen = sizeof ( sendlim );

    if ( getsockopt ( dstfd, SOL_SOCKET, SO_SNDBUF, &sendlim, &optlen ) < 0 )
    {
        return -1;
    }

    if ( optlen != sizeof ( sendlim ) )
    {
        return -1;
    }

    if ( sendwip > sendlim )
    {
        return -1;
    }

    sendlim -= sendwip;

    if ( !sendlim )
    {
        return -1;
    }

    if ( sendlim < len )
    {
        len = sendlim;
    }

    if ( !len )
    {
        return -1;
    }

    if ( !context->session.rx_nleft )
    {
        len = len - ( len % AES256_BLOCKLEN );

        if ( !len )
        {
            return 0;
        }

        if ( recv ( srcfd, left, len, 0 ) < len )
        {
            return -1;
        }

        if ( decrypt_data ( context, len, left, context->session.rx_left ) < 0 )
        {
            return -1;
        }

        context->session.rx_nleft = len;
    }

    if ( ( len =
            send ( dstfd, context->session.rx_left, context->session.rx_nleft,
                MSG_NOSIGNAL ) ) < 0 )
    {
        return -1;
    }

    if ( len )
    {
        nleft = context->session.rx_nleft - len;
        memcpy ( left, context->session.rx_left + len, nleft );
        memcpy ( context->session.rx_left, left, nleft );
        context->session.rx_nleft = nleft;
    }

    return context->session.rx_nleft == 0;
}


/**
 * Encrypt traffic
 */
static int encrypt_traffic ( struct nettalk_context_t *context, struct pollfd *src,
    struct pollfd *dst )
{
    int status;

    if ( dst->revents & POLLOUT )
    {
        if ( ( status = encrypt_traffic_in ( context, src->fd, dst->fd ) ) < 0 )
        {
            return -1;
        }

        if ( status )
        {
            dst->events &= ~POLLOUT;
            src->events |= POLLIN;
        }

        return 1;

    } else if ( src->revents & POLLIN )
    {
        src->events &= ~POLLIN;
        dst->events |= POLLOUT;
    }

    return 0;
}

/**
 * Decrypt traffic
 */
static int decrypt_traffic ( struct nettalk_context_t *context, struct pollfd *src,
    struct pollfd *dst )
{
    int status;

    if ( dst->revents & POLLOUT )
    {
        if ( ( status = decrypt_traffic_in ( context, src->fd, dst->fd ) ) < 0 )
        {
            return -1;
        }

        if ( status )
        {
            dst->events &= ~POLLOUT;
            src->events |= POLLIN;
        }

        return 1;

    } else if ( src->revents & POLLIN )
    {
        src->events &= ~POLLIN;
        dst->events |= POLLOUT;
    }

    return 0;
}

/**
 * Forward data cycle
 */
static int nettalk_forward_cycle ( struct nettalk_context_t *context, struct pollfd *fds,
    size_t nfds, struct nettalk_ack_t *ack )
{
    int status;
    time_t now;

    /* Check for abilities */
    if ( poll ( fds, nfds, 1000 ) < 0 )
    {
        nettalk_errcode ( context, "E:poll fds failed", errno );
        return -1;
    }

    /* Check for error */
    if ( ( fds[POLL_RESET_PIPE].
            revents | fds[POLL_NETWORK_SOCKET].revents | fds[POLL_BRIDGE_SOCKET].
            revents ) & ( POLLERR | POLLHUP ) )
    {
        return -1;
    }

    /* Check for reset event */
    if ( fds[POLL_RESET_PIPE].revents & ( POLLERR | POLLHUP | POLLIN ) )
    {
        return -1;
    }

    /* Get current time */
    now = time ( NULL );

    /* Check for lost connection */
    if ( ack->decrypted + 6 < now )
    {
        errno = ETIMEDOUT;
        return -1;
    }

    /* Reply with noop on idle */
    if ( ack->encrypted + 2 < now )
    {
        if ( send ( context->bridge.u.s.local, noop_chunk, sizeof ( noop_chunk ),
                MSG_DONTWAIT ) < 0 )
        {
            return -1;
        }
    }

    /* Forward data from socket to application */
    if ( ( status =
            decrypt_traffic ( context, fds + POLL_NETWORK_SOCKET, fds + POLL_BRIDGE_SOCKET ) ) < 0 )
    {
        return -1;
    }

    /* Update ACK */
    if ( status > 0 )
    {
        ack->decrypted = now;
    }

    /* Forward data from application to socket */
    if ( ( status =
            encrypt_traffic ( context, fds + POLL_BRIDGE_SOCKET, fds + POLL_NETWORK_SOCKET ) ) < 0 )
    {
        return -1;
    }

    /* Update ACK */
    if ( status > 0 )
    {
        ack->encrypted = now;
    }

    return 0;
}

/**
 * Forward application data
 */
int nettalk_forward_data ( struct nettalk_context_t *context )
{
    time_t now;
    struct nettalk_ack_t ack;
    struct pollfd fds[POLL_FDS_COUNT];

    /* Get current time */
    now = time ( NULL );
    ack.encrypted = now;
    ack.decrypted = now;

    /* Setup poll list */
    fds[POLL_NETWORK_SOCKET].fd = context->session.sock;
    fds[POLL_NETWORK_SOCKET].events = POLLERR | POLLHUP | POLLIN;
    fds[POLL_BRIDGE_SOCKET].fd = context->bridge.u.s.remote;
    fds[POLL_BRIDGE_SOCKET].events = POLLERR | POLLHUP | POLLIN;
    fds[POLL_RESET_PIPE].fd = context->reset_pipe.u.s.readfd;
    fds[POLL_RESET_PIPE].events = POLLERR | POLLHUP | POLLIN;

    /* Forward data loop */
    while ( nettalk_forward_cycle ( context, fds,
            sizeof ( fds ) / sizeof ( struct pollfd ), &ack ) >= 0 );

    return 0;
}
