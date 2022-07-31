/* ------------------------------------------------------------------
 * Net Talk - Application GUI
 * ------------------------------------------------------------------ */

#include "nettalk.h"

#ifdef GtkWidget
#undef GtkWidget
#endif

#include <gtk/gtk.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms-compat.h>

/**
 * Image resources
 */
extern guchar _binary_res_program_icon_jpg_start;
extern guchar _binary_res_program_icon_jpg_end;

/**
 * Load JPEG icon from memory
 */
static GdkPixbuf *load_jpeg_icon ( guchar * start, guchar * end )
{
    size_t size;
    GdkPixbufLoader *loader;

    loader = gdk_pixbuf_loader_new_with_mime_type ( "image/jpeg", NULL );
    size = end - start;
    gdk_pixbuf_loader_write ( loader, start, size, NULL );
    return gdk_pixbuf_loader_get_pixbuf ( loader );
}

/**
 * Load program icon
 */
static GdkPixbuf *load_program_icon ( void )
{
    return load_jpeg_icon ( &_binary_res_program_icon_jpg_start,
        &_binary_res_program_icon_jpg_end );
}

/**
 * Message cell data rendering function
 */
static void message_cell_data_func ( GtkTreeViewColumn * column, GtkCellRenderer * renderer,
    GtkTreeModel * model, GtkTreeIter * iter, gpointer user_data )
{
    int bold;
    float align;
    int smalltext;
    const char *color;
    gchar *value;

    UNUSED ( column );
    UNUSED ( user_data );

    gtk_tree_model_get ( model, iter, TABLE_CHAT_COLUMN, &value, -1 );

    if ( value )
    {
        switch ( value[0] )
        {
        case MESSAGE_TYPE_SELF:
            bold = FALSE;
            align = 1.0;
            smalltext = 0;
            color = "Black";
            break;
        case MESSAGE_TYPE_SELF_TIME:
            bold = FALSE;
            align = 1.0;
            smalltext = 2;
            color = "Gray";
            break;
        case MESSAGE_TYPE_PEER:
            bold = TRUE;
            align = 0.0;
            smalltext = 0;
            color = "Black";
            break;
        case MESSAGE_TYPE_PEER_TIME:
            bold = FALSE;
            align = 0.0;
            smalltext = 2;
            color = "Gray";
            break;
        case MESSAGE_TYPE_COMMON_TIME:
            bold = FALSE;
            align = 0.5;
            smalltext = 2;
            color = "Gray";
            break;
        case MESSAGE_TYPE_LOG:
            bold = FALSE;
            align = 0.5;
            smalltext = 1;
            color = "Gray";
            if ( strlen ( value ) > 2 )
            {
                switch ( value[1] )
                {
                case LOG_EVENT_SUCCESS:
                    color = "Green";
                    bold = TRUE;
                    break;
                case LOG_EVENT_ERROR:
                    color = "Red";
                    bold = TRUE;
                    break;
                }
                value++;
            }
            break;
        default:
            g_object_set ( renderer, "text", value + 1, NULL );
            return;

            g_object_set ( renderer, "text", "<unknown message type>", "weight",
                PANGO_WEIGHT_NORMAL, "xalign", 0.5, "size",
                11 * PANGO_SCALE, "foreground", "Black", NULL );
            return;
        }


        g_object_set ( renderer, "text", value + 1, "weight",
            bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, "xalign", align, "size",
            ( 11 - smalltext ) * PANGO_SCALE, "foreground", color, NULL );
        return;
        g_object_set ( renderer, "text", value + 1, "weight",
            bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, "xalign", align, "size",
            ( 11 - smalltext ) * PANGO_SCALE, "foreground", color, NULL );
    }
}

/**
 * Create chat table model
 */
static GtkTreeModel *create_chattab_model ( void )
{
    GtkListStore *treestore;

    treestore = gtk_list_store_new ( TABLE_NUM_COLS, G_TYPE_STRING );

    return GTK_TREE_MODEL ( treestore );
}

/**
 * Show message notification
 */
static void show_notification ( struct nettalk_context_t *context, const char *message )
{
    GError *error = NULL;

    if ( context->notif )
    {
        notify_notification_close ( context->notif, &error );
        context->notif = NULL;
    }

    if ( ( context->notif =
            notify_notification_new ( "Net Talk - New Message Appeared", message, NULL ) ) )
    {
        context->notexp = time ( NULL ) + 15;
        notify_notification_show ( context->notif, NULL );
    }
}

/**
 * Play message notification sound
 */
static void play_notification_sound ( struct nettalk_context_t *context )
{
    char *beep_args[] = {
        "paplay",
        "/usr/share/sounds/freedesktop/stereo/complete.oga",
        NULL
    };

    if ( !( context->notpid = fork (  ) ) )
    {
        exit ( execvp ( beep_args[0], beep_args ) );
    }
}

