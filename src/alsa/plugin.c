/*
 * ALSA Output Plugin for Audacious
 * Copyright 2009 John Lindgren
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

#include "alsa.h"

#include <stdarg.h>
#include <gtk/gtk.h>

static OutputPlugin plugin =
{
    .description = "ALSA Output Plugin",
    .probe_priority = 2,
    .init = alsa_init,
    .cleanup = alsa_cleanup,
    .open_audio = alsa_open_audio,
    .close_audio = alsa_close_audio,
    .write_audio = alsa_write_audio,
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
};

static OutputPlugin * list[] = {& plugin, NULL};

SIMPLE_OUTPUT_PLUGIN (alsa, list)

void alsa_about (void)
{
    const gchar markup[] = "<b>ALSA Output Plugin for Audacious</b>\n"
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
     "from the use of this software.";

    static GtkWidget * window = NULL;

    if (window == NULL)
    {
        window = gtk_message_dialog_new_with_markup (NULL, 0, GTK_MESSAGE_INFO,
         GTK_BUTTONS_OK, markup);
        g_signal_connect ((GObject *) window, "response", (GCallback)
         gtk_widget_destroy, NULL);
        g_signal_connect ((GObject *) window, "destroy", (GCallback)
         gtk_widget_destroyed, & window);
    }

    gtk_window_present ((GtkWindow *) window);
}

static gboolean show_error (void * message)
{
    static GtkWidget * window = NULL;

    if (window != NULL)
        return FALSE;

    window = gtk_message_dialog_new_with_markup (NULL, 0, GTK_MESSAGE_ERROR,
     GTK_BUTTONS_OK, "<b>%s</b>\n%s", _("ALSA error"), (const gchar *) message);
    g_signal_connect ((GObject *) window, "response", (GCallback)
     gtk_widget_destroy, NULL);
    g_signal_connect ((GObject *) window, "destroy", (GCallback)
     gtk_widget_destroyed, & window);

    gtk_window_present ((GtkWindow *) window);
    g_free (message);
    return FALSE;
}

void alsa_error (const gchar * format, ...)
{
    va_list args;
    gchar * message;

    va_start (args, format);
    message = g_strdup_vprintf (format, args);
    va_end (args);

    g_timeout_add (0, show_error, message);
}
