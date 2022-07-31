/* ------------------------------------------------------------------
 * Net Talk- Project Config Header
 * ------------------------------------------------------------------ */

#ifndef NETTALK_CONFIG_H
#define NETTALK_CONFIG_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <sys/time.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <math.h>
#include <stdarg.h>
#include <sys/wait.h>

#include <fxcrypt.h>
#include <mbedtls/pk.h>
#include <mbedtls/aes.h>
#include <libnotify/notify.h>

#define NET_TALK_VERSION "1.01.2"
#define NETTALK_PERS_STRING "NetTalk_" NET_TALK_VERSION
#define DERIVE_N_ROUNDS 50000
#define BUFSIZE 256
#define HOSTLEN BUFSIZE
#define CHANLEN 16
#define MSGSIZE 8192
#define NETTALK_CONN_TIMEOUT 4000
#define NETTALK_SEND_TIMEOUT 4000
#define NETTALK_RECV_TIMEOUT 4000
#define NETTALK_WAIT_TIMEOUT 30000
#define FORWARD_CHUNK_LEN 16384
#define CHAT_HISTORY_NMAX 48

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#define GtkWidget void

#ifdef CONFIG_USE_GTK2
#define VBOX_NEW gtk_vbox_new(0, 0)
#else
#define VBOX_NEW gtk_box_new(GTK_ORIENTATION_VERTICAL, 0)
#endif

#ifdef CONFIG_USE_GTK2
#define HBOX_NEW gtk_hbox_new(0, 0)
#else
#define HBOX_NEW gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0)
#endif

#define g_secure_free_string secure_free_string

#endif