/**
 * Create chat table
 */
static GtkWidget *create_chattab ( void )
{
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkWidget *view;
    GtkTreeModel *model;
    char chatname[BUFSIZE];

    view = gtk_tree_view_new (  );
    gtk_tree_view_set_search_column ( GTK_TREE_VIEW ( view ), -1 );
    snprintf ( chatname, sizeof ( chatname ), "Chat Messages" );
    column = gtk_tree_view_column_new (  );
    gtk_tree_view_column_set_title ( column, chatname );
    gtk_tree_view_append_column ( GTK_TREE_VIEW ( view ), column );
    renderer = gtk_cell_renderer_text_new (  );
    gtk_cell_renderer_set_padding ( renderer, 10, 3 );
    g_object_set ( renderer, "wrap-mode", PANGO_WRAP_WORD, NULL );
    g_object_set ( renderer, "wrap-width", 500, NULL );
    gtk_tree_view_column_pack_start ( column, renderer, TRUE );
    gtk_tree_view_column_set_cell_data_func ( column, renderer, message_cell_data_func, NULL,
        NULL );
    model = create_chattab_model (  );
    gtk_tree_view_set_model ( GTK_TREE_VIEW ( view ), model );
    g_object_unref ( model );

    return view;
}

/**
 * Append message timestamp to array
 */
static void append_message_timestamp ( struct nettalk_context_t *context, time_t time )
{
    size_t i;

    for ( i = sizeof ( context->msg_timeouts ) / sizeof ( time_t ) - 1; i > 0; i-- )
    {
        context->msg_timeouts[i] = context->msg_timeouts[i - 1];
    }

    context->msg_timeouts[0] = time;
}

/**
 * Remove oldest message
 */
static void remove_oldest_message ( struct nettalk_context_t *context )
{
    GtkTreeModel *model;
    GtkListStore *store;
    GtkTreePath *path;
    GtkTreeIter iter;

    model = gtk_tree_view_get_model ( GTK_TREE_VIEW ( context->gui.chat ) );
    store = GTK_LIST_STORE ( model );

    path = gtk_tree_path_new_from_indices ( 0, -1 );
    if ( gtk_tree_model_get_iter ( model, &iter, path ) )
    {
        gtk_list_store_remove ( store, &iter );
        context->nmessages--;
    }
}

/**
 * Cleanup timed out messages
 */
static void cleanup_messages ( struct nettalk_context_t *context )
{
    size_t i;
    time_t ts;
    time_t now;

    now = time ( NULL );

    for ( i = 0; i < sizeof ( context->msg_timeouts ) / sizeof ( time_t ); i++ )
    {
        ts = context->msg_timeouts[i];
        if ( ts && ts + 900 < now )
        {
            context->msg_timeouts[i] = 0;
            remove_oldest_message ( context );
        }
    }
}

/**
 * Append message to chat table with custom length
 */
static void put_message_len ( struct nettalk_context_t *context, int type, const char *message,
    size_t len )
{
    char *raw;
    GtkTreeModel *model;
    GtkListStore *store;
    GtkTreePath *path;
    GtkTreeIter iter;

    if ( !( raw = ( char * ) malloc ( len + 2 ) ) )
    {
        return;
    }

    raw[0] = type;
    memcpy ( raw + 1, message, len );
    raw[len + 1] = '\0';

    model = gtk_tree_view_get_model ( GTK_TREE_VIEW ( context->gui.chat ) );
    store = GTK_LIST_STORE ( model );

    if ( context->nmessages > CHAT_HISTORY_NMAX )
    {
        remove_oldest_message ( context );
    }

    append_message_timestamp ( context, time ( NULL ) );

    gtk_list_store_append ( store, &iter );
    gtk_list_store_set ( store, &iter, TABLE_CHAT_COLUMN, raw, -1 );
    path = gtk_tree_path_new_from_indices ( context->nmessages, -1 );

    gtk_tree_view_scroll_to_cell ( GTK_TREE_VIEW ( context->gui.chat ), path, NULL, FALSE, 0, 0 );
    gtk_tree_path_free ( path );

    gtk_tree_view_columns_autosize ( GTK_TREE_VIEW ( context->gui.chat ) );

    context->nmessages++;

    secure_free_mem ( raw, len + 2 );
}

/**
 * Append message to chat table
 */
static void put_message ( struct nettalk_context_t *context, int type, const char *message )
{
    put_message_len ( context, type, message, strlen ( message ) );
}

/**
 * Reply peer with a message
 */
static int reply_message ( struct nettalk_context_t *context, const char *message )
{
    const char delim = '\a';

    if ( context->online )
    {
        if ( write ( context->msgout.u.s.writefd, message, strlen ( message ) ) > 0 )
        {
            if ( write ( context->msgout.u.s.writefd, &delim, sizeof ( delim ) ) > 0 )
            {
                return 0;
            }
        }
    }

    return -1;
}

