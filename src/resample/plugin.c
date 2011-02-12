/*
 * Sample Rate Converter Plugin for Audacious
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

#include <audacious/configdb.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "config.h"
#include "resample.h"

#if ! GTK_CHECK_VERSION (3, 0, 0)
#define GtkComboBoxText GtkComboBox
#define gtk_combo_box_text_new gtk_combo_box_new_text
#define gtk_combo_box_text_append_text gtk_combo_box_append_text
#endif

int common_rates[] = {8000, 16000, 22050, 44100, 48000, 96000, 192000};
int n_common_rates = sizeof common_rates / sizeof common_rates[0];
int converted_rates[] = {48000, 48000, 44100, 44100, 48000, 96000, 96000};
int fallback_rate = 44100;
int method = SRC_LINEAR;

static GtkWidget * about_window = NULL;
static GtkWidget * config_window = NULL;

void resample_config_load (void)
{
    mcs_handle_t * database = aud_cfg_db_open ();
    if (! database)
        return;

    char scratch[16];
    int count;

    for (count = 0; count < n_common_rates; count ++)
    {
        snprintf (scratch, sizeof scratch, "%d", common_rates[count]);
        aud_cfg_db_get_int (database, "resample", scratch,
         & converted_rates[count]);
    }

    aud_cfg_db_get_int (database, "resample", "method", & method);
    aud_cfg_db_get_int (database, "resample", "fallback_rate", & fallback_rate);

    aud_cfg_db_close (database);
}

void resample_config_save (void)
{
    if (about_window != NULL)
        gtk_widget_destroy (about_window);
    if (config_window != NULL)
        gtk_widget_destroy (config_window);

    mcs_handle_t * database = aud_cfg_db_open ();
    if (! database)
        return;

    char scratch[16];
    int count;

    for (count = 0; count < n_common_rates; count ++)
    {
        snprintf (scratch, sizeof scratch, "%d", common_rates[count]);
        aud_cfg_db_set_int (database, "resample", scratch, converted_rates[count]);
    }

    aud_cfg_db_set_int (database, "resample", "method", method);
    aud_cfg_db_set_int (database, "resample", "fallback_rate", fallback_rate);

    aud_cfg_db_close (database);
}

static void resample_about (void)
{
    audgui_simple_message (& about_window, GTK_MESSAGE_INFO, _("About Sample "
     "Rate Converter Plugin"),
     "Sample Rate Converter Plugin for Audacious\n"
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

static void value_changed (GtkSpinButton * button, void * data)
{
    * (int *) data = gtk_spin_button_get_value (button);
}

static void list_changed (GtkComboBox * list, void * data)
{
    * (int *) data = gtk_combo_box_get_active (list);
}

static GtkWidget * make_method_list (void)
{
    int count;
    const char * name;

    GtkWidget * list = gtk_combo_box_text_new ();

    for (count = 0; (name = src_get_name (count)) != NULL; count ++)
        gtk_combo_box_text_append_text ((GtkComboBoxText *) list, name);

    gtk_combo_box_set_active ((GtkComboBox *) list, method);
    g_signal_connect (list, "changed", (GCallback) list_changed, & method);

    return list;
}

static void resample_configure (void)
{
    if (config_window == NULL)
    {
        GtkWidget * vbox, * hbox, * button;
        char scratch[16];
        int count;

        config_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_window_set_type_hint ((GtkWindow *) config_window,
         GDK_WINDOW_TYPE_HINT_DIALOG);
        gtk_window_set_resizable ((GtkWindow *) config_window, FALSE);
        gtk_window_set_title ((GtkWindow *) config_window, _("Sample Rate "
         "Converter Preferences"));
        gtk_container_set_border_width ((GtkContainer *) config_window, 6);
        g_signal_connect (config_window, "destroy", (GCallback)
         gtk_widget_destroyed, & config_window);

        vbox = gtk_vbox_new (FALSE, 6);
        gtk_container_add ((GtkContainer *) config_window, vbox);

        hbox = gtk_hbox_new (FALSE, 6);
        gtk_box_pack_start ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

        gtk_box_pack_start ((GtkBox *) hbox, gtk_label_new (_("Rate "
         "mappings:")), FALSE, FALSE, 0);

        for (count = 0; count < n_common_rates; count ++)
        {
            hbox = gtk_hbox_new (FALSE, 6);
            gtk_box_pack_start ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

            snprintf (scratch, sizeof scratch, "%d:", common_rates[count]);
            gtk_box_pack_start ((GtkBox *) hbox, gtk_label_new (scratch), FALSE,
             FALSE, 0);

            button = gtk_spin_button_new_with_range (8000, 192000, 50);
            gtk_box_pack_start ((GtkBox *) hbox, button, FALSE, FALSE, 0);
            gtk_spin_button_set_value ((GtkSpinButton *) button,
             converted_rates[count]);
            g_signal_connect (button, "value-changed", (GCallback)
             value_changed, & converted_rates[count]);
        }

        hbox = gtk_hbox_new (FALSE, 6);
        gtk_box_pack_start ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

        gtk_box_pack_start ((GtkBox *) hbox, gtk_label_new (_("All others:")),
         FALSE, FALSE, 0);

        button = gtk_spin_button_new_with_range (8000, 192000, 50);
        gtk_box_pack_start ((GtkBox *) hbox, button, FALSE, FALSE, 0);
        gtk_spin_button_set_value ((GtkSpinButton *) button, fallback_rate);
        g_signal_connect (button, "value-changed", (GCallback)
         value_changed, & fallback_rate);

        hbox = gtk_hbox_new (FALSE, 6);
        gtk_box_pack_start ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

        gtk_box_pack_start ((GtkBox *) hbox, gtk_label_new (_("Method:")),
         FALSE, FALSE, 0);
        gtk_box_pack_start ((GtkBox *) hbox, make_method_list (), FALSE, FALSE,
         0);

        hbox = gtk_hbox_new (FALSE, 6);
        gtk_box_pack_start ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

        button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
        gtk_box_pack_end ((GtkBox *) hbox, button, FALSE, FALSE, 0);
#if GTK_CHECK_VERSION (2, 18, 0)
        gtk_widget_set_can_default (button, TRUE);
#else
        GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
#endif
        gtk_widget_grab_default (button);
        g_signal_connect_swapped (button, "clicked", (GCallback)
         gtk_widget_destroy, config_window);

        audgui_destroy_on_escape (config_window);

        gtk_widget_show_all (vbox);
    }

    gtk_window_present ((GtkWindow *) config_window);
}

EffectPlugin resample_plugin =
{
    .description = "Sample Rate Converter",
    .init = resample_init,
    .cleanup = resample_cleanup,
    .about = resample_about,
    .configure = resample_configure,
    .start = resample_start,
    .process = resample_process,
    .flush = resample_flush,
    .finish = resample_finish,
    .decoder_to_output_time = resample_decoder_to_output_time,
    .output_to_decoder_time = resample_output_to_decoder_time,

    .order = 2, /* must be before crossfade */
};

EffectPlugin * resample_list[] = {& resample_plugin, NULL};

SIMPLE_EFFECT_PLUGIN (resample, resample_list)
