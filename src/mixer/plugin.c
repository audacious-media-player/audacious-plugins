/*
 * Channel Mixer Plugin for Audacious
 * Copyright 2011 John Lindgren
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

#include <stdlib.h>
#include <gtk/gtk.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudgui/libaudgui-gtk.h>

#include "config.h"
#include "mixer.h"

static const gchar * const mixer_defaults[] = {
 "channels", "2",
  NULL};

int mixer_channels;
float * mixer_buf;

static GtkWidget * about_win, * config_win;

static int mixer_init (void)
{
    aud_config_set_defaults ("mixer", mixer_defaults);
    mixer_channels = aud_get_int ("mixer", "channels");
    return 1;
}

static void mixer_cleanup (void)
{
    if (about_win)
        gtk_widget_destroy (about_win);
    if (config_win)
        gtk_widget_destroy (config_win);

    aud_set_int ("mixer", "channels", mixer_channels);

    free (mixer_buf);
    mixer_buf = 0;
}

static void mixer_about (void)
{
    audgui_simple_message (& about_win, GTK_MESSAGE_INFO,
     _("About Channel Mixer"),
     "Channel Mixer Plugin for Audacious\n"
     "Copyright 2011 John Lindgren\n\n"
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

static void spin_changed (GtkSpinButton * spin, void * data)
{
    * (int *) data = gtk_spin_button_get_value (spin);
}

static void mixer_configure (void)
{
    if (config_win)
    {
        gtk_window_present ((GtkWindow *) config_win);
        return;
    }

    config_win = gtk_dialog_new_with_buttons (_("Channel Mixer Settings"), 0, 0,
     GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
    gtk_window_set_resizable ((GtkWindow *) config_win, 0);

    GtkWidget * vbox = gtk_dialog_get_content_area ((GtkDialog *) config_win);

    GtkWidget * hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start ((GtkBox *) vbox, hbox, 0, 0, 0);

    GtkWidget * label = gtk_label_new (_("Output channels:"));
    gtk_box_pack_start ((GtkBox *) hbox, label, 0, 0, 0);

    GtkWidget * spin = gtk_spin_button_new_with_range (1, MAX_CHANNELS, 1);
    gtk_spin_button_set_value ((GtkSpinButton *) spin, mixer_channels);
    gtk_box_pack_start ((GtkBox *) hbox, spin, 0, 0, 0);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start ((GtkBox *) vbox, hbox, 0, 0, 0);

    label = gtk_label_new (_("Changes take effect at the next song change."));
    gtk_box_pack_start ((GtkBox *) hbox, label, 0, 0, 0);

    g_signal_connect (config_win, "response", (GCallback) gtk_widget_destroy, 0);
    g_signal_connect (config_win, "destroy", (GCallback) gtk_widget_destroyed, & config_win);
    g_signal_connect (spin, "value-changed", (GCallback) spin_changed, & mixer_channels);

    gtk_widget_show_all (config_win);
}

AUD_EFFECT_PLUGIN
(
    .name = "Channel Mixer",
    .init = mixer_init,
    .cleanup = mixer_cleanup,
    .about = mixer_about,
    .configure = mixer_configure,
    .start = mixer_start,
    .process = mixer_process,
    .flush = mixer_flush,
    .finish = mixer_process,
    .order = 2, /* must be before crossfade */
)
