/* ------------------------------------------------------------------
 * Net Talk - Voice Compression
 * ------------------------------------------------------------------ */

#include "nettalk.h"
#include "sound.h"

#include <soxr.h>
#include <gsmamr_enc.h>

/**
 * Initialize audio encoder
 */
int nettalk_audio_encoder_init ( struct nettalk_context_t *context,
    struct audio_encoder_t *encoder )
{
    int status;
    soxr_error_t soxr_error;
    soxr_quality_spec_t q_spec;

    /* Begin Initialization */
    encoder->resample_in = NULL;
    encoder->resample_out = NULL;
    encoder->samples = NULL;
    encoder->output = NULL;
    encoder->amrnb = NULL;
    encoder->soxr = NULL;
    encoder->samples_left = 0;

    /* AMR-NB rate is 8kHz */
    encoder->outrate = 8000;

    /* Adjust AMR-NB mode */
    switch ( encoder->bitrate )
    {
    case 4750:
        encoder->amrnb_mode = AMR_475;
        break;
    case 5150:
        encoder->amrnb_mode = AMR_515;
        break;
    case 5900:
        encoder->amrnb_mode = AMR_59;
        break;
    case 6700:
        encoder->amrnb_mode = AMR_67;
        break;
    case 7400:
        encoder->amrnb_mode = AMR_74;
        break;
    case 7950:
        encoder->amrnb_mode = AMR_795;
        break;
    case 10200:
        encoder->amrnb_mode = AMR_102;
        break;
    case 12200:
        encoder->amrnb_mode = AMR_122;
        break;
    default:
        encoder->amrnb_mode = AMR_795;
    }

    /* Prepare buffers allocation */
    if ( !( encoder->output_size = AMRNB_CHUNK_MAX * encoder->frames_max / AMRNB_SAMPLES_MAX ) )
    {
        return -1;
    }

    if ( !( encoder->resample_in =
            ( float * ) malloc ( encoder->frames_max * encoder->channels * sizeof ( float ) ) ) )
    {
        nettalk_errcode ( context, "mic resample-out inalloc failed", errno );
        nettalk_audio_encoder_free ( encoder );
        return -1;
    }

    if ( !( encoder->resample_out =
            ( float * ) malloc ( encoder->frames_max * sizeof ( float ) ) ) )
    {
        nettalk_errcode ( context, "mic resample-out alloc failed", errno );
        nettalk_audio_encoder_free ( encoder );
        return -1;
    }

    if ( !( encoder->samples =
            ( short * ) malloc ( ( encoder->frames_max +
                    AMRNB_SAMPLES_MAX ) * sizeof ( short ) ) ) )
    {
        nettalk_errcode ( context, "mic samples alloc failed", errno );
        nettalk_audio_encoder_free ( encoder );
        return -1;
    }

    /* Allocate buffers */
    if ( !( encoder->output = ( unsigned char * ) malloc ( encoder->output_size ) ) )
    {
        nettalk_errcode ( context, "mic output alloc failed", errno );
        nettalk_audio_encoder_free ( encoder );
        return -1;
    }

    /* Initialize AMR-NB encoder */
    status = AMREncodeInit ( &encoder->amrnb, &encoder->sid_sync, FALSE );

    /* Check for error */
    if ( !encoder->amrnb || status < 0 )
    {
        nettalk_error ( context, "mic amr-nb init failed (%s)", soxr_error );
        nettalk_audio_encoder_free ( encoder );
        return -1;
    }

    /* Select resample filter quality */
    q_spec = soxr_quality_spec ( SOXR_VHQ, SOXR_VR | SOXR_DOUBLE_PRECISION );

    /* Initialize SOXR resampler */
    encoder->soxr =
        soxr_create ( encoder->inrate, encoder->outrate, AUDIO_LAYOUT_MONO,
        &soxr_error, SOXR_FLOAT32_I, &q_spec, NULL );

    /* Check for error */
    if ( !encoder->soxr || soxr_error )
    {
        nettalk_error ( context, "mic soxr init failed (%s)", soxr_error );
        nettalk_audio_encoder_free ( encoder );
        return -1;
    }

    return 0;
}

/**
 * Calculate average from numbers
 */
static float calc_average ( float *values, size_t count )
{
    size_t i;
    float result;

    if ( count )
    {
        for ( i = 0, result = 0; i < count; i++ )
        {
            result += values[i];
        }

        return result / count;
    }

    return 0;
}

/**
 * Shrink sound from multi-channel to mono
 */
static void shrink_to_mono_samples ( size_t channels, float *samples, size_t frames_cnt )
{
    size_t i;
    size_t c;

    if ( channels > AUDIO_LAYOUT_MONO )
    {
        for ( i = 0, c = 0; i < frames_cnt; i++, c += channels )
        {
            samples[i] = calc_average ( samples + c, channels );
        }
    }
}

/**
 * Handle message output
 */
static int handle_message_output ( struct nettalk_context_t *context )
{
    ssize_t len;
    unsigned char buffer[AMRNB_CHUNK_MAX];
    memcpy ( buffer, text_chunk, sizeof ( buffer ) );

    while ( ( len = read ( context->msgout.u.s.readfd, buffer + 24, sizeof ( buffer ) - 24 ) ) > 0 )
    {
        if ( len )
        {
            memset ( buffer + 24 + len, '\0', sizeof ( buffer ) - 24 - len );
        }

        if ( send_complete_with_reset ( context, context->bridge.u.s.local, buffer,
                sizeof ( buffer ), NETTALK_SEND_TIMEOUT ) < 0 )
        {
            return -1;
        }

        if ( write ( context->msgloop.u.s.writefd, buffer + 24, sizeof ( buffer ) - 24 ) < 0 )
        {
        }
    }

    return 0;
}

