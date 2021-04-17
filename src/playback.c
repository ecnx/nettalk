/* ------------------------------------------------------------------
 * Net Talk - Voice Playback Task
 * ------------------------------------------------------------------ */

#include "nettalk.h"
#include "sound.h"

/**
 * Begin audio playback
 */
static int audioplay_start ( struct nettalk_context_t *context, struct audio_speaker_t *speaker )
{
    int err = 0;
    int err1;
    unsigned int rate;
    size_t done = 0;
    size_t nframes;
    size_t buffer_size;
    snd_pcm_uframes_t size;

    void *buffer = NULL;
    void *zeros = NULL;
    snd_pcm_t *playback_handle;
    snd_pcm_hw_params_t *hw_params = NULL;
    struct pollfd fds[1];

    /* Get request audio rate */
    rate = speaker->rate;

    /* Open ALSA device for audio capturing in PCM format */
    if ( ( err = snd_pcm_open ( &playback_handle, speaker->dev, SND_PCM_STREAM_PLAYBACK, 0 ) ) < 0 )
    {
        nettalk_error ( context, "speaker device '%s' open failed", speaker->dev );
        nettalk_error ( context, "%s", snd_strerror ( err ) );
        return err;
    }

    /* Allocate ALSA HW params structure */
    if ( ( err = snd_pcm_hw_params_malloc ( &hw_params ) ) < 0 )
    {
        nettalk_error ( context, "speaker alloc hw params failed" );
        nettalk_error ( context, "%s", snd_strerror ( err ) );
        goto exit;
    }

    /* Initialize ALSA HW params with default values */
    if ( ( err = snd_pcm_hw_params_any ( playback_handle, hw_params ) ) < 0 )
    {
        nettalk_error ( context, "speaker init hw params failed" );
        nettalk_error ( context, "%s", snd_strerror ( err ) );
        goto exit;
    }

    /* Set ALSA HW params access to read/write PCM interleaved */
    if ( ( err =
            snd_pcm_hw_params_set_access ( playback_handle, hw_params,
                SND_PCM_ACCESS_RW_INTERLEAVED ) ) < 0 )
    {
        nettalk_error ( context, "speaker device access denied" );
        nettalk_error ( context, "%s", snd_strerror ( err ) );
        goto exit;
    }

    /* Set ALSA HW params playback format to acquired PCM format */
    if ( ( err =
            snd_pcm_hw_params_set_format ( playback_handle, hw_params, speaker->format ) ) < 0 )
    {
        nettalk_error ( context, "speaker get playback format failed" );
        nettalk_error ( context, "%s", snd_strerror ( err ) );
        goto exit;
    }

    /* Set ALSA HW params playback exact or nearest rate */
    if ( ( err = snd_pcm_hw_params_set_rate_near ( playback_handle, hw_params, &rate, 0 ) ) < 0 )
    {
        nettalk_error ( context, "speaker set playback rate failed" );
        nettalk_error ( context, "%s", snd_strerror ( err ) );
        goto exit;
    }

    /* Set ALSA HW params channel layout */
    if ( ( err =
            snd_pcm_hw_params_set_channels ( playback_handle, hw_params, speaker->channels ) ) < 0 )
    {
        nettalk_error ( context, "speaker set channel to %i failed" );
        nettalk_error ( context, "%s", snd_strerror ( err ) );
        goto exit;
    }

    /* Calculate max frames per decode cycle */
    speaker->decoder->frames_max = NETTALK_DECODE_NCHUNKS * AMRNB_SAMPLES_MAX;

    size = speaker->decoder->frames_max;

    /* Set ALSA HW params buffer size */
    if ( ( err =
            snd_pcm_hw_params_set_buffer_size_near ( playback_handle, hw_params, &size ) ) < 0 )
    {
        nettalk_error ( context, "speaker set buffer size failed" );
        nettalk_error ( context, "%s", snd_strerror ( err ) );
        goto exit;
    }

    speaker->decoder->frames_max = size;

    /* Apply ALSA HW params */
    if ( ( err = snd_pcm_hw_params ( playback_handle, hw_params ) ) < 0 )
    {
        nettalk_error ( context, "speaker apply hw params failed" );
        nettalk_error ( context, "%s", snd_strerror ( err ) );
        goto exit;
    }

    /* Free ALSA HW params */
    snd_pcm_hw_params_free ( hw_params );
    hw_params = NULL;

    /* Prepare audio interface */
    if ( ( err = snd_pcm_prepare ( playback_handle ) ) < 0 )
    {
        nettalk_error ( context, "speaker prepare interface failed" );
        nettalk_error ( context, "%s", snd_strerror ( err ) );
        goto exit;
    }

    /* Configure decoder */
    speaker->decoder->channels = speaker->channels;
    speaker->decoder->outrate = rate;
    speaker->decoder->format = speaker->format;

    /* Initialize decoder */
    if ( ( err = speaker->decoder->init_callback ( context, speaker->decoder ) ) < 0 )
    {
        goto exit;
    }

    /* Calculate PCM buffer size */
    buffer_size =
        speaker->decoder->frames_max * snd_pcm_format_width ( speaker->format ) / 8 *
        speaker->decoder->channels;

    /* Allocate PCM buffer */
    if ( !( buffer = malloc ( buffer_size ) ) )
    {
        nettalk_errcode ( context, "speaker buffer alloc failed", errno );
        err = ENOMEM;
        goto exit;
    }

    /* Allocate PCM buffer */
    if ( !( zeros = calloc ( buffer_size, sizeof ( unsigned char ) ) ) )
    {
        nettalk_errcode ( context, "speaker buffer alloc failed", errno );
        err = ENOMEM;
        goto exit;
    }

    nettalk_info ( context, "speaker enabled" );

    /* Set initial frames count to zero */
    nframes = 0;

    /* Prepare poll events */
    fds[0].fd = context->bridge.u.s.local;
    fds[0].events = POLLERR | POLLHUP | POLLIN;

    /* Forward PCM data loop */
    while ( context->playback_status )
    {
        if ( done == nframes )
        {
            if ( poll ( fds, 1, 100 ) > 0 )
            {
                if ( fds[0].revents & ( POLLERR | POLLHUP ) )
                {
                    err = -EPIPE;
                    break;
                }

                /* Update buffer space */
                nframes = speaker->decoder->frames_max;
                done = 0;
                /* Gather PCM data */
                if ( ( err =
                        speaker->decoder->process_callback ( context, speaker->decoder, buffer,
                            &nframes ) ) < 0 )
                {
                    break;
                }
            }
        }

        /* Forward PCM data */
        if ( nframes )
        {
            if ( ( err = snd_pcm_writei ( playback_handle, buffer + done, nframes - done ) ) < 0 )
            {
                if ( ( err = snd_pcm_recover ( playback_handle, err, 0 ) ) < 0 )
                {
                    nettalk_error ( context, "speaker pcm write failed" );
                    nettalk_error ( context, "%s", snd_strerror ( err ) );
                    break;
                }

                usleep ( 100000 );

            } else
            {
                done += err;
            }
        }
    }

    /* Drain sound output */
    if ( ( err1 = snd_pcm_drain ( playback_handle ) ) < 0 )
    {
        nettalk_error ( context, "speaker drain failed" );
        nettalk_error ( context, "%s", snd_strerror ( err1 ) );
        if ( !err )
        {
            err = err1;
        }
        goto exit;
    }

    nettalk_info ( context, "speaker disabled" );

  exit:

    /* Uninitialize audio decoder */
    speaker->decoder->free_callback ( speaker->decoder );

    /* Free PCM buffer */
    free ( buffer );

    /* Free ALSA HW params */
    if ( hw_params )
    {
        snd_pcm_hw_params_free ( hw_params );
    }

    /* Close ALSA device */
    snd_pcm_close ( playback_handle );

    return err;
}

