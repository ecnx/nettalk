/* ------------------------------------------------------------------
 * Net Talk - Log Handler
 * ------------------------------------------------------------------ */

#include "nettalk.h"

/**
 * Log info message
 */
void nettalk_info ( struct nettalk_context_t *context, const char *format, ... )
{
    size_t len;
    va_list argp;
    char str[2048];

    va_start ( argp, format );
    if ( context->verbose )
    {
        str[0] = LOG_EVENT_INFO;
        if ( vsnprintf ( str + 1, sizeof ( str ) - 1, format, argp ) >= 0 )
        {
            len = strlen ( str );
            if ( len + 1 < sizeof ( str ) )
            {
                str[len++] = '\a';
                str[len] = '\0';
                if ( write ( context->applog.u.s.writefd, str, len ) >= 0 )
                {
                }
            }
        }
    }
    va_end ( argp );
}

/**
 * Log success message
 */
void nettalk_success ( struct nettalk_context_t *context, const char *format, ... )
{
    size_t len;
    va_list argp;
    char str[2048];

    va_start ( argp, format );
    if ( context->verbose )
    {
        str[0] = LOG_EVENT_SUCCESS;
        if ( vsnprintf ( str + 1, sizeof ( str ) - 1, format, argp ) >= 0 )
        {
            len = strlen ( str );
            if ( len + 1 < sizeof ( str ) )
            {
                str[len++] = '\a';
                str[len] = '\0';
                if ( write ( context->applog.u.s.writefd, str, len ) >= 0 )
                {
                }
            }
        }
    }
    va_end ( argp );
}

/**
 * Log error message
 */
void nettalk_error ( struct nettalk_context_t *context, const char *format, ... )
{
    size_t len;
    va_list argp;
    char str[2048];

    va_start ( argp, format );
    if ( context->verbose )
    {
        str[0] = LOG_EVENT_ERROR;
        if ( vsnprintf ( str + 1, sizeof ( str ) - 1, format, argp ) >= 0 )
        {
            len = strlen ( str );
            if ( len + 1 < sizeof ( str ) )
            {
                str[len++] = '\a';
                str[len] = '\0';
                if ( write ( context->applog.u.s.writefd, str, len ) >= 0 )
                {
                }
            }
        }
    }
    va_end ( argp );
}

/**
 * Log error message with code
 */
void nettalk_errcode ( struct nettalk_context_t *context, const char *message, int errcode )
{
    nettalk_error ( context, "%s (%i)", message, errcode );
}