/**
 * Process audio encoding
 */
int nettalk_encode_audio ( struct nettalk_context_t *context, struct audio_encoder_t *encoder,
    const void *frames, size_t nframes )
{
    ssize_t len;
    size_t frames_cnt;
    size_t output_pos;
    size_t samples_cnt;
    size_t resample_idone;
    size_t resample_odone;
    soxr_error_t soxr_error;
    enum Frame_Type_3GPP ft = ( enum Frame_Type_3GPP ) 0;
    short left[AMRNB_SAMPLES_MAX];

    /* Reset remote peer encoder */
    if ( context->reset_encoder_peer )
    {
        if ( send_complete_with_reset ( context, context->bridge.u.s.local, reset_chunk,
                sizeof ( reset_chunk ), NETTALK_SEND_TIMEOUT ) < 0 )
        {
            return -1;
        }
        context->reset_encoder_peer = 0;
    }

    /* Reset self encoder */
    if ( context->reset_encoder_self )
    {
        if ( AMREncodeReset ( encoder->amrnb, encoder->sid_sync ) < 0 )
        {
            return -1;
        }

        if ( send_complete_with_reset ( context, context->bridge.u.s.local, init_chunk,
                sizeof ( init_chunk ), NETTALK_SEND_TIMEOUT ) < 0 )
        {
            return -1;
        }
        context->reset_encoder_self = 0;
    }

    /* Handle message output */ ;
    if ( handle_message_output ( context ) < 0 )
    {
        return -1;
    }

    /* Validate input frames count */
    if ( nframes > encoder->frames_max )
    {
        return -1;
    }

    /* Calculate samples count */
    samples_cnt = nframes * encoder->channels;

    /* Convert samples to float32 format */
    switch ( encoder->format )
    {
    case SND_PCM_FORMAT_S16_LE:
    case SND_PCM_FORMAT_S16_BE:
        audio_short_to_float_array ( frames, encoder->resample_in, samples_cnt );
        break;
    case SND_PCM_FORMAT_FLOAT_LE:
    case SND_PCM_FORMAT_FLOAT_BE:
        memcpy ( encoder->resample_in, frames, samples_cnt * sizeof ( float ) );
        break;
    case SND_PCM_FORMAT_S32_LE:
    case SND_PCM_FORMAT_S32_BE:
        audio_int_to_float_array ( frames, encoder->resample_in, samples_cnt );
        break;
    default:
        return -1;
    }

    /* Shrink sound from multi-channel to mono */
    if ( encoder->channels != AUDIO_LAYOUT_MONO )
    {
        shrink_to_mono_samples ( encoder->channels, encoder->resample_in, nframes );
    }

    /* Resample source sound */
    soxr_error =
        soxr_process ( encoder->soxr, encoder->resample_in, nframes, &resample_idone,
        encoder->resample_out, encoder->frames_max, &resample_odone );

    /* Check for resampling error */
    if ( soxr_error || resample_odone > encoder->frames_max )
    {
        return -1;
    }

    /* Update sound sample count */
    nframes = resample_odone;

    /* Convert samples from soxr format to AMR-NB format */
    audio_float_to_short_array ( encoder->resample_out, encoder->samples + encoder->samples_left,
        nframes );

    /* Encode samples */
    for ( frames_cnt = 0, output_pos = 0; frames_cnt + AMRNB_SAMPLES_MAX < nframes;
        output_pos += len, frames_cnt += AMRNB_SAMPLES_MAX )
    {
        if ( ( len =
                AMREncode ( encoder->amrnb, encoder->sid_sync,
                    ( enum Mode ) encoder->amrnb_mode, encoder->samples + frames_cnt,
                    encoder->output + output_pos, &ft, AMR_TX_IETF ) ) <= 0 )
        {
            return -1;
        }

        encoder->output[output_pos] |= 0x04;
    }

    /* Keep unconsumed samples */
    if ( ( len = nframes - frames_cnt ) )
    {
        if ( len > AMRNB_SAMPLES_MAX )
        {
            len = AMRNB_SAMPLES_MAX;
        }
        memcpy ( left, encoder->samples + frames_cnt, len );
        memcpy ( encoder->samples, left, len );

    }

    encoder->samples_left = len;

    /* Send output data */
    if ( send_complete_with_reset ( context, context->bridge.u.s.local, encoder->output,
            output_pos, NETTALK_SEND_TIMEOUT ) < 0 )
    {
        return -1;
    }

    return 0;
}

/**
 * Uninitialize audio encoder
 */
void nettalk_audio_encoder_free ( struct audio_encoder_t *encoder )
{
    if ( encoder->amrnb && encoder->sid_sync )
    {
        AMREncodeExit ( &encoder->amrnb, &encoder->sid_sync );
        encoder->amrnb = NULL;
        encoder->sid_sync = NULL;
    }

    if ( encoder->soxr )
    {
        soxr_delete ( encoder->soxr );
        encoder->soxr = NULL;
    }

    free_ref ( ( void ** ) &encoder->resample_in );
    free_ref ( ( void ** ) &encoder->resample_out );
    free_ref ( ( void ** ) &encoder->samples );
    free_ref ( ( void ** ) &encoder->output );
}
