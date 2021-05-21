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
 * Connect socket with timeout
 */
int connect_timeout ( int sock, struct sockaddr *saddr, size_t saddr_len, int timeout_msec )
{
    int status = 0;
    int so_error = 0;
    long mode = 0;
    struct pollfd fds[1];
    socklen_t sock_len = sizeof ( int );

    if ( ( mode = fcntl ( sock, F_GETFL, 0 ) ) < 0 )
    {
        return -1;
    }

    if ( fcntl ( sock, F_SETFL, mode | O_NONBLOCK ) < 0 )
    {
        return -1;
    }

    if ( ( status = connect ( sock, saddr, saddr_len ) ) >= 0 )
    {
        return -1;
    }

    if ( errno != EINPROGRESS )
    {
        return -1;
    }

    /* Preapre poll fd list */
    fds[0].fd = sock;
    fds[0].events = POLLOUT;

    /* Perform poll operation */
    status = poll ( fds, sizeof ( fds ) / sizeof ( struct pollfd ), timeout_msec );

    if ( status < 0 && errno != EINTR )
    {
        return -1;

    } else if ( !status || ~fds[0].revents & POLLOUT )
    {
        errno = ETIMEDOUT;
        return -1;
    }

    if ( getsockopt ( sock, SOL_SOCKET, SO_ERROR, &so_error, &sock_len ) < 0 )
    {
        return -1;
    }

    if ( so_error )
    {
        errno = so_error;
        return -1;
    }

    if ( ( mode = fcntl ( sock, F_GETFL, 0 ) ) < 0 )
    {
        return -1;
    }

    if ( fcntl ( sock, F_SETFL, mode & ( ~O_NONBLOCK ) ) < 0 )
    {
        return -1;
    }

    return 0;
}

/**
 * Write data chunk to fd with reset event
 */
ssize_t write_with_reset ( struct nettalk_context_t *context, int sock, const void *arr, size_t len,
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
    return write ( sock, arr, len );
}

/**
 * Read data chunk from fd with reset event
 */
ssize_t read_with_reset ( struct nettalk_context_t *context, int sock, void *arr, size_t len,
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
    return read ( sock, arr, len );
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
 * Write complete data to fd with reset event
 */
int write_complete_with_reset ( struct nettalk_context_t *context, int sock, const void *arr,
    size_t len, int timeout )
{
    size_t ret;
    size_t sum;

    for ( sum = 0; sum < len; sum += ret )
    {
        if ( ( ssize_t ) ( ret =
                write_with_reset ( context, sock, ( const unsigned char * ) arr + sum,
                    len - sum, timeout ) ) < 0 )
        {
            return -1;
        }
    }

    return 0;
}

/**
 * Read complete data from fd with reset event
 */
int read_complete_with_reset ( struct nettalk_context_t *context, int sock, void *arr, size_t len,
    int timeout )
{
    size_t ret;
    size_t sum;

    for ( sum = 0; sum < len; sum += ret )
    {
        if ( ( ssize_t ) ( ret =
                read_with_reset ( context, sock, ( unsigned char * ) arr + sum, len - sum,
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

/**
 * Text ACK chunk
 */
const uint8_t ack_chunk[AMRNB_CHUNK_MAX] = {
    0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
    0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
    0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
