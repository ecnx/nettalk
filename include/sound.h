/* ------------------------------------------------------------------
 * Net Talk- Voice over IP Support
 * ------------------------------------------------------------------ */

#ifndef NETTALK_SOUND_H
#define NETTALK_SOUND_H

#include <alsa/asoundlib.h>
#include <soxr.h>

#define AUDIO_LAYOUT_MONO       1
#define AUDIO_LAYOUT_STEREO     2
#define AUDIO_RATE_DEFAULT      44100
#define AMRNB_SAMPLES_MAX       160
#define ALSA_DEFAULT_DEV        "default"
#define NETTALK_ENCODE_NCHUNKS  128
#define NETTALK_DECODE_NCHUNKS  2048

/*
 * CMR     MODE        FRAME SIZE( in bytes )
 *  0      AMR 4.75        13
 *  1      AMR 5.15        14 
 *  2      AMR 5.9         16
 *  3      AMR 6.7         18
 *  4      AMR 7.4         20
 *  5      AMR 7.95        21
 *  6      AMR 10.2        27
 *  7      AMR 12.2        32
 */

/**
 * Audio encoder context
 */
struct audio_encoder_t
{
    unsigned int channels;
    unsigned int inrate;
    unsigned int outrate;
    unsigned int bitrate;
    snd_pcm_format_t format;

    size_t frames_max;
    float *resample_in;
    float *resample_out;
    size_t output_size;
    size_t samples_left;
    short *samples;
    unsigned char *output;
    void *amrnb;
    void *sid_sync;
    int amrnb_mode;
    soxr_t soxr;

    int ( *init_callback ) ( struct nettalk_context_t *, struct audio_encoder_t * );
    int ( *process_callback ) ( struct nettalk_context_t *, struct audio_encoder_t *, const void *,
        size_t );
    void ( *free_callback ) ( struct audio_encoder_t * );
};

/**
 * Audio microphone context
 */
struct audio_mic_t
{
    char dev[BUFSIZE];

    unsigned int channels;
    unsigned int rate;
    snd_pcm_format_t format;

    struct audio_encoder_t *encoder;
};

/**
 * Audio decoder context
 */
struct audio_decoder_t
{
    unsigned int channels;
    unsigned int inrate;
    unsigned int outrate;
    snd_pcm_format_t format;

    int reset_needed;
    size_t frames_max;
    size_t input_len;
    size_t input_size;
    unsigned char *input;
    size_t samples_len;
    float *resample_in;
    float *resample_out;
    short *samples;
    void *amrnb;
    soxr_t soxr;

    int ( *init_callback ) ( struct nettalk_context_t *, struct audio_decoder_t * );
    int ( *process_callback ) ( struct nettalk_context_t *, struct audio_decoder_t *, void *,
        size_t * );
    void ( *free_callback ) ( struct audio_decoder_t * );
};

/**
 * Audio speaker context
 */
struct audio_speaker_t
{
    char dev[BUFSIZE];

    unsigned int channels;
    unsigned int rate;
    snd_pcm_format_t format;

    struct audio_decoder_t *decoder;
};

/**
 * Convert float array to short integer array
 */
extern void audio_float_to_short_array ( const float *input, short *output, int count );

/**
 * Convert float array to integer array
 */
extern void audio_float_to_int_array ( const float *input, int *output, int count );

/**
 * Convert short integer array to float array
 */
extern void audio_short_to_float_array ( const short *input, float *output, int count );

/**
 * Convert integer array to float array
 */
extern void audio_int_to_float_array ( const int *input, float *output, int count );

/**
 * Process audio encoding
 */
extern int nettalk_audio_encoder_init ( struct nettalk_context_t *context,
    struct audio_encoder_t *encoder );

/**
 * Process audio encoding
 */
extern int nettalk_encode_audio ( struct nettalk_context_t *context,
    struct audio_encoder_t *encoder, const void *frames, size_t nframes );

/**
 * Uninitialize audio encoder
 */
extern void nettalk_audio_encoder_free ( struct audio_encoder_t *encoder );

/**
 * Process audio decoder
 */
extern int nettalk_audio_decoder_init ( struct nettalk_context_t *context,
    struct audio_decoder_t *decoder );

/**
 * Process audio decoding
 */
extern int nettalk_decode_audio ( struct nettalk_context_t *context,
    struct audio_decoder_t *decoder, void *frames, size_t *nframes );

/**
 * Uninitialize audio decoder
 */
extern void nettalk_audio_decoder_free ( struct audio_decoder_t *decoder );

#endif