/**
 * Reply input key down handler
 */
static gboolean reply_on_key_press ( GtkWidget * widget, GdkEventKey * event, gpointer user_data )
{
    const gchar *message;
    struct nettalk_context_t *context;

    UNUSED ( widget );

    if ( !event || !user_data )
    {
        return FALSE;
    }

    context = ( struct nettalk_context_t * ) user_data;

    if ( event->keyval == 0xff0d )
    {
        if ( ( message = gtk_entry_get_text ( GTK_ENTRY ( widget ) ) ) )
        {
            if ( reply_message ( context, message ) >= 0 )
            {
                gtk_entry_set_text ( GTK_ENTRY ( widget ), "" );
            }

            return TRUE;
        }
    }

    return FALSE;
}

/**
 * Put time tag for a message
 */
static void put_timetag ( struct nettalk_context_t *context, int type )
{
    time_t time_stamp;
    struct tm time_struct;
    char message[256];

    /* Get current time */
    time_stamp = time ( NULL );

    /* Get time structure */
    if ( !localtime_r ( &time_stamp, &time_struct ) )
    {
        return;
    }

    /* Format time to a string */
    strftime ( message, sizeof ( message ) - 1, "%Y-%m-%d %H:%M:%S", &time_struct );

    /* Put the message */
    put_message ( context, type, message );
}

/**
 * Forward message from pipe
 */
static void forward_message_pipe ( struct nettalk_context_t *context, int type, int fd,
    struct nettalk_msgbuf_t *msgbuf )
{
    char c;
    int timetag_type;

    switch ( type )
    {
    case MESSAGE_TYPE_SELF:
        timetag_type = MESSAGE_TYPE_SELF_TIME;
        break;
    case MESSAGE_TYPE_PEER:
        timetag_type = MESSAGE_TYPE_PEER_TIME;
        break;
    default:
        timetag_type = MESSAGE_TYPE_COMMON_TIME;
    }

    for ( ;; )
    {
        if ( read ( fd, &c, sizeof ( c ) ) <= 0 )
        {
            break;
        }

        if ( c == '\a' )
        {
            msgbuf->array[msgbuf->len] = '\0';

            put_message_len ( context, type, msgbuf->array, msgbuf->len );
            msgbuf->len = 0;
            put_timetag ( context, timetag_type );

            if ( context->notena && type == MESSAGE_TYPE_PEER )
            {
                if ( !gtk_window_is_active ( context->gui.window ) )
                {
                    show_notification ( context, msgbuf->array );
                }

                play_notification_sound ( context );
            }

        } else if ( c )
        {
            if ( msgbuf->len + 1 >= sizeof ( msgbuf->array ) )
            {
                msgbuf->array[msgbuf->len] = '\0';
                put_message_len ( context, type, msgbuf->array, msgbuf->len );
                msgbuf->len = 0;
                put_timetag ( context, timetag_type );
            }

            msgbuf->array[msgbuf->len++] = c;
        }
    }
}

/**
 * Compare two timeval objects
 */
static int timeval_compare ( const volatile struct timeval *a, const volatile struct timeval *b )
{
    if ( a->tv_sec == b->tv_sec )
    {
        return a->tv_usec - b->tv_usec;
    }

    return a->tv_sec - b->tv_sec;
}

/**
 * Copy timeval object
 */
static void timeval_copy ( const volatile struct timeval *src, volatile struct timeval *dst )
{
    dst->tv_sec = src->tv_sec;
    dst->tv_usec = src->tv_usec;
}

/**
 * Clear all chat messages
 */
static void clear_messages ( struct nettalk_context_t *context )
{
    GtkTreeModel *model;
    GtkListStore *store;

    model = gtk_tree_view_get_model ( GTK_TREE_VIEW ( context->gui.chat ) );
    store = GTK_LIST_STORE ( model );
    gtk_list_store_clear ( store );
    gtk_tree_view_columns_autosize ( GTK_TREE_VIEW ( context->gui.chat ) );
    context->nmessages = 0;
    memset ( &context->msg_timeouts, '\0', sizeof ( context->msg_timeouts ) );
}

/**
 * Attempt to decrypt config and connect
 */
static void decrypt_config_and_connect ( struct nettalk_context_t *context, const char *password )
{
    if ( load_config ( context, context->confpath, password ) < 0 )
    {
        gtk_widget_show ( context->gui.autherr );
        return;
    }

    if ( gettimeofday ( &context->alarm_timestamp, NULL ) < 0 )
    {
        context->alarm_timestamp.tv_sec = 0;
        context->alarm_timestamp.tv_usec = 0;
    }

    nettalk_success ( context, "loaded config from file" );

    if ( nettask_launch ( context ) < 0 )
    {
        gtk_main_quit (  );
        return;
    }

    gtk_widget_hide ( context->gui.authpanel );
    gtk_widget_show ( context->gui.talkpanel );
    gtk_widget_grab_focus ( context->gui.reply );
    context->complete = TRUE;
}

