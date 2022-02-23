/* ------------------------------------------------------------------
 * Net Talk - Shared Project Header
 * ------------------------------------------------------------------ */

#ifndef NETTALK_H
#define NETTALK_H

#include "config.h"

#define AMRNB_CHUNK_MIN         13
#define AMRNB_CHUNK_MAX         32

/**
 * Log event types
 */
enum
{
    LOG_EVENT_INFO = 'I',
    LOG_EVENT_SUCCESS = 'S',
    LOG_EVENT_ERROR = 'E'
};

/**
 * Column names enumeration
 */
enum
{
    TABLE_CHAT_COLUMN = 0,
    TABLE_NUM_COLS
};

/**
 * Message types
 */
enum
{
    MESSAGE_TYPE_PEER = 'p',
    MESSAGE_TYPE_PEER_TIME = 'q',
    MESSAGE_TYPE_SELF = 's',
    MESSAGE_TYPE_SELF_TIME = 't',
    MESSAGE_TYPE_COMMON_TIME = 'c',
    MESSAGE_TYPE_LOG = 'l'
};

/**
 * Pipe structure
 */
struct pipe_t
{
    union
    {
        struct
        {
            int readfd;
            int writefd;
        } s;
        int fds[2];
    } u;
};

/**
 * Socket pair structure
 */
struct socket_pair_t
{
    union
    {
        struct
        {
            int local;
            int remote;
        } s;
        int fds[2];
    } u;
};

/**
 * Net Talk ACK structure
 */
struct nettalk_ack_t
{
    long long encrypted;
    long long decrypted;
};

/**
 * Net Talk config structure
 */
struct nettalk_config_t
{
    char hostname[HOSTLEN];
    char channel[CHANLEN + 1];
    unsigned short port;
    mbedtls_pk_context self_rsa_priv_key;
    mbedtls_pk_context self_rsa_pub_key;
    mbedtls_pk_context peer_rsa_pub_key;
};

/**
 * Net Talk random generator wrapper
 */
struct nettalk_random_t
{
    int initialized;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
};

/**
 * Net Talk session structure
 */
struct nettalk_session_t
{
    int sock;
    int sndbuf;
    mbedtls_aes_context tx_aes;
    mbedtls_aes_context rx_aes;
    uint8_t tx_iv[AES256_BLOCKLEN];
    uint8_t rx_iv[AES256_BLOCKLEN];
    size_t tx_nleft;
    uint8_t tx_left[FORWARD_CHUNK_LEN];
    size_t rx_nleft;
    uint8_t rx_left[FORWARD_CHUNK_LEN];
};

/**
 * Net Talk message buffer
 */
struct nettalk_msgbuf_t
{
    size_t len;
    char array[BUFSIZE];
};

/**
 * Net Talk GUI
 */
struct nettalk_gui_t
{
    int editable;
    GtkWidget *window;
    GtkWidget *talkpanel;
    GtkWidget *authpanel;
    GtkWidget *chat;
    GtkWidget *micctl;
    GtkWidget *speakerctl;
    GtkWidget *reply;
    GtkWidget *authpass;
    GtkWidget *autherr;
};

/**
 * Net Talk context
 */
struct nettalk_context_t
{
    int complete;
    int verbose;
    volatile int online;
    int nmessages;
    int notena;
    int notpid;
    int notexp;
    int socks5_enabled;
    unsigned int socks5_addr;
    unsigned short socks5_port;
    struct timeval alarm_timestamp;
    NotifyNotification *notif;
    const char *confpath;
    struct nettalk_random_t random;
    struct nettalk_session_t session;
    struct nettalk_config_t config;
    struct nettalk_gui_t gui;
    struct socket_pair_t bridge;
    struct pipe_t msgout;
    struct pipe_t msgin;
    struct nettalk_msgbuf_t bufin;
    struct pipe_t msgloop;
    struct nettalk_msgbuf_t bufloop;
    struct pipe_t applog;
    struct nettalk_msgbuf_t buflog;
    struct pipe_t reset_pipe;
    volatile int capture_status;
    volatile struct timeval capture_status_timestamp;
    volatile int playback_status;
    volatile struct timeval playback_status_timestamp;
    volatile int playback_preset;
    volatile struct timeval capture_preset_timestamp;
    volatile int capture_preset;
    volatile struct timeval playback_preset_timestamp;
    volatile int reset_encoder_self;
    volatile int reset_encoder_peer;
    time_t msg_timeouts[CHAT_HISTORY_NMAX];
};

/**
 * Initialize random generator wrapper
 */
extern int nettalk_random_init ( struct nettalk_random_t *random );

/**
 * Generate random bytes with random generator wrapper
 */
extern int nettalk_random_bytes ( struct nettalk_random_t *random, void *buf, size_t len );

/**
 * Uninitialize random generator wrapper
 */
extern void nettalk_random_free ( struct nettalk_random_t *random );

/**
 * Load config from file
 */
extern int load_config ( struct nettalk_context_t *context, const char *path,
    const char *password );

