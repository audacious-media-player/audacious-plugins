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
static void songchange_playback_begin(void *, void *);
static void songchange_playback_end(void *, void *);
static void songchange_playlist_eof(void *, void *);
static void songchange_playback_ttc(void *, void *);

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
    hook_dissociate("title change", songchange_playback_ttc);

    cmd_line = String ();
    cmd_line_after = String ();
    cmd_line_end = String ();
    cmd_line_ttc = String ();

    signal(SIGCHLD, SIG_DFL);
}

static int check_command(const char *command)
{
    const char *dangerous = "fns";
    const char *c;
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
    hook_associate("title change", songchange_playback_ttc, nullptr);

    return TRUE;
}

static void songchange_playback_begin(void *, void *)
{
    do_command (cmd_line);
}

static void songchange_playback_end(void *, void *)
{
    do_command (cmd_line_after);
}

static void songchange_playback_ttc(void *, void *)
{
    do_command (cmd_line_ttc);
}

static void songchange_playlist_eof(void *, void *)
{
    do_command (cmd_line_end);
}

typedef struct {
    String cmd;
    String cmd_after;
    String cmd_end;
    String cmd_ttc;
} SongChangeConfig;

static SongChangeConfig config;

static void edit_cb()
{
    if (check_command(config.cmd) < 0 || check_command(config.cmd_after) < 0 ||
     check_command(config.cmd_end) < 0 || check_command(config.cmd_ttc) < 0)
    {
        gtk_widget_show(cmd_warn_img);
        gtk_widget_show(cmd_warn_label);
    }
    else
    {
        gtk_widget_hide(cmd_warn_img);
        gtk_widget_hide(cmd_warn_label);
    }
}

static void configure_ok_cb()
{
    aud_set_str("song_change", "cmd_line", config.cmd);
    aud_set_str("song_change", "cmd_line_after", config.cmd_after);
    aud_set_str("song_change", "cmd_line_end", config.cmd_end);
    aud_set_str("song_change", "cmd_line_ttc", config.cmd_ttc);

    cmd_line = config.cmd;
    cmd_line_after = config.cmd_after;
    cmd_line_end = config.cmd_end;
    cmd_line_ttc = config.cmd_ttc;
}

/* static GtkWidget * custom_warning (void) */
static void * custom_warning (void)
{
    GtkWidget *bbox_hbox;
    char * temp;

    bbox_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    cmd_warn_img = gtk_image_new_from_icon_name("dialog-warning", GTK_ICON_SIZE_MENU);
    gtk_box_pack_start(GTK_BOX(bbox_hbox), cmd_warn_img, FALSE, FALSE, 0);

    temp = g_strdup_printf(_("<span size='small'>Parameters passed to the shell should be encapsulated in quotes. Doing otherwise is a security risk.</span>"));
    cmd_warn_label = gtk_label_new(temp);
    gtk_label_set_markup(GTK_LABEL(cmd_warn_label), temp);
    gtk_label_set_line_wrap(GTK_LABEL(cmd_warn_label), TRUE);
    gtk_box_pack_start(GTK_BOX(bbox_hbox), cmd_warn_label, FALSE, FALSE, 0);
    g_free(temp);

    gtk_widget_set_no_show_all(cmd_warn_img, TRUE);
    gtk_widget_set_no_show_all(cmd_warn_label, TRUE);

    edit_cb();

    return bbox_hbox;
}

static const PreferencesWidget settings[] = {
    WidgetLabel (N_("<b>Commands</b>")),

    WidgetLabel (N_("Command to run when starting a new song:")),
    WidgetEntry (0, WidgetString (config.cmd, edit_cb)),
    WidgetSeparator ({true}),

    WidgetLabel (N_("Command to run at the end of a song:")),
    WidgetEntry (0, WidgetString (config.cmd_after, edit_cb)),
    WidgetSeparator ({true}),

    WidgetLabel (N_("Command to run at the end of the playlist:")),
    WidgetEntry (0, WidgetString (config.cmd_end, edit_cb)),
    WidgetSeparator ({true}),

    WidgetLabel (N_("Command to run when song title changes (for network streams):")),
    WidgetEntry (0, WidgetString (config.cmd_ttc, edit_cb)),
    WidgetSeparator ({true}),

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

    WidgetCustomGTK (custom_warning)
};

static void configure_init(void)
{
    config.cmd = cmd_line;
    config.cmd_after = cmd_line_after;
    config.cmd_end = cmd_line_end;
    config.cmd_ttc = cmd_line_ttc;
}

static void configure_cleanup(void)
{
    config.cmd = String ();
    config.cmd_after = String ();
    config.cmd_end = String ();
    config.cmd_ttc = String ();
}

static const PluginPreferences preferences = {
    {settings},
    configure_init,
    configure_ok_cb,
    configure_cleanup,
};

#define AUD_PLUGIN_NAME        N_("Song Change")
#define AUD_PLUGIN_PREFS       & preferences
#define AUD_PLUGIN_INIT        init
#define AUD_PLUGIN_CLEANUP     cleanup

#define AUD_DECLARE_GENERAL
#include <libaudcore/plugin-declare.h>