/**
 * Forward text messages only
 */
static void playback_fallback ( struct nettalk_context_t *context )
{
    ssize_t len;
    size_t input_pos;
    size_t input_len = 0;
    struct pollfd fds[1];
    unsigned char left[AMRNB_CHUNK_MAX];
    unsigned char input[4096];

    /* Message forward loop */
    while ( !context->playback_status )
    {
        /* Prepare poll events */
        fds[0].fd = context->bridge.u.s.local;
        fds[0].events = POLLERR | POLLHUP | POLLIN;

        /* Poll events */
        if ( poll ( fds, 1, 100 ) > 0 )
        {
            if ( fds[0].revents & ( POLLERR | POLLHUP ) )
            {
                break;
            }

            /* Receive input data */
            if ( ( len =
                    recv_with_reset ( context, context->bridge.u.s.local, input + input_len,
                        sizeof ( input ) - input_len, -1 ) ) <= 0 )
            {
                break;
            }

            input_len += len;

            /* At least max size of AMR-NB chunk required */
            if ( input_len < AMRNB_CHUNK_MAX )
            {
                continue;
            }

            /* Decode input bytes */
            for ( input_pos = 0; input_pos + AMRNB_CHUNK_MAX <= input_len; input_pos += len )
            {
                if ( !memcmp ( input + input_pos, reset_chunk, sizeof ( reset_chunk ) ) )
                {
                    context->reset_encoder_self = TRUE;
                    len = sizeof ( reset_chunk );

                } else if ( !memcmp ( input + input_pos, noop_chunk, sizeof ( noop_chunk ) ) )
                {
                    len = sizeof ( noop_chunk );

                } else if ( !memcmp ( input + input_pos, text_chunk, 24 ) )
                {
                    if ( write ( context->msgin.u.s.writefd, input + input_pos + 24,
                            AMRNB_CHUNK_MAX - 24 ) < 0 )
                    {
                    }

                    len = AMRNB_CHUNK_MAX;

                } else
                {
                    len = 1;
                }
            }

            /* Save unaligned data for future use */
            if ( ( len = input_len - input_pos ) )
            {
                if ( len > AMRNB_CHUNK_MAX )
                {
                    len = AMRNB_CHUNK_MAX;
                }
                memcpy ( left, input + input_pos, len );
                memcpy ( input, left, len );
            }
            input_len = len;
        }
    }
}

