/* ------------------------------------------------------------------
 * Net Talk - Utility Stuff
 * ------------------------------------------------------------------ */

#include "nettalk.h"

/**
 * Set socket blocking mode
 */
int socket_set_blocking ( int sock )
{
    long mode = 0;

    /* Get current socket mode */
    if ( ( mode = fcntl ( sock, F_GETFL, 0 ) ) < 0 )
    {
        return -1;
    }

    /* Update socket mode */
    if ( fcntl ( sock, F_SETFL, mode & ~O_NONBLOCK ) < 0 )
    {
        return -1;
    }

    return 0;
}

/**
 * Set socket non-blocking mode
 */
int socket_set_nonblocking ( int sock )
{
    long mode = 0;

    /* Get current socket mode */
    if ( ( mode = fcntl ( sock, F_GETFL, 0 ) ) < 0 )
    {
        return -1;
    }

    /* Update socket mode */
    if ( fcntl ( sock, F_SETFL, mode | O_NONBLOCK ) < 0 )
    {
        return -1;
    }

    return 0;
}

/**
 * Set socket send timeout
 */
int socket_set_send_timeout ( int sock, int timeout )
{
    struct timeval tv;

    /* Prepare time value */
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = ( timeout - tv.tv_sec * 1000 ) * 1000;

    /* Set socket send timeuot */
    if ( setsockopt ( sock, SOL_SOCKET, SO_SNDTIMEO, ( const char * ) &tv, sizeof ( tv ) ) < 0 )
    {
        return -1;
    }

    return 0;
}

/**
 * Unset socket send timeout
 */
int socket_unset_send_timeout ( int sock )
{
    return socket_set_send_timeout ( sock, 0 );
}

/**
 * Send complete data via socket
 */
int send_complete ( int sock, const void *arr, size_t len )
{
    size_t ret;
    size_t sum;

    for ( sum = 0; sum < len; sum += ret )
    {
        if ( ( ssize_t ) ( ret =
                send ( sock, ( const unsigned char * ) arr + sum, len - sum, MSG_NOSIGNAL ) ) < 0 )
        {
            return -1;
        }
    }

    return 0;
}

/**
 * Receive complete data via socket
 */
int recv_complete ( int sock, void *arr, size_t len )
{
    size_t ret;
    size_t sum;

    for ( sum = 0; sum < len; sum += ret )
    {
        if ( ( ssize_t ) ( ret = recv ( sock, ( unsigned char * ) arr + sum, len - sum, 0 ) ) <= 0 )
        {
            if ( !ret )
            {
                errno = EPIPE;
            }
            return -1;
        }
    }

    return 0;
}

/**
 * Send data chunk via socket with reset event
 */
ssize_t send_with_reset ( struct nettalk_context_t *context, int sock, const void *arr, size_t len,
    int timeout )
{
    int status;
    struct pollfd fds[2];

    /* Prepare poll events */
    fds[0].fd = sock;
    fds[0].events = POLLERR | POLLHUP | POLLOUT;
    fds[1].fd = context->reset_pipe.u.s.readfd;
    fds[1].events = POLLERR | POLLHUP | POLLIN;

    /* Poll events */
    if ( ( status = poll ( fds, sizeof ( fds ) / sizeof ( struct pollfd ), timeout ) ) < 0 )
    {
        return -1;
    }

    /* Check for timeout */
    if ( !status )
    {
        errno = ETIMEDOUT;
        return -1;
    }

    /* Check for errors */
    if ( ( fds[0].revents | fds[1].revents ) & ( POLLERR | POLLHUP ) )
    {
        return -1;
    }

    /* Check for reset event */
    if ( ( ~fds[0].revents & POLLOUT ) || ( fds[1].revents & POLLIN ) )
    {
        errno = EINTR;
        return -1;
    }

    /* Send data chunk */
    return send ( sock, arr, len, 0 );
}

/**
 * Receive data chunk via socket with reset event
 */
