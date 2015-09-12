/*
 * Audacious: A cross-platform multimedia player.
 * Copyright (c) 2005  Audacious Team
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <signal.h>
#include <unistd.h>

#include <assert.h>
#include <string.h>

#include <libaudcore/runtime.h>
#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/hook.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/tuple.h>

#include "formatter.h"

class SongChange : public GeneralPlugin
{
public:
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("Song Change"),
        PACKAGE,
        nullptr,
        & prefs
    };

    constexpr SongChange () : GeneralPlugin (info, false) {}

    bool init ();
    void cleanup ();
};

EXPORT SongChange aud_plugin_instance;

static String cmd_line;
static String cmd_line_after;
static String cmd_line_end;
static String cmd_line_ttc;

/**
 * Escapes characters that are special to the shell inside double quotes.
 *
 * @param string String to be escaped.
 * @return Given string with special characters escaped.
 */
static StringBuf escape_shell_chars (const char * string)
{
    const char * special = "$`\"\\";    /* Characters to escape */
    const char * in = string;
    char * out;
    int num = 0;

    while (* in != '\0')
        if (strchr (special, * in ++))
            num ++;

    StringBuf escaped (strlen (string) + num);

    in = string;
    out = escaped;

    while (* in != '\0')
    {
        if (strchr (special, * in))
            * out ++ = '\\';
        * out ++ = * in ++;
    }

    // sanity check
    assert (out == escaped + escaped.len ());

    return escaped;
}

static void bury_child (int signal)
{
    waitpid (-1, nullptr, WNOHANG);
}