/**
 * Voice Recorder cycle
 */
static void voiceplay_cycle ( struct nettalk_context_t *context )
{
    struct audio_decoder_t decoder;
    struct audio_speaker_t speaker;

    if ( !context->playback_status )
    {
        playback_fallback ( context );
        return;
    }

    memset ( &decoder, '\0', sizeof ( decoder ) );
    decoder.init_callback = nettalk_audio_decoder_init;
    decoder.process_callback = nettalk_decode_audio;
    decoder.free_callback = nettalk_audio_decoder_free;

    memset ( &speaker, '\0', sizeof ( speaker ) );
    strncpy ( speaker.dev, ALSA_DEFAULT_DEV, sizeof ( speaker.dev ) );
    speaker.channels = AUDIO_LAYOUT_MONO;
    speaker.rate = AUDIO_RATE_DEFAULT;
    speaker.format = SND_PCM_FORMAT_FLOAT_LE;
    speaker.decoder = &decoder;

    if ( audioplay_start ( context, &speaker ) < 0 )
    {
        nettalk_info ( context, "audio output not available" );
        context->playback_status = FALSE;
        gettimeofdayv ( &context->playback_status_timestamp );
        playback_fallback ( context );
    }
}

/**
 * Voice Playback entry point
 */
static void *voice_playback_entry ( void *arg )
{
    struct nettalk_context_t *context = ( struct nettalk_context_t * ) arg;

    for ( ;; )
    {
        voiceplay_cycle ( context );
        sleep ( 1 );
    }

    return NULL;
}

/**
 * Voice Playback Task
 */
int voice_playback_launch ( struct nettalk_context_t *context )
{
    long pref;

    /* Start scanner task asynchronously */
    if ( pthread_create ( ( pthread_t * ) & pref, NULL, voice_playback_entry, context ) != 0 )
    {
        return -1;
    }

    return 0;
}
