/* ------------------------------------------------------------------
 * Net Talk - Voice Decompression
 * ------------------------------------------------------------------ */

#include "nettalk.h"
#include "sound.h"

#include <gsmamr_dec.h>

/**
 * Initialize audio decoder
 */
int nettalk_audio_decoder_init ( struct nettalk_context_t *context,
    struct audio_decoder_t *decoder )
{
    int status;
    size_t ratio;
    soxr_error_t soxr_error;
    soxr_quality_spec_t q_spec;
    signed char decoder_name[] = { "Decoder" };

    /* Begin Initialization */
    decoder->input = NULL;
    decoder->samples = NULL;
    decoder->resample_in = NULL;
    decoder->resample_out = NULL;
    decoder->amrnb = NULL;
    decoder->soxr = NULL;
    decoder->input_len = 0;
    decoder->reset_needed = 1;
    context->reset_encoder_peer = 1;

    /* AMR-NB rate is 8kHz */
    decoder->inrate = 8000;

    /* Calculate resample size ratio */
    ratio = decoder->outrate / decoder->inrate;

    /* Prepare buffers allocation */
    if ( !( decoder->input_size =
            ( decoder->frames_max - 2 ) * AMRNB_CHUNK_MIN / ratio / AMRNB_SAMPLES_MAX ) )
    {
        return -1;
    }

    /* Allocate buffers */
    if ( !( decoder->input =
            ( unsigned char * ) malloc ( decoder->frames_max * AMRNB_CHUNK_MIN /
                AMRNB_SAMPLES_MAX ) ) )
    {
        nettalk_errcode ( context, "speaker input alloc failed", errno );
        nettalk_audio_decoder_free ( decoder );
        return -1;
    }

    if ( !( decoder->samples = ( short * ) malloc ( decoder->frames_max * sizeof ( short ) ) ) )
    {
        nettalk_errcode ( context, "speaker samples alloc failed", errno );
        nettalk_audio_decoder_free ( decoder );
        return -1;
    }

    if ( !( decoder->resample_in = ( float * ) malloc ( decoder->frames_max * sizeof ( float ) ) ) )
    {
        nettalk_errcode ( context, "speaker resample-out inalloc failed", errno );
        nettalk_audio_decoder_free ( decoder );
        return -1;
    }

    if ( !( decoder->resample_out =
            ( float * ) malloc ( decoder->frames_max * decoder->channels * sizeof ( float ) ) ) )
    {
        nettalk_errcode ( context, "speaker resample-out alloc failed", errno );
        nettalk_audio_decoder_free ( decoder );
        return -1;
    }

    /* Initialize AMR-NB decoder */
    status = GSMInitDecode ( &decoder->amrnb, decoder_name );

    /* Check for error */
    if ( !decoder->amrnb || status < 0 )
    {
        nettalk_error ( context, "speaker amr-nb init failed (%s)", soxr_error );
        nettalk_audio_decoder_free ( decoder );
        return -1;
    }

    /* Select resample filter quality */
    q_spec = soxr_quality_spec ( SOXR_VHQ, SOXR_VR | SOXR_DOUBLE_PRECISION );

    /* Initialize SOXR resampler */
    decoder->soxr =
        soxr_create ( decoder->inrate, decoder->outrate, AUDIO_LAYOUT_MONO,
        &soxr_error, SOXR_FLOAT32_I, &q_spec, NULL );

    /* Check for error */
    if ( !decoder->soxr || soxr_error )
    {
        nettalk_error ( context, "speaker soxr init failed (%s)", soxr_error );
        nettalk_audio_decoder_free ( decoder );
        return -1;
    }

    return 0;
}

/**
 * Expand sound from mono to multi-channel
 */
static void expand_from_mono_samples ( size_t channels, float *samples, size_t frames_cnt )
{
    size_t i;
    size_t c;
    size_t j;

    if ( channels > AUDIO_LAYOUT_MONO && frames_cnt )
    {
        i = frames_cnt;
        c = i * channels;

        do
        {
            i--;
            c -= channels;

            for ( j = 0; j < channels; j++ )
            {
                samples[c + j] = samples[i];
            }

        } while ( i > 0 );
    }
}

/**
 * Process audio decoding
 */
