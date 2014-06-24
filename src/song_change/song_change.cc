/*
 * Audacious: A cross-platform multimedia player.
 * Copyright (c) 2005  Audacious Team
 */
#include <glib.h>
#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <signal.h>
#include <unistd.h>
#include <stdio.h>

#include <string.h>

#include <libaudcore/runtime.h>
#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/playlist.h>
#include <libaudcore/hook.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/tuple.h>

#include "formatter.h"

static bool init (void);
static void cleanup(void);
static void songchange_playback_begin(gpointer unused, gpointer unused2);
static void songchange_playback_end(gpointer unused, gpointer unused2);
static void songchange_playlist_eof(gpointer unused, gpointer unused2);
//static void songchange_playback_ttc(gpointer, gpointer);

typedef struct
{
    char *title;
    char *filename;
}
songchange_playback_ttc_prevs_t;
static songchange_playback_ttc_prevs_t *ttc_prevs = nullptr;

static String cmd_line;
static String cmd_line_after;
static String cmd_line_end;
static String cmd_line_ttc;

static GtkWidget *cmd_warn_label, *cmd_warn_img;

/**
 * Escapes characters that are special to the shell inside double quotes.
 *
 * @param string String to be escaped.
 * @return Given string with special characters escaped. Must be freed with g_free().
 */
static char *escape_shell_chars(const char * string)
{
    const char *special = "$`\"\\";    /* Characters to escape */
    const char *in = string;
    char *out, *escaped;
    int num = 0;

    while (*in != '\0')
        if (strchr(special, *in++))
            num++;

    escaped = g_new(char, strlen(string) + num + 1);

    in = string;
    out = escaped;

    while (*in != '\0') {
        if (strchr(special, *in))
            *out++ = '\\';
        *out++ = *in++;
    }
    *out = '\0';

    return escaped;
}

static void bury_child(int signal)
{
    waitpid(-1, nullptr, WNOHANG);
}

static void execute_command(char *cmd)
{
    const char *argv[4] = {"/bin/sh", "-c", nullptr, nullptr};
    int i;
    argv[2] = cmd;
    signal(SIGCHLD, bury_child);
    if (fork() == 0)
    {
        /* We don't want this process to hog the audio device etc */
        for (i = 3; i < 255; i++)
            close(i);
        execv("/bin/sh", (char **)argv);
    }
}

/* Format codes:
 *
 *   F - frequency (in hertz)
 *   c - number of channels
 *   f - filename (full path)
 *   l - length (in milliseconds)
 *   n - name
 *   r - rate (in bits per second)
 *   s - name (since everyone's used to it)
 *   t - playlist position (%02d)
 *   p - currently playing (1 or 0)
 *   a - artist
 *   b - album
 *   r - track title
 */
/* do_command(): do @cmd after replacing the format codes
   @cmd: command to run */
static void do_command (const char * cmd)
{
    int playlist = aud_playlist_get_playing ();
    int pos = aud_playlist_get_position (playlist);

    char *shstring = nullptr, *temp;
    gboolean playing;
    Formatter *formatter;

    if (cmd && strlen(cmd) > 0)
    {
        formatter = formatter_new();

        String ctitle = aud_playlist_entry_get_title (playlist, pos, FALSE);
        if (ctitle)
        {
            temp = escape_shell_chars (ctitle);
            formatter_associate(formatter, 's', temp);
            formatter_associate(formatter, 'n', temp);
            g_free(temp);
        }
        else
        {
            formatter_associate(formatter, 's', "");
            formatter_associate(formatter, 'n', "");
        }

        String filename = aud_playlist_entry_get_filename (playlist, pos);
        if (filename)
        {
            temp = escape_shell_chars (filename);
            formatter_associate(formatter, 'f', temp);
            g_free(temp);
        }
        else
            formatter_associate(formatter, 'f', "");

        formatter_associate(formatter, 't', str_printf ("%02d", pos + 1));

        int length = aud_playlist_entry_get_length (playlist, pos, FALSE);
        if (length > 0)
            formatter_associate(formatter, 'l', int_to_str (length));
        else
            formatter_associate(formatter, 'l', "0");

        playing = aud_drct_get_playing();
        formatter_associate(formatter, 'p', int_to_str (playing));

        if (playing)
        {
            int brate, srate, chans;
            aud_drct_get_info (& brate, & srate, & chans);
            formatter_associate (formatter, 'r', int_to_str (brate));
            formatter_associate (formatter, 'F', int_to_str (srate));
            formatter_associate (formatter, 'c', int_to_str (chans));
        }

        Tuple tuple = aud_playlist_entry_get_tuple
            (aud_playlist_get_active (), pos, 0);

        String artist = tuple.get_str (FIELD_ARTIST);
        if (artist)
            formatter_associate(formatter, 'a', artist);
        else
            formatter_associate(formatter, 'a', "");

        String album = tuple.get_str (FIELD_ALBUM);
        if (album)
            formatter_associate(formatter, 'b', album);
        else
            formatter_associate(formatter, 'b', "");

        String title = tuple.get_str (FIELD_TITLE);
        if (title)
            formatter_associate(formatter, 'T', title);
        else
            formatter_associate(formatter, 'T', "");

        shstring = formatter_format(formatter, cmd);
        formatter_destroy(formatter);

        if (shstring)
        {
            execute_command(shstring);
            /* FIXME: This can possibly be freed too early */
            g_free(shstring);
        }
    }
}