/**
 * Attempt to read password from stdin
 */
static char *read_stdin_password ( void )
{
    int length;
    char *password;

    /* Get password length */
    if ( ioctl ( 0, FIONREAD, &length ) < 0 )
    {
        return NULL;
    }

    /* Check if password was provided */
    if ( !length )
    {
        return NULL;
    }

    /* Allocate password buffer */
    if ( !( password = ( char * ) malloc ( length + 1 ) ) )
    {
        return NULL;
    }

    /* Read password from stdin */
    if ( read ( 0, password, length ) < length )
    {
        free ( password );
        return NULL;
    }

    /* Trim new lines at the end */
    while ( length > 1 )
    {
        if ( password[length - 1] == '\n' )
        {
            length--;

        } else
        {
            break;
        }
    }

    /* Put string terminator */
    password[length] = '\0';

    return password;
}

/**
 * Get basename from file path
 */
static const char *get_basename ( const char *input )
{
    const char *result;
    const char *ptr;

    result = input;

    for ( ptr = input; *ptr; ptr++ )
    {
        if ( *ptr == '/' )
        {
            result = ptr + 1;
        }
    }

    return result;
}

/**
 * Update window title
 */
static void update_window_title ( struct nettalk_context_t *context )
{
    char title[BUFSIZE];

    snprintf ( title, sizeof ( title ), "Net Talk [%s]%s", get_basename ( context->confpath ),
        context->online ? " online" : "" );
    gtk_window_set_title ( GTK_WINDOW ( context->gui.window ), title );
}

/**
 * Update window
 */
static gboolean update_window ( gpointer user_data )
{
    int status;
    char *password;
    GError *error = NULL;
    struct nettalk_context_t *context;
    struct timeval now;

    if ( !user_data )
    {
        return FALSE;
    }

    context = ( struct nettalk_context_t * ) user_data;

    if ( !context->complete )
    {
        if ( ( password = read_stdin_password (  ) ) )
        {
            decrypt_config_and_connect ( context, password );
            free ( password );
        }

        return TRUE;
    }

    forward_message_pipe ( context, MESSAGE_TYPE_PEER, context->msgin.u.s.readfd, &context->bufin );
    forward_message_pipe ( context, MESSAGE_TYPE_SELF, context->msgloop.u.s.readfd,
        &context->bufloop );
    forward_message_pipe ( context, MESSAGE_TYPE_LOG, context->applog.u.s.readfd,
        &context->buflog );

    if ( context->playback_status != context->playback_preset )
    {
        status =
            timeval_compare ( &context->playback_status_timestamp,
            &context->playback_preset_timestamp );

        if ( status > 0 )
        {
            context->playback_preset = context->playback_status;
            timeval_copy ( &context->playback_status_timestamp,
                &context->playback_preset_timestamp );
            gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( context->gui.speakerctl ),
                context->playback_status );

        } else if ( status < 0 )
        {
            context->playback_status = context->playback_preset;
            timeval_copy ( &context->playback_preset_timestamp,
                &context->playback_status_timestamp );
        }
    }

    if ( context->capture_status != context->capture_preset )
    {
        status =
            timeval_compare ( &context->capture_status_timestamp,
            &context->capture_preset_timestamp );

        if ( status > 0 )
        {
            context->capture_preset = context->capture_status;
            timeval_copy ( &context->capture_status_timestamp, &context->capture_preset_timestamp );
            gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( context->gui.micctl ),
                context->capture_status );

        } else if ( status < 0 )
        {
            context->capture_status = context->capture_preset;
            timeval_copy ( &context->capture_preset_timestamp, &context->capture_status_timestamp );
        }
    }

    if ( context->notpid >= 0 )
    {
        waitpid ( context->notpid, &status, WNOHANG );
    }

    if ( context->notif && context->notexp <= time ( NULL ) )
    {
        notify_notification_close ( context->notif, &error );
        context->notif = NULL;
    }

    if ( context->online )
    {
        if ( !context->gui.editable )
        {
            gtk_widget_set_sensitive ( context->gui.reply, TRUE );
            gtk_entry_set_text ( GTK_ENTRY ( context->gui.reply ), "" );
            gtk_widget_grab_focus ( context->gui.reply );
            context->gui.editable = TRUE;
            update_window_title ( context );
        }

    } else
    {
        if ( context->gui.editable )
        {
            gtk_widget_set_sensitive ( context->gui.reply, FALSE );
            gtk_entry_set_text ( GTK_ENTRY ( context->gui.reply ), "Not connected with peer..." );
            context->gui.editable = FALSE;
            update_window_title ( context );
        }
    }

    if ( gettimeofday ( &now, NULL ) >= 0 )
    {
        if ( labs ( ( long ) ( context->alarm_timestamp.tv_sec - now.tv_sec ) ) > 2 )
        {
            clear_messages ( context );
        }
        context->alarm_timestamp.tv_sec = now.tv_sec;
        context->alarm_timestamp.tv_usec = now.tv_usec;
    }

    cleanup_messages ( context );

    return TRUE;
}

