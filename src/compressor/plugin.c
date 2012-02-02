/*
 * Dynamic Range Compression Plugin for Audacious
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

#include <gtk/gtk.h>

#include "config.h"

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "compressor.h"

/* What is a "normal" volume?  Replay Gain stuff claims to use 89 dB, but what
 * does that translate to in our PCM range?  Does anybody even know? */
static const gchar * const compressor_defaults[] = {
 "center", "0.5",
 "range", "0.5",
 NULL};

static GtkWidget * about_window = NULL;
static GtkWidget * config_window = NULL;

void compressor_config_load (void)
{
    aud_config_set_defaults ("compressor", compressor_defaults);
    compressor_center = aud_get_double ("compressor", "center");
    compressor_range = aud_get_double ("compressor", "range");
}

void compressor_config_save (void)
{
    if (about_window != NULL)
        gtk_widget_destroy (about_window);
    if (config_window != NULL)
        gtk_widget_destroy (config_window);

    aud_set_double ("compressor", "center", compressor_center);
    aud_set_double ("compressor", "range", compressor_range);
}

static void compressor_about (void)
{
    audgui_simple_message (& about_window, GTK_MESSAGE_INFO, _("About Dynamic "
     "Range Compression Plugin"),
     "Dynamic Range Compression Plugin for Audacious\n"
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

static void value_changed (GtkRange * range, void * data)
{
    * (double *) data = gtk_range_get_value (range);
}

static void compressor_configure (void)
{
    if (config_window == NULL)
    {
        GtkWidget * vbox, * hbox, * slider, * button;

        config_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_window_set_type_hint ((GtkWindow *) config_window,
         GDK_WINDOW_TYPE_HINT_DIALOG);
        gtk_window_set_resizable ((GtkWindow *) config_window, FALSE);
        gtk_window_set_title ((GtkWindow *) config_window, _("Dynamic Range "
         "Compressor Preferences"));
        gtk_container_set_border_width ((GtkContainer *) config_window, 6);
        g_signal_connect (config_window, "destroy", (GCallback)
         gtk_widget_destroyed, & config_window);

        vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_add ((GtkContainer *) config_window, vbox);

        hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_pack_start ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

        gtk_box_pack_start ((GtkBox *) hbox, gtk_label_new (_("Center "
         "volume:")), FALSE, FALSE, 0);

        slider = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.1, 1.0, 0.1);
        gtk_range_set_value ((GtkRange *) slider, compressor_center);
        gtk_widget_set_size_request (slider, 100, -1);
        gtk_box_pack_start ((GtkBox *) hbox, slider, FALSE, FALSE, 0);
        g_signal_connect (slider, "value-changed", (GCallback) value_changed,
         & compressor_center);

        hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_pack_start ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

        gtk_box_pack_start ((GtkBox *) hbox, gtk_label_new (_("Dynamic "
         "range:")), FALSE, FALSE, 0);

        slider = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.0, 3.0, 0.1);
        gtk_range_set_value ((GtkRange *) slider, compressor_range);
        gtk_widget_set_size_request (slider, 250, -1);
        gtk_box_pack_start ((GtkBox *) hbox, slider, FALSE, FALSE, 0);
        g_signal_connect (slider, "value-changed", (GCallback) value_changed,
         & compressor_range);

        hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_pack_start ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

        button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
        gtk_box_pack_end ((GtkBox *) hbox, button, FALSE, FALSE, 0);
        gtk_widget_set_can_default (button, TRUE);
        gtk_widget_grab_default (button);
        g_signal_connect_swapped (button, "clicked", (GCallback)
         gtk_widget_destroy, config_window);

        audgui_destroy_on_escape (config_window);

        gtk_widget_show_all (vbox);
    }

    gtk_window_present ((GtkWindow *) config_window);
}

AUD_EFFECT_PLUGIN
(
    .name = "Dynamic Range Compressor",
    .init = compressor_init,
    .cleanup = compressor_cleanup,
    .about = compressor_about,
    .configure = compressor_configure,
    .start = compressor_start,
    .process = compressor_process,
    .flush = compressor_flush,
    .finish = compressor_finish,
    .decoder_to_output_time = compressor_decoder_to_output_time,
    .output_to_decoder_time = compressor_output_to_decoder_time,
    .preserves_format = TRUE,
)
