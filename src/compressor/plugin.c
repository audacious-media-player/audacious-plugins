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

#include <audacious/configdb.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "compressor.h"

void compressor_config_load (void)
{
    mcs_handle_t * database = aud_cfg_db_open ();

    aud_cfg_db_get_float (database, "compressor", "target", & compressor_target);
    aud_cfg_db_get_float (database, "compressor", "range", & compressor_range);

    aud_cfg_db_close (database);
}

void compressor_config_save (void)
{
    mcs_handle_t * database = aud_cfg_db_open ();

    aud_cfg_db_set_float (database, "compressor", "target", compressor_target);
    aud_cfg_db_set_float (database, "compressor", "range", compressor_range);

    aud_cfg_db_close (database);
}

static void compressor_about (void)
{
    static GtkWidget * window = NULL;

    audgui_simple_message (& window, GTK_MESSAGE_INFO, _("About Dynamic Range "
     "Compression Plugin"), "Dynamic Range Compression Plugin for Audacious\n"
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
    * (float *) data = gtk_range_get_value (range);
}

static void compressor_configure (void)
{
    static GtkWidget * window = NULL;

    if (window == NULL)
    {
        GtkWidget * vbox, * hbox, * slider, * button;

        window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_window_set_type_hint ((GtkWindow *) window,
         GDK_WINDOW_TYPE_HINT_DIALOG);
        gtk_window_set_resizable ((GtkWindow *) window, FALSE);
        gtk_window_set_title ((GtkWindow *) window, _("Dynamic Range "
         "Compressor Preferences"));
        gtk_container_set_border_width ((GtkContainer *) window, 6);
        g_signal_connect (window, "destroy", (GCallback) gtk_widget_destroyed,
         & window);

        vbox = gtk_vbox_new (FALSE, 6);
        gtk_container_add ((GtkContainer *) window, vbox);

        hbox = gtk_hbox_new (FALSE, 6);
        gtk_box_pack_start ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

        gtk_box_pack_start ((GtkBox *) hbox, gtk_label_new (_("Target "
         "volume:")), FALSE, FALSE, 0);

        slider = gtk_hscale_new_with_range (0.1, 1.0, 0.1);
        gtk_range_set_value ((GtkRange *) slider, compressor_target);
        gtk_widget_set_size_request (slider, 100, -1);
        gtk_box_pack_start ((GtkBox *) hbox, slider, FALSE, FALSE, 0);
        g_signal_connect (slider, "value-changed", (GCallback) value_changed,
         & compressor_target);

        hbox = gtk_hbox_new (FALSE, 6);
        gtk_box_pack_start ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

        gtk_box_pack_start ((GtkBox *) hbox, gtk_label_new (_("Dynamic "
         "range:")), FALSE, FALSE, 0);

        slider = gtk_hscale_new_with_range (0.0, 3.0, 0.1);
        gtk_range_set_value ((GtkRange *) slider, compressor_range);
        gtk_widget_set_size_request (slider, 250, -1);
        gtk_box_pack_start ((GtkBox *) hbox, slider, FALSE, FALSE, 0);
        g_signal_connect (slider, "value-changed", (GCallback) value_changed,
         & compressor_range);

        hbox = gtk_hbox_new (FALSE, 6);
        gtk_box_pack_start ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

        button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
        gtk_box_pack_end ((GtkBox *) hbox, button, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION (2, 18, 0)
        gtk_widget_set_can_default (button, TRUE);
#endif
        gtk_widget_grab_default (button);
        g_signal_connect_swapped (button, "clicked", (GCallback)
         gtk_widget_destroy, window);

        audgui_destroy_on_escape (window);

        gtk_widget_show_all (vbox);
    }

    gtk_window_present ((GtkWindow *) window);
}

EffectPlugin compressor_plugin =
{
    .description = "Dynamic Range Compressor",
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
};

EffectPlugin * compressor_list[] = {& compressor_plugin, NULL};

SIMPLE_EFFECT_PLUGIN (compressor, compressor_list)