static void read_config(void)
{
    cmd_line = aud_get_str("song_change", "cmd_line");
    cmd_line_after = aud_get_str("song_change", "cmd_line_after");
    cmd_line_end = aud_get_str("song_change", "cmd_line_end");
    cmd_line_ttc = aud_get_str("song_change", "cmd_line_ttc");
}

static void cleanup(void)
{
    hook_dissociate("playback begin", songchange_playback_begin);
    hook_dissociate("playback end", songchange_playback_end);
    hook_dissociate("playlist end reached", songchange_playlist_eof);
    // hook_dissociate( "playlist set info" , songchange_playback_ttc);

    if ( ttc_prevs != nullptr )
    {
        if ( ttc_prevs->title != nullptr ) g_free( ttc_prevs->title );
        if ( ttc_prevs->filename != nullptr ) g_free( ttc_prevs->filename );
        g_free( ttc_prevs );
        ttc_prevs = nullptr;
    }

    cmd_line = String ();
    cmd_line_after = String ();
    cmd_line_end = String ();
    cmd_line_ttc = String ();

    signal(SIGCHLD, SIG_DFL);
}

static void save_and_close(char * cmd, char * cmd_after, char * cmd_end, char * cmd_ttc)
{
    aud_set_str("song_change", "cmd_line", cmd);
    aud_set_str("song_change", "cmd_line_after", cmd_after);
    aud_set_str("song_change", "cmd_line_end", cmd_end);
    aud_set_str("song_change", "cmd_line_ttc", cmd_ttc);

    cmd_line = String (cmd);
    cmd_line_after = String (cmd_after);
    cmd_line_end = String (cmd_end);
    cmd_line_ttc = String (cmd_ttc);
}

static int check_command(char *command)
{
    const char *dangerous = "fns";
    char *c;
    int qu = 0;

    for (c = command; *c != '\0'; c++)
    {
        if (*c == '"' && (c == command || *(c - 1) != '\\'))
            qu = !qu;
        else if (*c == '%' && !qu && strchr(dangerous, *(c + 1)))
            return -1;
    }
    return 0;
}

static bool init (void)
{
    read_config();

    hook_associate("playback begin", songchange_playback_begin, nullptr);
    hook_associate("playback end", songchange_playback_end, nullptr);
    hook_associate("playlist end reached", songchange_playlist_eof, nullptr);

    ttc_prevs = g_new0(songchange_playback_ttc_prevs_t, 1);
    ttc_prevs->title = nullptr;
    ttc_prevs->filename = nullptr;
    // hook_associate( "playlist set info" , songchange_playback_ttc , ttc_prevs );

    return TRUE;
}

static void songchange_playback_begin(gpointer unused, gpointer unused2)
{
    do_command (cmd_line);
}

static void songchange_playback_end(gpointer unused, gpointer unused2)
{
    do_command (cmd_line_after);
}

#if 0
    static void
songchange_playback_ttc(gpointer plentry_gp, gpointer prevs_gp)
{
    if ( ( aud_ip_state->playing ) && ( strcmp(cmd_line_ttc,"") ) )
    {
        songchange_playback_ttc_prevs_t *prevs = prevs_gp;
        PlaylistEntry *pl_entry = plentry_gp;

        /* same filename but title changed, useful to detect http stream song changes */

        if ( ( prevs->title != nullptr ) && ( prevs->filename != nullptr ) )
        {
            if ( ( pl_entry->filename != nullptr ) && ( !strcmp(pl_entry->filename,prevs->filename) ) )
            {
                if ( ( pl_entry->title != nullptr ) && ( strcmp(pl_entry->title,prevs->title) ) )
                {
                    int pos = aud_drct_pl_get_pos();
                    char *current_file = aud_drct_pl_get_file(pos);
                    do_command(cmd_line_ttc, current_file, pos);
                    g_free(current_file);
                    g_free(prevs->title);
                    prevs->title = g_strdup(pl_entry->title);
                }
            }
            else
            {
                g_free(prevs->filename);
                prevs->filename = g_strdup(pl_entry->filename);
                /* if filename changes, reset title as well */
                if ( prevs->title != nullptr )
                    g_free(prevs->title);
                prevs->title = g_strdup(pl_entry->title);
            }
        }
        else
        {
            if ( prevs->title != nullptr )
                g_free(prevs->title);
            prevs->title = g_strdup(pl_entry->title);
            if ( prevs->filename != nullptr )
                g_free(prevs->filename);
            prevs->filename = g_strdup(pl_entry->filename);
        }
    }
}
#endif

