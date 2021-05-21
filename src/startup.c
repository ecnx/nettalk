/* ------------------------------------------------------------------
 * Net Talk - Main Source File
 * ------------------------------------------------------------------ */

#include "nettalk.h"

/**
 * Initialize application context
 */
static int nettalk_init ( struct nettalk_context_t *context )
{
    context->nmessages = 0;
    context->online = FALSE;
    context->playback_status = FALSE;
    context->playback_status_timestamp.tv_sec = 0;
    context->playback_status_timestamp.tv_usec = 0;
    context->capture_status = FALSE;
    context->capture_status_timestamp.tv_sec = 0;
    context->capture_status_timestamp.tv_usec = 0;
    context->playback_preset = FALSE;
    context->playback_preset_timestamp.tv_sec = 0;
    context->playback_preset_timestamp.tv_usec = 0;
    context->capture_preset = FALSE;
    context->capture_preset_timestamp.tv_sec = 0;
    context->capture_preset_timestamp.tv_usec = 0;

    context->bridge.u.s.local = -1;
    context->bridge.u.s.remote = -1;
    context->reset_pipe.u.s.readfd = -1;
    context->reset_pipe.u.s.writefd = -1;
    context->notpid = -1;
    context->notexp = 0;
    context->notif = NULL;

    if ( pipe_new_nonblocking ( &context->reset_pipe ) < 0 )
    {
        return -1;
    }

    if ( socket_set_nonblocking ( context->reset_pipe.u.s.readfd ) < 0 )
    {
        pipe_close ( &context->reset_pipe );
        return -1;
    }

    if ( pipe_new_nonblocking ( &context->msgin ) < 0 )
    {
        pipe_close ( &context->reset_pipe );
        return -1;
    }

    if ( pipe_new_nonblocking ( &context->msgout ) < 0 )
    {
        pipe_close ( &context->reset_pipe );
        pipe_close ( &context->msgin );
        return -1;
    }

    if ( pipe_new_nonblocking ( &context->msgloop ) < 0 )
    {
        pipe_close ( &context->reset_pipe );
        pipe_close ( &context->msgin );
        pipe_close ( &context->msgout );
        return -1;
    }

    if ( pipe_new_nonblocking ( &context->applog ) < 0 )
    {
        pipe_close ( &context->reset_pipe );
        pipe_close ( &context->msgin );
        pipe_close ( &context->msgout );
        pipe_close ( &context->msgloop );
        return -1;
    }

    if ( nettalk_random_init ( &context->random ) < 0 )
    {
        pipe_close ( &context->reset_pipe );
        pipe_close ( &context->msgin );
        pipe_close ( &context->msgout );
        pipe_close ( &context->msgloop );
        pipe_close ( &context->applog );
        return -1;
    }

    return 0;
}

/**
 * Uninitialize application context
 */
static void nettalk_free ( struct nettalk_context_t *context )
{
    pipe_close ( &context->reset_pipe );
    pipe_close ( &context->msgin );
    pipe_close ( &context->msgout );
    pipe_close ( &context->msgloop );
    pipe_close ( &context->applog );
    nettalk_random_free ( &context->random );
    mbedtls_pk_free ( &context->config.self_rsa_priv_key );
    mbedtls_pk_free ( &context->config.self_rsa_pub_key );
    mbedtls_pk_free ( &context->config.peer_rsa_pub_key );
}

/**
 * Decode ip address and port number
 */
static int ip_port_decode ( const char *input, unsigned int *addr, unsigned short *port )
{
    unsigned int lport;
    size_t len;
    const char *ptr;
    char buffer[32];

    /* Find port number separator */
    if ( !( ptr = strchr ( input, ':' ) ) )
    {
        return -1;
    }

    /* Validate destination buffer size */
    if ( ( len = ptr - input ) >= sizeof ( buffer ) )
    {
        return -1;
    }

    /* Save address string */
    memcpy ( buffer, input, len );
    buffer[len] = '\0';

    /* Parse IP address */
    if ( inet_pton ( AF_INET, buffer, addr ) <= 0 )
    {
        return -1;
    }

    ptr++;

    /* Parse port b number */
    if ( sscanf ( ptr, "%u", &lport ) <= 0 || lport > 65535 )
    {
        return -1;
    }

    *port = lport;
    return 0;
}

/**
 * Show program usage message
 */
static void show_usage ( void )
{
    fprintf ( stderr, "\n" "usage: nettalk [--socks5h addr:port] config\n\n" );
}

/**
 * Program entry point
 */
int main ( int argc, char *argv[] )
{
    int arg_off = 0;
    static struct nettalk_context_t context;

    /* Reset app context */
    memset ( &context, '\0', sizeof ( context ) );

    /* Validate arguments count */
    if ( argc < 2 )
    {
        show_usage (  );
        return 1;
    }

    /* Check for SOCKS-5 proxy */
    if ( !strcmp ( argv[1], "--socks5h" ) )
    {
        if ( ip_port_decode ( argv[2], &context.socks5_addr, &context.socks5_port ) < 0 )
        {
            show_usage (  );
            return 1;
        }
        context.socks5_enabled = TRUE;
        arg_off = 2;
    }

    /* Validate arguments count again */
    if ( argc < arg_off + 2 )
    {
        show_usage (  );
        return 1;
    }

    /* Assign config path */
    context.confpath = argv[arg_off + 1];

    /* Initialize context */
    if ( nettalk_init ( &context ) < 0 )
    {
        fprintf ( stderr, "Error: Initialization failed.\n" );
        return 1;
    }

    /* Print version */
    nettalk_info ( &context, "Net Talk - ver. " NET_TALK_VERSION );
    nettalk_info ( &context, "setup was successful" );

    /* Launch program task */
    window_init ( &context );

    /* Uninitialize context */
    nettalk_free ( &context );

    return 0;
}