/**
 * Set socket blocking mode
 */
extern int socket_set_blocking ( int sock );

/**
 * Set socket non-blocking mode
 */
extern int socket_set_nonblocking ( int sock );

/**
 * Connect socket with timeout
 */
extern int connect_timeout ( int sock, struct sockaddr *saddr, size_t saddr_len, int timeout_msec );

/**
 * Write data chunk to fd with reset event
 */
extern ssize_t write_with_reset ( struct nettalk_context_t *context, int sock, const void *arr,
    size_t len, int timeout );

/**
 * Read data chunk from fd with reset event
 */
extern ssize_t read_with_reset ( struct nettalk_context_t *context, int sock, void *arr, size_t len,
    int timeout );

/**
 * Send data chunk via socket with reset event
 */
extern ssize_t send_with_reset ( struct nettalk_context_t *context, int sock, const void *arr,
    size_t len, int timeout );

/**
 * Receive data chunk via socket with reset event
 */
extern ssize_t recv_with_reset ( struct nettalk_context_t *context, int sock, void *arr, size_t len,
    int timeout );

/**
 * Write complete data to fd with reset event
 */
extern int write_complete_with_reset ( struct nettalk_context_t *context, int sock, const void *arr,
    size_t len, int timeout );

/**
 * Read complete data from fd with reset event
 */
extern int read_complete_with_reset ( struct nettalk_context_t *context, int sock, void *arr,
    size_t len, int timeout );

/**
 * Send complete data via socket with reset event
 */
extern int send_complete_with_reset ( struct nettalk_context_t *context, int sock, const void *arr,
    size_t len, int timeout );

/**
 * Receive complete data via socket with reset event
 */
extern int recv_complete_with_reset ( struct nettalk_context_t *context, int sock, void *arr,
    size_t len, int timeout );

/**
 * Shutdown and close the socket
 */
extern void shutdown_then_close ( int sock );

/**
 * Secure move data in memory
 */
extern void secure_move_mem ( void *dst, void *src, size_t size );

/**
 * Secure free data from memory
 */
extern void secure_free_mem ( void *mem, size_t size );

/**
 * Secure free string from memory
 */
extern void secure_free_string ( char *string );

/**
 * Free buffer and set pointer to NULL
 */
extern void free_ref ( void **buffer );

/**
 * Close existing pipe
 */
extern void pipe_close ( struct pipe_t *thepipe );

/**
 * Create new non-blocking pipe
 */
extern int pipe_new_nonblocking ( struct pipe_t *thepipe );

/**
 * Get current time volatile
 */
extern int gettimeofdayv ( volatile struct timeval *tv );

/**
 * AMR-NB compression state reset chunk
 */
extern const uint8_t reset_chunk[AMRNB_CHUNK_MAX];

/**
 * AMR-NB compression state initialize chunk
 */
extern const uint8_t init_chunk[AMRNB_CHUNK_MAX];

/**
 * No operation chunk
 */
extern const uint8_t noop_chunk[AMRNB_CHUNK_MAX];

/**
 * Text message chunk
 */
extern const uint8_t text_chunk[AMRNB_CHUNK_MAX];

/**
 * Text ACK chunk
 */
extern const uint8_t ack_chunk[AMRNB_CHUNK_MAX];

/**
 * Connect with remote peer
 */
extern int nettalk_connect ( struct nettalk_context_t *context );

/**
 * Connect with remote peer
 */
extern int nettalk_handshake ( struct nettalk_context_t *context );

/**
 * Forward application data
 */
extern int nettalk_forward_data ( struct nettalk_context_t *context );

/**
 * Launch Networking Task
 */
extern int nettask_launch ( struct nettalk_context_t *context );

/**
 * Reconnect the session
 */
extern void reconnect_session ( struct nettalk_context_t *context );

/**
 * Check if session reconnect is in progress
 */
extern int session_would_reconnect ( struct nettalk_context_t *context );

/**
 * Initialize application window
 */
extern int window_init ( struct nettalk_context_t *context );

/**
 * Voice Capture Task
 */
extern int voice_capture_launch ( struct nettalk_context_t *context, pthread_t * pthread );

/**
 * Voice Playback Task
 */
extern int voice_playback_launch ( struct nettalk_context_t *context, pthread_t * pthread );

/*
 * Perform Socks5 handshake
 */
extern int socks5_handshake ( struct nettalk_context_t *context, int sock );

/**
 * Request new Socks5 connection
 */
extern int socks5_request_hostname ( struct nettalk_context_t *context, int sock,
    const char *hostname, unsigned short port );

/**
 * Log info message
 */
extern void nettalk_info ( struct nettalk_context_t *context, const char *format, ... );

/**
 * Log success message
 */
extern void nettalk_success ( struct nettalk_context_t *context, const char *format, ... );

/**
 * Log error message
 */
extern void nettalk_error ( struct nettalk_context_t *context, const char *format, ... );

/**
 * Log error message with code
 */
extern void nettalk_errcode ( struct nettalk_context_t *context, const char *message, int errcode );

#endif
