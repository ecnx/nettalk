/* ------------------------------------------------------------------
 * Net Talk - Sound Conversion Stuff
 * ------------------------------------------------------------------ */

#include "nettalk.h"
#include "sound.h"

/**
 * Convert float array to short integer array
 */
void audio_float_to_short_array ( const float *input, short *output, int count )
{
    double scaled_value;

    while ( count )
    {
        count--;
        scaled_value = input[count] * ( 8.0 * 0x10000000 );

        if ( scaled_value >= ( 1.0 * 0x7FFFFFFF ) )
        {
            output[count] = 32767;

        } else if ( scaled_value <= ( -8.0 * 0x10000000 ) )
        {
            output[count] = -32768;

        } else
        {
            output[count] = ( short ) ( lrint ( scaled_value ) >> 16 );
        }
    }
}

/**
 * Convert float array to integer array
 */
void audio_float_to_int_array ( const float *input, int *output, int count )
{
    double scaled_value;

    while ( count )
    {
        count--;
        scaled_value = input[count] * ( 8.0 * 0x10000000 );

        if ( scaled_value >= ( 1.0 * 0x7FFFFFFF ) )
        {
            output[count] = 2147483647;

        } else if ( scaled_value <= ( -8.0 * 0x10000000 ) )
        {
            output[count] = -2147483648;

        } else
        {
            output[count] = lrint ( scaled_value );
        }
    }
}

/**
 * Convert short integer array to float array
 */
void audio_short_to_float_array ( const short *input, float *output, int count )
{
    /* Rescale and save samples values */
    while ( count )
    {
        count--;
        output[count] = ( ( float ) input[count] ) / ( 1.0 * 0x8000 );
    }
}

/**
 * Convert integer array to float array
 */
void audio_int_to_float_array ( const int *input, float *output, int count )
{
    while ( count )
    {
        count--;
        output[count] = ( ( float ) input[count] ) / ( 8.0 * 0x10000000 );
    }
}