/**
 * Microphone controll callback
 */
static void micctl_toggled_callback ( GtkToggleButton * toggle_button, gpointer user_data )
{
    struct nettalk_context_t *context;

    if ( toggle_button && user_data )
    {
        context = ( struct nettalk_context_t * ) user_data;
        context->capture_preset = gtk_toggle_button_get_active ( toggle_button );
        gettimeofdayv ( &context->capture_preset_timestamp );
    }
}

/**
 * Speaker controll callback
 */
static void speakerctl_toggled_callback ( GtkToggleButton * toggle_button, gpointer user_data )
{
    struct nettalk_context_t *context;

    if ( toggle_button && user_data )
    {
        context = ( struct nettalk_context_t * ) user_data;
        context->playback_preset = gtk_toggle_button_get_active ( toggle_button );
        gettimeofdayv ( &context->playback_preset_timestamp );
    }
}

/**
 * Notifications controll callback
 */
static void notctl_toggled_callback ( GtkToggleButton * toggle_button, gpointer user_data )
{
    int newstatus;
    struct nettalk_context_t *context;

    if ( toggle_button && user_data )
    {
        context = ( struct nettalk_context_t * ) user_data;
        newstatus = gtk_toggle_button_get_active ( toggle_button );
        if ( context->notena != newstatus )
        {
            nettalk_info ( context, newstatus ? "notifications enabled" : "notena disabled" );
        }
        context->notena = newstatus;
    }
}

/**
 * Add new item to menu bar
 */
static void add_menu_item ( struct nettalk_context_t *context, GtkWidget * submenu,
    const char *name, GCallback callback, GtkAccelGroup * accel_group, gint shortcut, gint mask )
{
    GtkWidget *menu_item;
    menu_item = gtk_menu_item_new_with_label ( name );
    g_signal_connect ( menu_item, "activate", callback, context );
    gtk_widget_add_accelerator ( menu_item, "activate", accel_group, shortcut, mask,
        GTK_ACCEL_VISIBLE );
    gtk_menu_shell_append ( GTK_MENU_SHELL ( submenu ), menu_item );
}

/**
 * Add new check item to menu bar
 */
static void add_menu_check_item ( struct nettalk_context_t *context, GtkWidget * submenu,
    const char *name, GCallback callback, GtkAccelGroup * accel_group, gint shortcut, gint mask )
{
    GtkWidget *menu_item;
    menu_item = gtk_check_menu_item_new_with_label ( name );
    g_signal_connect ( menu_item, "activate", callback, context );
    gtk_widget_add_accelerator ( menu_item, "activate", accel_group, shortcut, mask,
        GTK_ACCEL_VISIBLE );
    gtk_menu_shell_append ( GTK_MENU_SHELL ( submenu ), menu_item );
}

/**
 * File Reconnect menu handler
 */
static void menu_reconnect ( GtkMenuItem * menu_item, gpointer user_data )
{
    struct nettalk_context_t *context;

    UNUSED ( menu_item );

    if ( user_data )
    {
        context = ( struct nettalk_context_t * ) user_data;

        if ( !context->complete )
        {
            return;
        }

        reconnect_session ( context );
    }
}

/**
 * File Quit menu handler
 */
static void menu_quit ( GtkMenuItem * menu_item, gpointer user_data )
{
    UNUSED ( menu_item );
    UNUSED ( user_data );
    gtk_main_quit (  );
}

/**
 * Edit Clear Messages menu handler
 */
static void menu_clear ( GtkMenuItem * menu_item, gpointer user_data )
{
    struct nettalk_context_t *context;

    UNUSED ( menu_item );

    if ( user_data )
    {
        context = ( struct nettalk_context_t * ) user_data;

        if ( !context->complete )
        {
            return;
        }

        clear_messages ( context );
    }
}

/**
 * Toggle verbose mode menu handler
 */
static void menu_verbose_mode ( GtkMenuItem * menu_item, gpointer user_data )
{
    struct nettalk_context_t *context;

    UNUSED ( menu_item );

    if ( user_data )
    {
        context = ( struct nettalk_context_t * ) user_data;
        context->verbose = !context->verbose;
    }
}

/**
 * Copy message internal
 */
