/*
 * SDL Output Plugin for Audacious
 * Copyright 2010 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <stdarg.h>

#include <gtk/gtk.h>

#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "config.h"
#include "sdlout.h"

AUD_OUTPUT_PLUGIN
(
    .name = N_("SDL Output"),
    .domain = PACKAGE,
    .init = sdlout_init,
    .cleanup = sdlout_cleanup,
    .about = sdlout_about,
    .probe_priority = 1,
    .get_volume = sdlout_get_volume,
    .set_volume = sdlout_set_volume,
    .open_audio = sdlout_open_audio,
    .close_audio = sdlout_close_audio,
    .buffer_free = sdlout_buffer_free,
    .period_wait = sdlout_period_wait,
    .write_audio = sdlout_write_audio,
    .drain = sdlout_drain,
    .written_time = sdlout_written_time,
    .output_time = sdlout_output_time,
    .pause = sdlout_pause,
    .flush = sdlout_flush,
    .set_written_time = sdlout_set_written_time,
)

void sdlout_about (void)
{
    static GtkWidget * window = NULL;

    audgui_simple_message (& window, GTK_MESSAGE_INFO, _("About SDL Output "
     "Plugin"), "SDL Output Plugin for Audacious\n"
     "Copyright 2010 John Lindgren\n\n"
     "Redistribution and use in source and binary forms, with or without "
     "modification, are permitted provided that the following conditions are "
     "met:\n\n"
     "1. Redistributions of source code must retain the above copyright "
     "notice, this list of conditions, and the following disclaimer.\n\n"
     "2. Redistributions in binary form must reproduce the above copyright "
     "notice, this list of conditions, and the following disclaimer in the "
     "documentation provided with the distribution.\n\n"
     "This software is provided \"as is\" and without any warranty, express or "
     "implied. In no event shall the authors be liable for any damages arising "
     "from the use of this software.");
}

static int show_error (void * message)
{
    static GtkWidget * window = NULL;

    audgui_simple_message (& window, GTK_MESSAGE_ERROR, _("SDL error"),
     message);
    g_free (message);
    return 0;
}

void sdlout_error (const gchar * format, ...)
{
    va_list args;
    char * message;

    va_start (args, format);
    message = g_strdup_vprintf (format, args);
    va_end (args);

    g_timeout_add (0, show_error, message);
}
