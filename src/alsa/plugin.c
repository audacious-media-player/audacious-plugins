/*
 * ALSA Output Plugin for Audacious
 * Copyright 2009-2010 John Lindgren
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

#include "alsa.h"
#include "config.h"

AUD_OUTPUT_PLUGIN
(
    .name = N_("ALSA Output"),
    .domain = PACKAGE,
    .probe_priority = 5,
    .init = alsa_init,
    .cleanup = alsa_cleanup,
    .open_audio = alsa_open_audio,
    .close_audio = alsa_close_audio,
    .buffer_free = alsa_buffer_free,
    .write_audio = alsa_write_audio,
    .period_wait = alsa_period_wait,
    .drain = alsa_drain,
    .set_written_time = alsa_set_written_time,
    .written_time = alsa_written_time,
    .output_time = alsa_output_time,
    .flush = alsa_flush,
    .pause = alsa_pause,
    .set_volume = alsa_set_volume,
    .get_volume = alsa_get_volume,
    .about = alsa_about,
    .configure = alsa_configure,
)

void alsa_about (void)
{
    static GtkWidget * window = NULL;

    audgui_simple_message (& window, GTK_MESSAGE_INFO, _("About ALSA Output "
     "Plugin"), "ALSA Output Plugin for Audacious\n"
     "Copyright 2009-2010 John Lindgren\n\n"
     "My thanks to William Pitcock, author of the ALSA Output Plugin NG, whose "
     "code served as a reference when the ALSA manual was not enough.\n\n"
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

    audgui_simple_message (& window, GTK_MESSAGE_ERROR, _("ALSA error"),
     message);
    g_free (message);
    return 0;
}

void alsa_error (const gchar * format, ...)
{
    va_list args;
    char * message;

    va_start (args, format);
    message = g_strdup_vprintf (format, args);
    va_end (args);

    g_timeout_add (0, show_error, message);
}
