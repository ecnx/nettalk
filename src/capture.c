/* ------------------------------------------------------------------
 * Net Talk - Voice captureing Task
 * ------------------------------------------------------------------ */

#include "nettalk.h"
#include "sound.h"

/**
 * Begin audio capturing
 */
static int audiorec_start ( struct nettalk_context_t *context, struct audio_mic_t *mic )
{
    int err = 0;
    unsigned int rate;
    size_t buffer_size;

    unsigned char *buffer = NULL;
    snd_pcm_t *capture_handle;
    snd_pcm_hw_params_t *hw_params = NULL;
    snd_pcm_sframes_t read_frames;

    /* Get request audio rate */
    rate = mic->rate;

    /* Open ALSA device for audio capturing in PCM format */
    if ( ( err = snd_pcm_open ( &capture_handle, mic->dev, SND_PCM_STREAM_CAPTURE, 0 ) ) < 0 )
    {
        nettalk_error ( context, "mic device '%s' open failed", mic->dev );
        nettalk_error ( context, "%s", snd_strerror ( err ) );
        return err;
    }

    /* Allocate ALSA HW params structure */
    if ( ( err = snd_pcm_hw_params_malloc ( &hw_params ) ) < 0 )
    {
        nettalk_error ( context, "mic alloc hw params failed" );
        nettalk_error ( context, "%s", snd_strerror ( err ) );
        goto exit;
    }

    /* Initialize ALSA HW params with default values */
    if ( ( err = snd_pcm_hw_params_any ( capture_handle, hw_params ) ) < 0 )
    {
        nettalk_error ( context, "mic init hw params failed" );
        nettalk_error ( context, "%s", snd_strerror ( err ) );
        goto exit;
    }

    /* Set ALSA HW params access to read/write PCM interleaved */
    if ( ( err =
            snd_pcm_hw_params_set_access ( capture_handle, hw_params,
                SND_PCM_ACCESS_RW_INTERLEAVED ) ) < 0 )
    {
        nettalk_error ( context, "mic device access denied" );
        nettalk_error ( context, "%s", snd_strerror ( err ) );
        goto exit;
    }

    /* Set ALSA HW params mic format to acquired PCM format */
    if ( ( err = snd_pcm_hw_params_set_format ( capture_handle, hw_params, mic->format ) ) < 0 )
    {
        nettalk_error ( context, "mic get mic format failed" );
        nettalk_error ( context, "%s", snd_strerror ( err ) );
        goto exit;
    }

    /* Set ALSA HW params mic exact or nearest rate */
    if ( ( err = snd_pcm_hw_params_set_rate_near ( capture_handle, hw_params, &rate, 0 ) ) < 0 )
    {
        nettalk_error ( context, "mic set mic rate failed" );
        nettalk_error ( context, "%s", snd_strerror ( err ) );
        goto exit;
    }

    /* Set ALSA HW params channel layout */
    if ( ( err = snd_pcm_hw_params_set_channels ( capture_handle, hw_params, mic->channels ) ) < 0 )
    {
        nettalk_error ( context, "mic set channel to %i failed" );
        nettalk_error ( context, "%s", snd_strerror ( err ) );
        goto exit;
    }

    /* Apply ALSA HW params */
    if ( ( err = snd_pcm_hw_params ( capture_handle, hw_params ) ) < 0 )
    {
        nettalk_error ( context, "mic apply hw params failed" );
        nettalk_error ( context, "%s", snd_strerror ( err ) );
        goto exit;
    }

    /* Free ALSA HW params */
    snd_pcm_hw_params_free ( hw_params );
    hw_params = NULL;

    /* Prepare audio interface for audio capturing */
    if ( ( err = snd_pcm_prepare ( capture_handle ) ) < 0 )
    {
        nettalk_error ( context, "mic prepare interface failed" );
        nettalk_error ( context, "%s", snd_strerror ( err ) );
        goto exit;
    }

    /* Calculate max frames per encode cycle */
    mic->encoder->frames_max = NETTALK_ENCODE_NCHUNKS * AMRNB_SAMPLES_MAX;

    /* Configure encoder */
    mic->encoder->channels = mic->channels;
    mic->encoder->inrate = rate;
    mic->encoder->format = mic->format;

    /* Initialize encoder */
    if ( ( err = mic->encoder->init_callback ( context, mic->encoder ) ) < 0 )
    {
        goto exit;
    }

    /* Calculate PCM buffer size */
    buffer_size =
        mic->encoder->frames_max * snd_pcm_format_width ( mic->format ) / 8 *
        mic->encoder->channels;

    /* Allocate PCM buffer */
    if ( !( buffer = malloc ( buffer_size ) ) )
    {
        nettalk_errcode ( context, "mic buffer alloc failed", errno );
        err = ENOMEM;
        goto exit;
    }

    nettalk_info ( context, "microphone enabled" );

    /* Forward PCM data loop */
    while ( context->capture_status )
    {
        /* Gather PCM data */
        if ( ( int ) ( read_frames =
                snd_pcm_readi ( capture_handle, buffer, mic->encoder->frames_max ) ) < 0 )
        {
            nettalk_error ( context, "mic pcm read failed" );
            nettalk_error ( context, "%s", snd_strerror ( err ) );
            err = -1;
            break;
        }

        /* Forward PCM data */
        if ( ( err =
                mic->encoder->process_callback ( context, mic->encoder, buffer,
                    read_frames ) ) < 0 )
        {
            break;
        }
    }

    nettalk_info ( context, "microphone disabled" );

  exit:

    /* Uninitialize audio encoder */
    mic->encoder->free_callback ( mic->encoder );

    /* Free PCM buffer */
    free ( buffer );

    /* Free ALSA HW params */
    if ( hw_params )
    {
        snd_pcm_hw_params_free ( hw_params );
    }

    /* Close ALSA device */
    snd_pcm_close ( capture_handle );

    return err;
}

