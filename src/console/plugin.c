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
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "configure.h"

Tuple * console_probe_for_tuple(const gchar *filename, VFSFile *fd);
gboolean console_play(InputPlayback *playback, const gchar *filename,
    VFSFile *file, gint start_time, gint stop_time, gboolean pause);
void console_seek(InputPlayback *data, gint time);
void console_stop(InputPlayback *playback);
void console_pause(InputPlayback * playback, gboolean pause);
gboolean console_init (void);
void console_cleanup(void);

static const char console_about[] =
 "Console music decoder engine based on Game_Music_Emu 0.5.2\n"
 "Supported formats: AY, GBS, GYM, HES, KSS, NSF, NSFE, SAP, SPC, VGM, VGZ\n\n"
 "Audacious plugin by:\n"
 "William Pitcock <nenolod@dereferenced.org>\n"
 "Shay Green <gblargg@gmail.com>";

static const gchar *gme_fmts[] = {
    "ay", "gbs", "gym",
    "hes", "kss", "nsf",
    "nsfe", "sap", "spc",
    "vgm", "vgz", NULL
};

AUD_INPUT_PLUGIN
(
    .name = N_("Game Console Music Decoder"),
    .domain = PACKAGE,
    .about_text = console_about,
    .init = console_init,
    .cleanup = console_cleanup,
    .configure = console_cfg_ui,
    .play = console_play,
    .stop = console_stop,
    .pause = console_pause,
    .mseek = console_seek,
    .extensions = gme_fmts,
    .probe_for_tuple = console_probe_for_tuple,
    .have_subtune = TRUE
)