static void songchange_playlist_eof(gpointer unused, gpointer unused2)
{
    do_command (cmd_line_end);
}

typedef struct {
    char * cmd;
    char * cmd_after;
    char * cmd_end;
    char * cmd_ttc;
} SongChangeConfig;

static SongChangeConfig config = {nullptr};

static void configure_ok_cb()
{
    char *cmd, *cmd_after, *cmd_end, *cmd_ttc;
    cmd = g_strdup(config.cmd);
    cmd_after = g_strdup(config.cmd_after);
    cmd_end = g_strdup(config.cmd_end);
    cmd_ttc = g_strdup(config.cmd_ttc);

    if (check_command(cmd) < 0 || check_command(cmd_after) < 0 ||
            check_command(cmd_end) < 0 || check_command(cmd_ttc) < 0)
    {
        gtk_widget_show(cmd_warn_img);
        gtk_widget_show(cmd_warn_label);
    }
    else
    {
        gtk_widget_hide(cmd_warn_img);
        gtk_widget_hide(cmd_warn_label);
        save_and_close(cmd, cmd_after, cmd_end, cmd_ttc);
    }

    g_free(cmd);
    g_free(cmd_after);
    g_free(cmd_end);
    g_free(cmd_ttc);
}

/* static GtkWidget * custom_warning (void) */
static void * custom_warning (void)
{
    GtkWidget *bbox_hbox;
    char * temp;

    bbox_hbox = gtk_hbox_new (FALSE, 6);

    cmd_warn_img = gtk_image_new_from_icon_name("dialog-warning", GTK_ICON_SIZE_MENU);
    gtk_box_pack_start(GTK_BOX(bbox_hbox), cmd_warn_img, FALSE, FALSE, 0);

    temp = g_strdup_printf(_("<span size='small'>Parameters passed to the shell should be encapsulated in quotes. Doing otherwise is a security risk.</span>"));
    cmd_warn_label = gtk_label_new(temp);
    gtk_label_set_markup(GTK_LABEL(cmd_warn_label), temp);
    gtk_label_set_line_wrap(GTK_LABEL(cmd_warn_label), TRUE);
    gtk_box_pack_start(GTK_BOX(bbox_hbox), cmd_warn_label, FALSE, FALSE, 0);
    g_free(temp);

    return bbox_hbox;
}

static const PreferencesWidget settings[] = {
    WidgetLabel (N_("<b>Commands</b>")),

    WidgetLabel (N_("Command to run when starting a new song:")),
    WidgetEntry (0, {VALUE_STRING, & config.cmd, 0, 0, configure_ok_cb}),
    WidgetSeparator (),

    WidgetLabel (N_("Command to run at the end of a song:")),
    WidgetEntry (0, {VALUE_STRING, & config.cmd_after, 0, 0, configure_ok_cb}),
    WidgetSeparator (),

    WidgetLabel (N_("Command to run at the end of the playlist:")),
    WidgetEntry (0, {VALUE_STRING, & config.cmd_end, 0, 0, configure_ok_cb}),
    WidgetSeparator (),

    WidgetLabel (N_("Command to run when song title changes (for network streams):")),
    WidgetEntry (0, {VALUE_STRING, & config.cmd_ttc, 0, 0, configure_ok_cb}),
    WidgetSeparator (),

    WidgetLabel (N_("You can use the following format strings which "
                    "will be substituted before calling the command "
                    "(not all are useful for the end-of-playlist command):\n\n"
                    "%F: Frequency (in hertz)\n"
                    "%c: Number of channels\n"
                    "%f: File name (full path)\n"
                    "%l: Length (in milliseconds)\n"
                    "%n or %s: Song name\n"
                    "%r: Rate (in bits per second)\n"
                    "%t: Playlist position (%02d)\n"
                    "%p: Currently playing (1 or 0)\n"
                    "%a: Artist\n"
                    "%b: Album\n"
                    "%T: Track title")),

    WidgetCustom (custom_warning)
};

static void configure_init(void)
{
    read_config();

    config.cmd = g_strdup(cmd_line);
    config.cmd_after = g_strdup(cmd_line_after);
    config.cmd_end = g_strdup(cmd_line_end);
    config.cmd_ttc = g_strdup(cmd_line_ttc);
}

static void configure_cleanup(void)
{
    g_free(config.cmd);
    config.cmd = nullptr;

    g_free(config.cmd_after);
    config.cmd_after = nullptr;

    g_free(config.cmd_end);
    config.cmd_end = nullptr;

    g_free(config.cmd_ttc);
    config.cmd_ttc = nullptr;
}

static const PluginPreferences preferences = {
    settings,
    ARRAY_LEN (settings),
    configure_init,
    nullptr,  // apply
    configure_cleanup,
};

#define AUD_PLUGIN_NAME        N_("Song Change")
#define AUD_PLUGIN_PREFS       & preferences
#define AUD_PLUGIN_INIT        init
#define AUD_PLUGIN_CLEANUP     cleanup

#define AUD_DECLARE_GENERAL
#include <libaudcore/plugin-declare.h>