/**
 * Forward text messages only
 */
static void capture_fallback ( struct nettalk_context_t *context )
{
    ssize_t len;
    struct pollfd fds[1];
    unsigned char buffer[AMRNB_CHUNK_MAX];

    /* Prepare message chunk buffer */
    memcpy ( buffer, text_chunk, sizeof ( buffer ) );

    /* Message forward loop */
    while ( !context->capture_status )
    {
        /* Reset remote decoder once */
        if ( context->reset_encoder_peer )
        {
            if ( send_complete_with_reset ( context, context->bridge.u.s.local, reset_chunk,
                    sizeof ( reset_chunk ), NETTALK_SEND_TIMEOUT ) < 0 )
            {
                break;
            }
            context->reset_encoder_peer = 0;
        }

        /* Prepare poll events */
        fds[0].fd = context->msgout.u.s.readfd;
        fds[0].events = POLLERR | POLLHUP | POLLIN;

        if ( poll ( fds, 1, 100 ) > 0 )
        {
            if ( fds[0].revents & ( POLLERR | POLLHUP ) )
            {
                break;
            }

            if ( ( len =
                    read ( context->msgout.u.s.readfd, buffer + 24,
                        sizeof ( buffer ) - 24 ) ) <= 0 )
            {
                break;
            }

            memset ( buffer + 24 + len, '\0', sizeof ( buffer ) - 24 - len );

            if ( send_complete_with_reset ( context, context->bridge.u.s.local, buffer,
                    sizeof ( buffer ), NETTALK_SEND_TIMEOUT ) < 0 )
            {
                break;
            }

            if ( write ( context->msgloop.u.s.writefd, buffer + 24, sizeof ( buffer ) - 24 ) < 0 )
            {
            }
        }
    }
}

/**
 * Voice captureer cycle
 */
static void voicerec_cycle ( struct nettalk_context_t *context )
{
    struct audio_encoder_t encoder;
    struct audio_mic_t mic;

    if ( !context->capture_status )
    {
        capture_fallback ( context );
        return;
    }

    memset ( &encoder, '\0', sizeof ( encoder ) );
    encoder.bitrate = 12200;
    encoder.init_callback = nettalk_audio_encoder_init;
    encoder.process_callback = nettalk_encode_audio;
    encoder.free_callback = nettalk_audio_encoder_free;

    memset ( &mic, '\0', sizeof ( mic ) );
    strncpy ( mic.dev, ALSA_DEFAULT_DEV, sizeof ( mic.dev ) );
    mic.channels = AUDIO_LAYOUT_MONO;
    mic.rate = AUDIO_RATE_DEFAULT;
    mic.format = SND_PCM_FORMAT_FLOAT_LE;
    mic.encoder = &encoder;

    if ( audiorec_start ( context, &mic ) < 0 )
    {
        nettalk_info ( context, "audio input not available" );
        context->capture_status = FALSE;
        gettimeofdayv ( &context->capture_status_timestamp );
        capture_fallback ( context );
    }
}

/**
 * Voice captureer entry point
 */
static void *voice_capture_entry ( void *arg )
{
    struct nettalk_context_t *context = ( struct nettalk_context_t * ) arg;

    for ( ;; )
    {
        voicerec_cycle ( context );
        sleep ( 1 );
    }

    return NULL;
}

/**
 * Voice Capture Task
 */
int voice_capture_launch ( struct nettalk_context_t *context )
{
    long pref;

    /* Start scanner task asynchronously */
    if ( pthread_create ( ( pthread_t * ) & pref, NULL, voice_capture_entry, context ) != 0 )
    {
        return -1;
    }

    return 0;
}