static void menu_copy_internal ( struct nettalk_context_t *context )
{
    gchar *value;
    gchar *message;
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    GtkClipboard *clipboard;

    if ( !context->complete )
    {
        return;
    }

    if ( ( selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW ( context->gui.chat ) ) ) )
    {
        if ( gtk_tree_selection_get_selected ( selection, &model, &iter ) )
        {
            gtk_tree_model_get ( model, &iter, TABLE_CHAT_COLUMN, &value, -1 );
            if ( value )
            {
                message = value;

                switch ( message[0] )
                {
                case MESSAGE_TYPE_PEER:
                case MESSAGE_TYPE_PEER_TIME:
                case MESSAGE_TYPE_SELF:
                case MESSAGE_TYPE_SELF_TIME:
                case MESSAGE_TYPE_COMMON_TIME:
                    message++;
                    break;
                case MESSAGE_TYPE_LOG:
                    message++;
                    if ( message[0] )
                    {
                        message++;
                    }
                    break;
                }

                if ( ( clipboard = gtk_clipboard_get ( GDK_SELECTION_CLIPBOARD ) ) )
                {
                    gtk_clipboard_set_text ( clipboard, message, -1 );
                }

                g_secure_free_string ( value );
            }
        }
    }
}

/**
 * Edit Copy Message menu handler
 */
static void menu_copy ( GtkMenuItem * menu_item, gpointer user_data )
{
    struct nettalk_context_t *context;

    UNUSED ( menu_item );

    if ( user_data )
    {
        context = ( struct nettalk_context_t * ) user_data;
        menu_copy_internal ( context );
    }
}

/**
 * Table Copy Message event handler
 */
static void table_on_row_activated ( GtkTreeView * view, GtkTreePath * path,
    GtkTreeViewColumn * column, gpointer user_data )
{
    struct nettalk_context_t *context;

    UNUSED ( view );
    UNUSED ( path );
    UNUSED ( column );

    if ( user_data )
    {
        context = ( struct nettalk_context_t * ) user_data;
        menu_copy_internal ( context );
    }
}

/**
 * Edit Paste Message menu handler
 */
static void menu_paste ( GtkMenuItem * menu_item, gpointer user_data )
{
    gchar *value;
    GtkClipboard *clipboard;
    struct nettalk_context_t *context;

    UNUSED ( menu_item );

    if ( user_data )
    {
        context = ( struct nettalk_context_t * ) user_data;

        if ( !context->complete )
        {
            return;
        }

        if ( ( clipboard = gtk_clipboard_get ( GDK_SELECTION_CLIPBOARD ) ) )
        {
            if ( ( value = gtk_clipboard_wait_for_text ( clipboard ) ) )
            {
                reply_message ( context, value );
                g_secure_free_string ( value );
            }
        }
    }
}

/**
 * Help About menu handler
 */
static void menu_about ( GtkMenuItem * menu_item, gpointer user_data )
{
    GdkPixbuf *pixbuf;
    GtkWidget *dialog;

    UNUSED ( menu_item );
    UNUSED ( user_data );

    dialog = gtk_about_dialog_new (  );
    gtk_about_dialog_set_program_name ( GTK_ABOUT_DIALOG ( dialog ), "Net Talk" );
    gtk_about_dialog_set_version ( GTK_ABOUT_DIALOG ( dialog ), NET_TALK_VERSION );
    gtk_about_dialog_set_comments ( GTK_ABOUT_DIALOG ( dialog ),
        "Encrypted Peer-to-Peer VoIP + Chat\nSource Code: https://github.com/ecnx/nettalk" );
    pixbuf = load_program_icon (  );
    gtk_about_dialog_set_logo ( GTK_ABOUT_DIALOG ( dialog ), pixbuf );
    g_object_unref ( pixbuf );
    gtk_dialog_run ( GTK_DIALOG ( dialog ) );
    gtk_widget_destroy ( dialog );
}

/**
 * Authorization input key down handler
 */
static gboolean authpass_on_key_press ( GtkWidget * widget, GdkEventKey * event,
    gpointer user_data )
{
    const gchar *password;
    struct nettalk_context_t *context;

    UNUSED ( widget );

    if ( !event || !user_data )
    {
        return FALSE;
    }

    context = ( struct nettalk_context_t * ) user_data;

    if ( event->keyval == 0xff0d )
    {
        if ( ( password = gtk_entry_get_text ( GTK_ENTRY ( context->gui.authpass ) ) ) )
        {
            decrypt_config_and_connect ( context, password );
        }
        return TRUE;
    }

    return FALSE;
}

/**
 * Authorization button click callback
 */
static gboolean auth_button_callback ( GtkWidget * widget, gpointer user_data )
{
    const char *password;
    struct nettalk_context_t *context;

    UNUSED ( widget );

    if ( !user_data )
    {
        return FALSE;
    }

    context = ( struct nettalk_context_t * ) user_data;

    if ( ( password = gtk_entry_get_text ( GTK_ENTRY ( context->gui.authpass ) ) ) )
    {
        decrypt_config_and_connect ( context, password );
    }

    return TRUE;
}