ssize_t recv_with_reset ( struct nettalk_context_t *context, int sock, void *arr, size_t len,
    int timeout )
{
    int status;
    struct pollfd fds[2];

    /* Prepare poll events */
    fds[0].fd = sock;
    fds[0].events = POLLERR | POLLHUP | POLLIN;
    fds[1].fd = context->reset_pipe.u.s.readfd;
    fds[1].events = POLLERR | POLLHUP | POLLIN;

    /* Poll events */
    if ( ( status = poll ( fds, sizeof ( fds ) / sizeof ( struct pollfd ), timeout ) ) < 0 )
    {
        return -1;
    }

    /* Check for timeout */
    if ( !status )
    {
        errno = ETIMEDOUT;
        return -1;
    }

    /* Check for errors */
    if ( ( fds[0].revents | fds[1].revents ) & ( POLLERR | POLLHUP ) )
    {
        return -1;
    }

    /* Check for reset event */
    if ( ( ~fds[0].revents & POLLIN ) || ( fds[1].revents & POLLIN ) )
    {
        errno = EINTR;
        return -1;
    }

    /* Receive data chunk */
    return recv ( sock, arr, len, 0 );
}

/**
 * Send complete data via socket with reset event
 */
int send_complete_with_reset ( struct nettalk_context_t *context, int sock, const void *arr,
    size_t len, int timeout )
{
    size_t ret;
    size_t sum;

    for ( sum = 0; sum < len; sum += ret )
    {
        if ( ( ssize_t ) ( ret =
                send_with_reset ( context, sock, ( const unsigned char * ) arr + sum,
                    len - sum, timeout ) ) < 0 )
        {
            return -1;
        }
    }

    return 0;
}

/**
 * Receive complete data via socket with reset event
 */
int recv_complete_with_reset ( struct nettalk_context_t *context, int sock, void *arr, size_t len,
    int timeout )
{
    size_t ret;
    size_t sum;

    for ( sum = 0; sum < len; sum += ret )
    {
        if ( ( ssize_t ) ( ret =
                recv_with_reset ( context, sock, ( unsigned char * ) arr + sum, len - sum,
                    timeout ) ) <= 0 )
        {
            if ( !ret )
            {
                errno = EPIPE;
            }
            return -1;
        }
    }

    return 0;
}

/**
 * Shutdown and close the socket
 */
void shutdown_then_close ( int sock )
{
    shutdown ( sock, SHUT_RDWR );
    close ( sock );
}

/**
 * Secure free data from memory
 */
void secure_free_mem ( void *mem, size_t size )
{
    memset ( mem, '\0', size );
    free ( mem );
}

/**
 * Secure free string from memory
 */
void secure_free_string ( char *string )
{
    if ( string )
    {
        secure_free_mem ( string, strlen ( string ) + 1 );
    }
}

/**
 * Free buffer and set pointer to NULL
 */
void free_ref ( void **buffer )
{
    if ( *buffer )
    {
        free ( *buffer );
        *buffer = NULL;
    }
}

/**
 * Close existing pipe
 */
void pipe_close ( struct pipe_t *thepipe )
{
    if ( thepipe->u.s.readfd >= 0 )
    {
        close ( thepipe->u.s.readfd );
        thepipe->u.s.readfd = -1;
    }

    if ( thepipe->u.s.writefd >= 0 )
    {
        close ( thepipe->u.s.writefd );
        thepipe->u.s.writefd = -1;
    }
}

/**
 * Create new non-blocking pipe
 */
int pipe_new_nonblocking ( struct pipe_t *thepipe )
{
    if ( pipe ( thepipe->u.fds ) < 0 )
    {
        return -1;
    }

    if ( socket_set_nonblocking ( thepipe->u.s.readfd ) < 0 )
    {
        pipe_close ( thepipe );
    }

    return 0;
}

/**
 * Get current time volatile
 */
int gettimeofdayv ( volatile struct timeval *tv )
{
    struct timeval now;

    if ( gettimeofday ( &now, NULL ) < 0 )
    {
        return -1;
    }

    tv->tv_sec = now.tv_sec;
    tv->tv_usec = now.tv_usec;

    return 0;
}

/**
 * AMR-NB compression state reset chunk
 */
const uint8_t reset_chunk[AMRNB_CHUNK_MAX] = {
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc
};

/**
 * AMR-NB compression state initialize chunk
 */
const uint8_t init_chunk[AMRNB_CHUNK_MAX] = {
    0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
    0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
    0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
    0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd
};

/**
 * No operation chunk
 */
const uint8_t noop_chunk[AMRNB_CHUNK_MAX] = {
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
    0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee
};

/**
 * Text message chunk
 */
const uint8_t text_chunk[AMRNB_CHUNK_MAX] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
