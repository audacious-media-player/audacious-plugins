/*
 * Audacious bs2b effect plugin
 * Copyright (C) 2009, Sebastian Pipping <sebastian@pipping.org>
 * Copyright (C) 2009, Tony Vroon <chainsaw@gentoo.org>
 * Copyright (C) 2010, John Lindgren <john.lindgren@tds.net>
 * Copyright (C) 2011, Michał Lipski <tallica@o2.pl>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gtk/gtk.h>
#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>
#include <audacious/misc.h>
#include <bs2b.h>

static t_bs2bdp bs2b = NULL;
static gint bs2b_channels;
static GtkWidget *config_window, *feed_slider, *fcut_slider;
static const gchar * const bs2b_defaults[] = {
 "feed", "45",
 "fcut", "700",
 NULL};

#define feed_level aud_get_int("bs2b", "feed")
#define fcut_level aud_get_int("bs2b", "fcut")

gboolean init()
{
    aud_config_set_defaults("bs2b", bs2b_defaults);
    bs2b = bs2b_open();

    if (bs2b == NULL)
        return FALSE;

    bs2b_set_level_feed(bs2b, feed_level);
    bs2b_set_level_fcut(bs2b, fcut_level);

    return TRUE;
}

static void cleanup()
{
    if (bs2b == NULL)
        return;

    bs2b_close(bs2b);
    bs2b = NULL;
}

static void bs2b_start (gint * channels, gint * rate)
{
    if (bs2b == NULL)
        return;

    bs2b_channels = * channels;

    if (* channels != 2)
        return;

    bs2b_set_srate (bs2b, * rate);
}

static void bs2b_process (gfloat * * data, gint * samples)
{
    if (bs2b == NULL || bs2b_channels != 2)
        return;

    bs2b_cross_feed_f (bs2b, * data, (* samples) / 2);
}

static void bs2b_flush (void)
{
}

static void bs2b_finish (gfloat * * data, gint * samples)
{
    bs2b_process (data, samples);
}

static gint bs2b_decoder_to_output_time (gint time)
{
    return time;
}

static gint bs2b_output_to_decoder_time (gint time)
{
    return time;
}

static void feed_value_changed(GtkRange *range, gpointer data)
{
    aud_set_int("bs2b", "feed", gtk_range_get_value(range));
    bs2b_set_level_feed(bs2b, feed_level);
}

static gchar *feed_format_value(GtkScale *scale, gdouble value)
{
    return g_strdup_printf("%.1f dB", (float) value / 10);
}

static void fcut_value_changed(GtkRange *range, gpointer data)
{
    aud_set_int("bs2b", "fcut", gtk_range_get_value(range));
    bs2b_set_level_fcut(bs2b, fcut_level);
}

static gchar *fcut_format_value(GtkScale *scale, gdouble value)
{
    return g_strdup_printf("%d Hz, %dµs", (int) value, bs2b_level_delay((int) value));
}

static void preset_button_clicked(GtkButton *button, gpointer data)
{
    gint clevel = GPOINTER_TO_INT(data);
    gtk_range_set_value(GTK_RANGE(feed_slider), clevel >> 16);
    gtk_range_set_value(GTK_RANGE(fcut_slider), clevel & 0xffff);
}

static GtkWidget *preset_button(const gchar *label, gint clevel)
{
    GtkWidget *button = gtk_button_new_with_label(label);
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    g_signal_connect(button, "clicked", (GCallback)
        preset_button_clicked, GINT_TO_POINTER(clevel));

    return button;
}

static void configure (void)
{
    if (config_window == NULL)
    {
        GtkWidget *vbox, *hbox, *button;

        config_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_type_hint ((GtkWindow *) config_window,
                GDK_WINDOW_TYPE_HINT_DIALOG);
        gtk_window_set_resizable ((GtkWindow *) config_window, FALSE);
        gtk_window_set_title ((GtkWindow *) config_window,
                    _("Bauer stereophonic-to-binaural Preferences"));
        gtk_container_set_border_width ((GtkContainer *) config_window, 6);
        g_signal_connect (config_window, "destroy", (GCallback)
                gtk_widget_destroyed, & config_window);

        vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_add ((GtkContainer *) config_window, vbox);

        hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_pack_start ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(_("Feed level:")), TRUE, FALSE, 0);

        feed_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, BS2B_MINFEED, BS2B_MAXFEED, 1.0);
        gtk_range_set_value (GTK_RANGE(feed_slider), feed_level);
        gtk_widget_set_size_request (feed_slider, 200, -1);
        gtk_box_pack_start ((GtkBox *) hbox, feed_slider, FALSE, FALSE, 0);
        g_signal_connect (feed_slider, "value-changed", (GCallback) feed_value_changed,
                NULL);
        g_signal_connect (feed_slider, "format-value", (GCallback) feed_format_value,
                NULL);

        hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_pack_start ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

        gtk_box_pack_start (GTK_BOX(hbox), gtk_label_new(_("Cut frequency:")), TRUE, FALSE, 0);

        fcut_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, BS2B_MINFCUT, BS2B_MAXFCUT, 1.0);
        gtk_range_set_value (GTK_RANGE(fcut_slider), fcut_level);
        gtk_widget_set_size_request (fcut_slider, 200, -1);
        gtk_box_pack_start ((GtkBox *) hbox, fcut_slider, FALSE, FALSE, 0);
        g_signal_connect (fcut_slider, "value-changed", (GCallback) fcut_value_changed,
                NULL);
        g_signal_connect (fcut_slider, "format-value", (GCallback) fcut_format_value,
                NULL);

        hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_pack_start ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);

        gtk_box_pack_start ((GtkBox *) hbox, gtk_label_new(_("Presets:")), TRUE, FALSE, 0);

        button = preset_button(_("Default"), BS2B_DEFAULT_CLEVEL);
        gtk_box_pack_start ((GtkBox *) hbox, button, TRUE, FALSE, 0);

        button = preset_button("C. Moy", BS2B_CMOY_CLEVEL);
        gtk_box_pack_start ((GtkBox *) hbox, button, TRUE, FALSE, 0);

        button = preset_button("J. Meier", BS2B_JMEIER_CLEVEL);
        gtk_box_pack_start ((GtkBox *) hbox, button, TRUE, FALSE, 0);

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
    .name = "Bauer stereophonic-to-binaural",
    .init = init,
    .cleanup = cleanup,
    .configure = configure,
    .start = bs2b_start,
    .process = bs2b_process,
    .flush = bs2b_flush,
    .finish = bs2b_finish,
    .decoder_to_output_time = bs2b_decoder_to_output_time,
    .output_to_decoder_time = bs2b_output_to_decoder_time,
    .preserves_format = TRUE,
)