/**
 * Window close handler
 */
static gboolean window_on_deleted ( GtkWidget * widget, GdkEvent * event, gpointer data )
{
    UNUSED ( widget );
    UNUSED ( event );
    UNUSED ( data );
    gtk_main_quit (  );
    return TRUE;
}

/**
 * Initialize application window
 */
int window_init ( struct nettalk_context_t *context )
{
    GtkWidget *menu_bar;
    GtkAccelGroup *accel_group;
    GtkWidget *file_menu;
    GtkWidget *file_submenu;
    GtkWidget *edit_menu;
    GtkWidget *edit_submenu;
    GtkWidget *help_menu;
    GtkWidget *help_submenu;
    GdkPixbuf *pixbuf;
    GtkWidget *authbox;
    GtkWidget *authlabel;
    GtkWidget *authbtn;
    GtkWidget *padboxh;
    GtkWidget *talkbox;
    GtkWidget *optbox;
    GtkWidget *options;
    GtkWidget *notctl;
    GtkWidget *chatbox;
    GtkWidget *replybox;
    GtkWidget *chatscroll;

    gtk_init ( 0, NULL );
    notify_init ( "Nettalk" );

    context->bufin.len = 0;
    context->bufloop.len = 0;
    context->buflog.len = 0;
    context->gui.editable = FALSE;

    context->gui.window = gtk_window_new ( GTK_WINDOW_TOPLEVEL );
    update_window_title ( context );
    gtk_widget_set_size_request ( context->gui.window, 550, 825 );

    pixbuf = load_program_icon (  );
    gtk_window_set_icon ( GTK_WINDOW ( context->gui.window ), pixbuf );
    g_object_unref ( pixbuf );

    authlabel = gtk_label_new ( "Enter config password:" );

    context->gui.authpass = gtk_entry_new (  );
    gtk_entry_set_visibility ( GTK_ENTRY ( context->gui.authpass ), FALSE );
    g_signal_connect ( context->gui.authpass, "key-release-event",
        G_CALLBACK ( authpass_on_key_press ), context );

    authbtn = gtk_button_new_with_label ( "Decrypt & Connect" );
    g_signal_connect ( authbtn, "clicked", G_CALLBACK ( auth_button_callback ), context );

    context->gui.autherr = gtk_label_new ( "Failed to decrypt config" );

    authbox = VBOX_NEW;

    gtk_box_pack_start ( GTK_BOX ( authbox ), authlabel, FALSE, TRUE, 5 );
    gtk_box_pack_start ( GTK_BOX ( authbox ), context->gui.authpass, FALSE, TRUE, 5 );
    gtk_box_pack_start ( GTK_BOX ( authbox ), authbtn, FALSE, TRUE, 5 );
    gtk_box_pack_start ( GTK_BOX ( authbox ), context->gui.autherr, FALSE, TRUE, 5 );

    context->gui.authpanel = HBOX_NEW;
    gtk_box_pack_start ( GTK_BOX ( context->gui.authpanel ), authbox, TRUE, TRUE, 80 );

    menu_bar = gtk_menu_bar_new (  );
    accel_group = gtk_accel_group_new (  );
    gtk_window_add_accel_group ( GTK_WINDOW ( context->gui.window ), accel_group );

    /* File */
    file_menu = gtk_menu_item_new_with_label ( "File" );
    file_submenu = gtk_menu_new (  );

    add_menu_item ( context, file_submenu, "Reconnect", G_CALLBACK ( menu_reconnect ),
        accel_group, GDK_r, GDK_CONTROL_MASK );

    add_menu_item ( context, file_submenu, "Quit", G_CALLBACK ( menu_quit ),
        accel_group, GDK_q, GDK_CONTROL_MASK );

    gtk_menu_item_set_submenu ( GTK_MENU_ITEM ( file_menu ), file_submenu );
    gtk_menu_shell_append ( GTK_MENU_SHELL ( menu_bar ), file_menu );

    /* Edit */
    edit_menu = gtk_menu_item_new_with_label ( "Edit" );
    edit_submenu = gtk_menu_new (  );

    add_menu_item ( context, edit_submenu, "Clear Messages", G_CALLBACK ( menu_clear ),
        accel_group, GDK_F5, 0 );

    add_menu_check_item ( context, edit_submenu, "Verbose Mode", G_CALLBACK ( menu_verbose_mode ),
        accel_group, GDK_F6, 0 );

    add_menu_item ( context, edit_submenu, "Copy Message", G_CALLBACK ( menu_copy ),
        accel_group, GDK_c, GDK_CONTROL_MASK | GDK_SHIFT_MASK );

    add_menu_item ( context, edit_submenu, "Paste Message", G_CALLBACK ( menu_paste ),
        accel_group, GDK_v, GDK_CONTROL_MASK | GDK_SHIFT_MASK );

    gtk_menu_item_set_submenu ( GTK_MENU_ITEM ( edit_menu ), edit_submenu );
    gtk_menu_shell_append ( GTK_MENU_SHELL ( menu_bar ), edit_menu );

    /* Help */
    help_menu = gtk_menu_item_new_with_label ( "Help" );
    help_submenu = gtk_menu_new (  );

    add_menu_item ( context, help_submenu, "About", G_CALLBACK ( menu_about ),
        accel_group, GDK_F2, 0 );

    gtk_menu_item_set_submenu ( GTK_MENU_ITEM ( help_menu ), help_submenu );
    gtk_menu_shell_append ( GTK_MENU_SHELL ( menu_bar ), help_menu );

    context->gui.chat = create_chattab (  );
    g_signal_connect ( context->gui.chat, "row-activated", G_CALLBACK ( table_on_row_activated ),
        context );

    chatscroll = gtk_scrolled_window_new ( NULL, NULL );
    gtk_container_add ( GTK_CONTAINER ( chatscroll ), context->gui.chat );

    chatbox = HBOX_NEW;
    gtk_box_pack_start ( GTK_BOX ( chatbox ), chatscroll, TRUE, TRUE, 0 );

    options = gtk_label_new ( "Options: " );

    context->gui.micctl = gtk_toggle_button_new_with_label ( "Microphone" );
    g_signal_connect ( GTK_TOGGLE_BUTTON ( context->gui.micctl ), "toggled",
        G_CALLBACK ( micctl_toggled_callback ), context );

    context->gui.speakerctl = gtk_toggle_button_new_with_label ( "Speaker" );
    g_signal_connect ( GTK_TOGGLE_BUTTON ( context->gui.speakerctl ), "toggled",
        G_CALLBACK ( speakerctl_toggled_callback ), context );

    notctl = gtk_toggle_button_new_with_label ( "Notifications" );
    g_signal_connect ( GTK_TOGGLE_BUTTON ( notctl ), "toggled",
        G_CALLBACK ( notctl_toggled_callback ), context );
    gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( notctl ), TRUE );

    optbox = HBOX_NEW;
    gtk_box_pack_start ( GTK_BOX ( optbox ), options, TRUE, TRUE, 6 );
    gtk_box_pack_start ( GTK_BOX ( optbox ), context->gui.micctl, TRUE, TRUE, 6 );
    gtk_box_pack_start ( GTK_BOX ( optbox ), context->gui.speakerctl, TRUE, TRUE, 6 );
    gtk_box_pack_start ( GTK_BOX ( optbox ), notctl, TRUE, TRUE, 6 );

    context->gui.reply = gtk_entry_new (  );
    gtk_widget_set_sensitive ( context->gui.reply, FALSE );
    gtk_entry_set_text ( GTK_ENTRY ( context->gui.reply ), "Not connected with peer..." );
    g_signal_connect ( context->gui.reply, "key-release-event", G_CALLBACK ( reply_on_key_press ),
        context );

    replybox = HBOX_NEW;
    gtk_box_pack_start ( GTK_BOX ( replybox ), context->gui.reply, TRUE, TRUE, 0 );

    talkbox = VBOX_NEW;
    gtk_box_set_homogeneous ( GTK_BOX ( talkbox ), FALSE );
    gtk_box_pack_start ( GTK_BOX ( talkbox ), chatbox, TRUE, TRUE, 1 );
    gtk_box_pack_start ( GTK_BOX ( talkbox ), replybox, FALSE, TRUE, 1 );
    gtk_box_pack_end ( GTK_BOX ( talkbox ), optbox, FALSE, TRUE, 1 );

    g_signal_connect ( context->gui.window, "delete-event", G_CALLBACK ( window_on_deleted ),
        NULL );

    context->gui.talkpanel = HBOX_NEW;
    gtk_box_pack_start ( GTK_BOX ( context->gui.talkpanel ), talkbox, TRUE, TRUE, 1 );

    padboxh = VBOX_NEW;
    gtk_box_pack_start ( GTK_BOX ( padboxh ), menu_bar, FALSE, FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( padboxh ), context->gui.talkpanel, TRUE, TRUE, 2 );
    gtk_box_pack_start ( GTK_BOX ( padboxh ), context->gui.authpanel, FALSE, TRUE, 200 );

    gtk_container_add ( GTK_CONTAINER ( context->gui.window ), padboxh );
    gtk_widget_grab_focus ( context->gui.authpass );

    gtk_widget_show_all ( context->gui.window );
    gtk_widget_hide ( context->gui.talkpanel );
    gtk_widget_hide ( context->gui.autherr );

    g_timeout_add ( 100, update_window, context );

    gtk_main (  );

    return 0;
}