int nettalk_decode_audio ( struct nettalk_context_t *context, struct audio_decoder_t *decoder,
    void *frames, size_t *nframes )
{
    unsigned char type;
    ssize_t len;
    size_t input_pos;
    size_t frames_cnt;
    size_t samples_cnt;
    size_t resample_idone;
    size_t resample_odone;
    soxr_error_t soxr_error;
    unsigned char left[AMRNB_CHUNK_MAX];

    /* Optionally discard buffer data */
    if ( decoder->reset_needed && decoder->input_len > decoder->input_size / 2 )
    {
        decoder->input_len = 0;
    }

    /* Receive input data */
    if ( ( len =
            recv_with_reset ( context, context->bridge.u.s.local,
                decoder->input + decoder->input_len,
                decoder->input_size - decoder->input_len, -1 ) ) <= 0 )
    {
        return -1;
    }

    decoder->input_len += len;

    /* At least max size of AMR-NB chunk required */
    if ( decoder->input_len < AMRNB_CHUNK_MAX )
    {
        *nframes = 0;
        return 0;
    }

    /* Decode input bytes */
    for ( frames_cnt = 0, input_pos = 0; input_pos + AMRNB_CHUNK_MAX <= decoder->input_len;
        input_pos += len )
    {
        if ( !memcmp ( decoder->input + input_pos, reset_chunk, sizeof ( reset_chunk ) ) )
        {
            context->reset_encoder_self = TRUE;
            len = sizeof ( reset_chunk );

        } else if ( !memcmp ( decoder->input + input_pos, noop_chunk, sizeof ( noop_chunk ) ) )
        {
            len = sizeof ( noop_chunk );

        } else if ( !memcmp ( decoder->input + input_pos, init_chunk, sizeof ( init_chunk ) ) )
        {
            if ( Speech_Decode_Frame_reset ( decoder->amrnb ) < 0 )
            {
                return -1;
            }
            decoder->reset_needed = 0;
            len = sizeof ( reset_chunk );

        } else if ( !memcmp ( decoder->input + input_pos, text_chunk, 24 ) )
        {
            if ( write ( context->msgin.u.s.writefd, decoder->input + input_pos + 24,
                    AMRNB_CHUNK_MAX - 24 ) < 0 )
            {
            }
            len = AMRNB_CHUNK_MAX;

        } else if ( decoder->reset_needed )
        {
            len = 1;

        } else
        {
            type = ( decoder->input[input_pos] >> 3 ) & 0x0f;
            if ( ( len =
                    AMRDecode ( decoder->amrnb, ( enum Frame_Type_3GPP ) type,
                        decoder->input + input_pos + 1, decoder->samples + frames_cnt,
                        MIME_IETF ) ) <= 0 )
            {
                decoder->reset_needed = 1;
                context->reset_encoder_peer = 1;
                *nframes = 0;
                return 0;
            }
            frames_cnt += AMRNB_SAMPLES_MAX;
            len++;
        }
    }

    /* Save unaligned data for future use */
    if ( ( len = decoder->input_len - input_pos ) )
    {
        if ( len > AMRNB_CHUNK_MAX )
        {
            len = AMRNB_CHUNK_MAX;
        }
        memcpy ( left, decoder->input + input_pos, len );
        memcpy ( decoder->input, left, len );
    }
    decoder->input_len = len;

    /* Check if there are any frames */
    if ( !frames_cnt )
    {
        *nframes = 0;
        return 0;
    }

    /* Convert samples from AMR-NB format to soxr format */
    audio_short_to_float_array ( decoder->samples, decoder->resample_in, frames_cnt );

    /* Resample AMR-NB decoded sound */
    soxr_error =
        soxr_process ( decoder->soxr, decoder->resample_in, frames_cnt, &resample_idone,
        decoder->resample_out, decoder->frames_max, &resample_odone );

    /* Check for resampling error */
    if ( soxr_error || resample_odone > decoder->frames_max )
    {
        return -1;
    }

    /* Update sound sample count */
    frames_cnt = resample_odone;

    /* Expand sound from mono to multi-channel */
    if ( decoder->channels != AUDIO_LAYOUT_MONO )
    {
        expand_from_mono_samples ( decoder->channels, decoder->resample_out, frames_cnt );
    }

    /* Check output buffer size */
    if ( *nframes < frames_cnt )
    {
        return -1;
    }

    /* Calculate samples count */
    samples_cnt = frames_cnt * decoder->channels;

    /* Convert samples to output format */
    switch ( decoder->format )
    {
    case SND_PCM_FORMAT_S16_LE:
    case SND_PCM_FORMAT_S16_BE:
        audio_float_to_short_array ( decoder->resample_out, frames, samples_cnt );
        break;
    case SND_PCM_FORMAT_FLOAT_LE:
    case SND_PCM_FORMAT_FLOAT_BE:
        memcpy ( frames, decoder->resample_out, samples_cnt * sizeof ( float ) );
        break;
    case SND_PCM_FORMAT_S32_LE:
    case SND_PCM_FORMAT_S32_BE:
        audio_float_to_int_array ( decoder->resample_out, frames, samples_cnt );
        break;
    default:
        return -1;
    }

    /* Update frame count */
    *nframes = frames_cnt;
    return 0;
}

/**
 * Uninitialize audio decoder
 */
void nettalk_audio_decoder_free ( struct audio_decoder_t *decoder )
{
    if ( decoder->amrnb )
    {
        GSMDecodeFrameExit ( &decoder->amrnb );
        decoder->amrnb = NULL;
    }

    if ( decoder->soxr )
    {
        soxr_delete ( decoder->soxr );
        decoder->soxr = NULL;
    }

    free_ref ( ( void ** ) &decoder->input );
    free_ref ( ( void ** ) &decoder->samples );
    free_ref ( ( void ** ) &decoder->resample_in );
    free_ref ( ( void ** ) &decoder->resample_out );
}
