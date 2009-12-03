/*
 * Audacious: Cross platform multimedia player
 * Copyright (c) 2009 Audacious Team
 *
 * Driver for Game_Music_Emu library. See details at:
 * http://www.slack.net/~ant/libs/
 */


#include "config.h"
#include <glib.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include "configure.h"

Tuple * console_get_song_tuple(const gchar *path);
Tuple * console_probe_for_tuple(const gchar *filename, VFSFile *fd);
void console_play_file(InputPlayback *playback);
void console_seek(InputPlayback *data, gint time);
void console_stop(InputPlayback *playback);
void console_pause(InputPlayback * playback, gshort p);
void console_init(void);
void console_cleanup(void);

static void console_aboutbox(void)
{
    static GtkWidget *aboutbox = NULL;

    if (aboutbox == NULL)
    {
        aboutbox = audacious_info_dialog(
        _("About the Game Console Music Decoder"),
        _("Console music decoder engine based on Game_Music_Emu 0.5.2.\n"
        "Supported formats: AY, GBS, GYM, HES, KSS, NSF, NSFE, SAP, SPC, VGM, VGZ\n"
        "Audacious implementation by: William Pitcock <nenolod@dereferenced.org>, \n"
        "        Shay Green <gblargg@gmail.com>\n"),
        _("Ok"),
        FALSE, NULL, NULL);
        g_signal_connect(G_OBJECT(aboutbox), "destroy",
            G_CALLBACK(gtk_widget_destroyed), &aboutbox);
    }
}

static gchar *gme_fmts[] = {
    "ay", "gbs", "gym",
    "hes", "kss", "nsf",
    "nsfe", "sap", "spc",
    "vgm", "vgz", NULL
};

static InputPlugin console_ip =
{
    .description = "Game Console Music Decoder",
    .init = console_init,
    .cleanup = console_cleanup,
    .about = console_aboutbox,
    .configure = console_cfg_ui,
    .play_file = console_play_file,
    .stop = console_stop,
    .pause = console_pause,
    .seek = console_seek,
    .get_song_tuple = console_get_song_tuple,
    .vfs_extensions = gme_fmts,
    .probe_for_tuple = console_probe_for_tuple,
    .have_subtune = TRUE
};

static InputPlugin *console_iplist[] = { &console_ip, NULL };

SIMPLE_INPUT_PLUGIN(console, console_iplist);