static void execute_command (const char * cmd)
{
    const char * argv[4] = {"/bin/sh", "-c", nullptr, nullptr};
    argv[2] = cmd;
    signal (SIGCHLD, bury_child);

    if (fork () == 0)
    {
        /* We don't want this process to hog the audio device etc */
        for (int i = 3; i < 255; i ++)
            close (i);
        execv (argv[0], (char * *) argv);
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
    if (cmd && strlen (cmd) > 0)
    {
        Formatter formatter;
        bool playing = aud_drct_get_ready ();

        Tuple tuple;
        if (playing)
            tuple = aud_drct_get_tuple ();

        String ctitle = tuple.get_str (Tuple::FormattedTitle);
        if (ctitle)
        {
            StringBuf temp = escape_shell_chars (ctitle);
            formatter.set ('s', temp);
            formatter.set ('n', temp);
        }
        else
        {
            formatter.set ('s', "");
            formatter.set ('n', "");
        }

        String filename = aud_drct_get_filename ();
        if (filename)
            formatter.set ('f', escape_shell_chars (filename));
        else
            formatter.set ('f', "");

        if (playing)
        {
            int pos = aud_drct_get_position ();
            formatter.set ('t', str_printf ("%02d", pos + 1));
        }
        else
            formatter.set ('t', "");

        int length = tuple.get_int (Tuple::Length);
        if (length > 0)
            formatter.set ('l', int_to_str (length));
        else
            formatter.set ('l', "0");

        formatter.set ('p', int_to_str (playing));

        if (playing)
        {
            int brate, srate, chans;
            aud_drct_get_info (brate, srate, chans);
            formatter.set ('r', int_to_str (brate));
            formatter.set ('F', int_to_str (srate));
            formatter.set ('c', int_to_str (chans));
        }

        String artist = tuple.get_str (Tuple::Artist);
        if (artist)
            formatter.set ('a', artist);
        else
            formatter.set ('a', "");

        String album = tuple.get_str (Tuple::Album);
        if (album)
            formatter.set ('b', album);
        else
            formatter.set ('b', "");

        String title = tuple.get_str (Tuple::Title);
        if (title)
            formatter.set ('T', title);
        else
            formatter.set ('T', "");

        StringBuf shstring = formatter.format (cmd);
        if (shstring)
            execute_command (shstring);
    }
}

static void songchange_playback_begin (void *, void *)
{
    do_command (cmd_line);
}

static void songchange_playback_end (void *, void *)
{
    do_command (cmd_line_after);
}

static void songchange_playback_ttc (void *, void *)
{
    do_command (cmd_line_ttc);
}

static void songchange_playlist_eof (void *, void *)
{
    do_command (cmd_line_end);
}

static void read_config ()
{
    cmd_line = aud_get_str ("song_change", "cmd_line");
    cmd_line_after = aud_get_str ("song_change", "cmd_line_after");
    cmd_line_end = aud_get_str ("song_change", "cmd_line_end");
    cmd_line_ttc = aud_get_str ("song_change", "cmd_line_ttc");
}

bool SongChange::init ()
{
    read_config ();

    hook_associate ("playback ready", songchange_playback_begin, nullptr);
    hook_associate ("playback end", songchange_playback_end, nullptr);
    hook_associate ("playlist end reached", songchange_playlist_eof, nullptr);
    hook_associate ("title change", songchange_playback_ttc, nullptr);

    return true;
}

void SongChange::cleanup ()
{
    hook_dissociate ("playback ready", songchange_playback_begin);
    hook_dissociate ("playback end", songchange_playback_end);
    hook_dissociate ("playlist end reached", songchange_playlist_eof);
    hook_dissociate ("title change", songchange_playback_ttc);

    cmd_line = String ();
    cmd_line_after = String ();
    cmd_line_end = String ();
    cmd_line_ttc = String ();

    signal (SIGCHLD, SIG_DFL);
}

typedef struct {
    String cmd;
    String cmd_after;
    String cmd_end;
    String cmd_ttc;
} SongChangeConfig;

static SongChangeConfig config;

const PreferencesWidget SongChange::widgets[] = {
    WidgetLabel (N_("<b>Commands</b>")),

    WidgetLabel (N_("Command to run when starting a new song:")),
    WidgetEntry (0, WidgetString (config.cmd)),

    WidgetLabel (N_("Command to run at the end of a song:")),
    WidgetEntry (0, WidgetString (config.cmd_after)),

    WidgetLabel (N_("Command to run at the end of the playlist:")),
    WidgetEntry (0, WidgetString (config.cmd_end)),

    WidgetLabel (N_("Command to run when song title changes (for network streams):")),
    WidgetEntry (0, WidgetString (config.cmd_ttc)),

    WidgetLabel (N_("You can use the following format codes, which will be "
                    "replaced before running the command (not all are useful "
                    "for the end-of-playlist command):")),
    WidgetLabel (N_("%a: Artist\n"
                    "%b: Album\n"
                    "%c: Number of channels\n"
                    "%f: File name (full path)\n"
                    "%F: Frequency (Hertz)\n"
                    "%l: Length (milliseconds)\n"
                    "%n or %s: Formatted title (see playlist settings)\n"
                    "%p: Currently playing (1 or 0)\n"
                    "%r: Rate (bits per second)\n"
                    "%t: Playlist position\n"
                    "%T: Title (unformatted)")),
    WidgetLabel (N_("Parameters passed to the shell should be enclosed in "
                    "quotation marks.  Unquoted parameters may lead to "
                    "unexpected results."))
};

static void configure_init ()
{
    config.cmd = cmd_line;
    config.cmd_after = cmd_line_after;
    config.cmd_end = cmd_line_end;
    config.cmd_ttc = cmd_line_ttc;
}

static void configure_ok_cb ()
{
    aud_set_str ("song_change", "cmd_line", config.cmd);
    aud_set_str ("song_change", "cmd_line_after", config.cmd_after);
    aud_set_str ("song_change", "cmd_line_end", config.cmd_end);
    aud_set_str ("song_change", "cmd_line_ttc", config.cmd_ttc);

    cmd_line = config.cmd;
    cmd_line_after = config.cmd_after;
    cmd_line_end = config.cmd_end;
    cmd_line_ttc = config.cmd_ttc;
}

static void configure_cleanup ()
{
    config.cmd = String ();
    config.cmd_after = String ();
    config.cmd_end = String ();
    config.cmd_ttc = String ();
}

const PluginPreferences SongChange::prefs = {
    {widgets},
    configure_init,
    configure_ok_cb,
    configure_cleanup,
};
