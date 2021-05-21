/* ------------------------------------------------------------------
 * Net Talk - Networking Task
 * ------------------------------------------------------------------ */

#include "nettalk.h"

/**
 * Networking Task process function
 */
static void nettask_process ( struct nettalk_context_t *context )
{
    int err = FALSE;
    pthread_t playback_thread;
    pthread_t capture_thread;

    if ( socketpair ( AF_UNIX, SOCK_STREAM, 0, context->bridge.u.fds ) < 0 )
    {
        pipe_close ( &context->msgin );
        pipe_close ( &context->msgout );
        pipe_close ( &context->msgloop );
        return;
    }

    if ( socket_set_nonblocking ( context->bridge.u.s.remote ) < 0 )
    {
        close ( context->bridge.u.s.local );
        close ( context->bridge.u.s.remote );
        return;
    }

    if ( nettalk_connect ( context ) < 0 )
    {
        shutdown_then_close ( context->bridge.u.s.local );
        shutdown_then_close ( context->bridge.u.s.remote );
        return;
    }

    if ( nettalk_handshake ( context ) < 0 )
    {
        shutdown_then_close ( context->bridge.u.s.local );
        shutdown_then_close ( context->bridge.u.s.remote );
        shutdown_then_close ( context->session.sock );
        return;
    }

    if ( voice_playback_launch ( context, &playback_thread ) < 0 )
    {
        shutdown_then_close ( context->bridge.u.s.local );
        shutdown_then_close ( context->bridge.u.s.remote );
        shutdown_then_close ( context->session.sock );
        return;
    }

    if ( voice_capture_launch ( context, &capture_thread ) < 0 )
    {
        reconnect_session ( context );
        pthread_join ( playback_thread, NULL );
        shutdown_then_close ( context->bridge.u.s.local );
        shutdown_then_close ( context->bridge.u.s.remote );
        shutdown_then_close ( context->session.sock );
        return;
    }

    context->online = TRUE;

    if ( nettalk_forward_data ( context ) < 0 )
    {
        err = TRUE;
    }

    context->online = FALSE;
    reconnect_session ( context );
    pthread_join ( playback_thread, NULL );
    pthread_join ( capture_thread, NULL );
    shutdown_then_close ( context->bridge.u.s.local );
    shutdown_then_close ( context->bridge.u.s.remote );
    shutdown_then_close ( context->session.sock );
    memset ( context->session.tx_left, '\0', sizeof ( context->session.tx_left ) );
    memset ( context->session.rx_left, '\0', sizeof ( context->session.rx_left ) );
    mbedtls_aes_free ( &context->session.tx_aes );
    mbedtls_aes_free ( &context->session.rx_aes );

    if ( !err )
    {
        nettalk_errcode ( context, "lost connection with peer", errno ? errno : EPIPE );
    }
}

/**
 * Make some delay between retries
 */
static void nettask_delay ( struct nettalk_context_t *context )
{
    struct pollfd fds[1];

    nettalk_info ( context, "retrying in 5 secs..." );

    /* Prepare poll events */
    fds[0].fd = context->reset_pipe.u.s.readfd;
    fds[0].events = POLLERR | POLLHUP | POLLIN;

    /* Wait delay or for reset event */
    if ( poll ( fds, 1, 5000 ) < 0 )
    {
        sleep ( 5 );
    }
}

/* Discard old reset events */
static int nettask_discard_reset ( struct nettalk_context_t *context )
{
    int count = 0;
    uint8_t byte;

    while ( read ( context->reset_pipe.u.s.readfd, &byte, sizeof ( byte ) ) > 0 )
    {
        count++;
    }

    return count;
}

/**
 * Networking Task entry point
 */
static void *nettask_entry_point ( void *arg )
{
    time_t ts;
    struct nettalk_context_t *context = ( struct nettalk_context_t * ) arg;

    for ( ;; )
    {
        ts = time ( NULL );
        nettask_process ( context );
        if ( !nettask_discard_reset ( context ) )
        {
            if ( ts + 2 >= time ( NULL ) )
            {
                nettask_delay ( context );
            }
        }
    }

    return NULL;
}

/**
 * Launch Networking Task
 */
int nettask_launch ( struct nettalk_context_t *context )
{
    long pref;

    /* Start scanner task asynchronously */
    if ( pthread_create ( ( pthread_t * ) & pref, NULL, nettask_entry_point, context ) != 0 )
    {
        return -1;
    }

    return 0;
}

/**
 * Reconnect the session
 */
void reconnect_session ( struct nettalk_context_t *context )
{
    uint8_t byte = '\n';

    if ( write ( context->reset_pipe.u.s.writefd, &byte, sizeof ( byte ) ) >= 0 )
    {
    }
}

/**
 * Check if session reconnect is in progress
 */
int session_would_reconnect ( struct nettalk_context_t *context )
{
    int available;

    if ( ioctl ( context->reset_pipe.u.s.readfd, FIONREAD, &available ) < 0 )
    {
        return TRUE;
    }

    return !!available;
}
