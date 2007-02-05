/*
 * mad plugin for audacious
 * Copyright (C) 2005-2007 William Pitcock, Yoshiki Yazawa
 *
 * Portions derived from xmms-mad:
 * Copyright (C) 2001-2002 Sam Clegg - See COPYING
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "plugin.h"

#include <gtk/gtk.h>
#include <math.h>
#include <audacious/configdb.h>

static GtkWidget *configure_win = NULL;
static GtkWidget *vbox;
static GtkWidget *fast_playback, *use_xing, *dither;
static GtkWidget *RG_enable, *RG_track_mode, *RG_default, *pregain,
    *hard_limit;
static GtkWidget *title_id3_box, *title_tag_desc;
static GtkWidget *title_override, *title_id3_entry;

static void configure_win_ok(GtkWidget * widget, gpointer data)
{
    ConfigDb *db;
    const gchar *text = NULL;
#ifdef DEBUG
    g_message("saving\n");
#endif

    audmad_config.fast_play_time_calc =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fast_playback));
    audmad_config.use_xing =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(use_xing));
    audmad_config.dither =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dither));

    audmad_config.replaygain.enable =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(RG_enable));
    audmad_config.replaygain.track_mode =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(RG_track_mode));
    audmad_config.hard_limit =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hard_limit));
    text = gtk_entry_get_text(GTK_ENTRY(RG_default));
    audmad_config.replaygain.default_db = g_strdup(text);

    text = gtk_entry_get_text(GTK_ENTRY(pregain));
    audmad_config.pregain_db = g_strdup(text);

    audmad_config_compute(&audmad_config);

    db = bmp_cfg_db_open();
    bmp_cfg_db_set_int(db, "MAD", "http_buffer_size",
                       audmad_config.http_buffer_size);
    bmp_cfg_db_set_bool(db, "MAD", "fast_play_time_calc",
                        audmad_config.fast_play_time_calc);
    bmp_cfg_db_set_bool(db, "MAD", "use_xing", audmad_config.use_xing);
    bmp_cfg_db_set_bool(db, "MAD", "dither", audmad_config.dither);
    bmp_cfg_db_set_bool(db, "MAD", "hard_limit",
                        audmad_config.hard_limit);
    bmp_cfg_db_set_string(db, "MAD", "pregain_db",
                          audmad_config.pregain_db);

    bmp_cfg_db_set_bool(db, "MAD", "RG.enable",
                        audmad_config.replaygain.enable);
    bmp_cfg_db_set_bool(db, "MAD", "RG.track_mode",
                        audmad_config.replaygain.track_mode);
    bmp_cfg_db_set_string(db, "MAD", "RG.default_db",
                          audmad_config.replaygain.default_db);

    bmp_cfg_db_set_bool(db, "MAD", "title_override", audmad_config.title_override);
    bmp_cfg_db_set_string(db, "MAD", "id3_format", audmad_config.id3_format);

    bmp_cfg_db_close(db);
    gtk_widget_destroy(configure_win);
}

static void configure_destroy(GtkWidget * w, gpointer data)
{
}

static void
title_override_cb(GtkWidget * w, gpointer data)
{
    gboolean override;
    override =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(title_override));
    gtk_widget_set_sensitive(title_id3_box, override);
    gtk_widget_set_sensitive(title_tag_desc, override);
}

void audmad_configure(void)
{
    GtkWidget *bbox, *ok, *cancel;
    GtkWidget *label, *RG_default_hbox, *pregain_hbox;
    GtkWidget *notebook, *vbox2, *title_id3_label;

    if (configure_win != NULL)
    {
        gtk_widget_show(configure_win);
        return;
    }

    configure_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_type_hint(GTK_WINDOW(configure_win), GDK_WINDOW_TYPE_HINT_DIALOG);

    g_signal_connect(G_OBJECT(configure_win), "destroy",
                       G_CALLBACK(gtk_widget_destroyed),
                       &configure_win);
    g_signal_connect(G_OBJECT(configure_win), "destroy",
                       G_CALLBACK(configure_destroy), &configure_win);

    gtk_window_set_title(GTK_WINDOW(configure_win),
                         _("MPEG Audio Plugin Configuration"));
    gtk_window_set_policy(GTK_WINDOW(configure_win), FALSE, FALSE, FALSE);
    gtk_container_border_width(GTK_CONTAINER(configure_win), 10);

    vbox = gtk_vbox_new(FALSE, 10);
    gtk_container_add(GTK_CONTAINER(configure_win), vbox);

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

    vbox2 = gtk_vbox_new(FALSE, 5);

    fast_playback =
        gtk_check_button_new_with_label(_("Enable fast play-length calculation"));
    gtk_box_pack_start(GTK_BOX(vbox2), fast_playback, TRUE, TRUE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fast_playback),
                                 audmad_config.fast_play_time_calc);

    use_xing = gtk_check_button_new_with_label(_("Parse XING headers"));
    gtk_box_pack_start(GTK_BOX(vbox2), use_xing, TRUE, TRUE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_xing),
                                 audmad_config.use_xing);

    dither =
        gtk_check_button_new_with_label
        (_("Dither output when rounding to 16-bit"));
    gtk_box_pack_start(GTK_BOX(vbox2), dither, TRUE, TRUE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dither),
                                 audmad_config.dither);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox2, gtk_label_new(_("General")));

    vbox2 = gtk_vbox_new(FALSE, 5);

    /* SKR added config : */
    RG_enable = gtk_check_button_new_with_label(_("Enable ReplayGain processing"));
    gtk_box_pack_start(GTK_BOX(vbox2), RG_enable, TRUE, TRUE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(RG_enable),
                                 audmad_config.replaygain.enable);
    RG_track_mode =
        gtk_check_button_new_with_label(_("Track mode"));
    gtk_box_pack_start(GTK_BOX(vbox2), RG_track_mode, TRUE, TRUE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(RG_track_mode),
                                 audmad_config.replaygain.track_mode);

    hard_limit =
        gtk_check_button_new_with_label
        (_("6dB hard limiting"));
    gtk_box_pack_start(GTK_BOX(vbox2), hard_limit, TRUE, TRUE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hard_limit),
                                 audmad_config.hard_limit);

    label = gtk_label_new(_("Default gain (dB):"));
    RG_default_hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox2), RG_default_hbox, TRUE, TRUE, 0);
    RG_default = gtk_entry_new();
    gtk_widget_set_usize(RG_default, 80, -1);
    gtk_entry_set_text(GTK_ENTRY(RG_default),
                       audmad_config.replaygain.default_db);
    gtk_box_pack_start(GTK_BOX(RG_default_hbox), label, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(RG_default_hbox), RG_default, FALSE, TRUE,
                       0);

    label = gtk_label_new(_("Preamp (dB):"));
    pregain_hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox2), pregain_hbox, TRUE, TRUE, 0);
    pregain = gtk_entry_new();
    gtk_widget_set_usize(pregain, 80, -1);
    gtk_entry_set_text(GTK_ENTRY(pregain), audmad_config.pregain_db);
    gtk_box_pack_start(GTK_BOX(pregain_hbox), label, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(pregain_hbox), pregain, FALSE, TRUE, 0);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox2, gtk_label_new("ReplayGain"));

    vbox2 = gtk_vbox_new(FALSE, 5);

    title_override =
        gtk_check_button_new_with_label(_("Override generic titles"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(title_override),
                                 audmad_config.title_override);
    g_signal_connect(G_OBJECT(title_override), "clicked",
                     G_CALLBACK(title_override_cb), NULL);
    gtk_box_pack_start(GTK_BOX(vbox2), title_override, FALSE,
                       FALSE, 0);

    title_id3_box = gtk_hbox_new(FALSE, 5);
    gtk_widget_set_sensitive(title_id3_box, audmad_config.title_override);
    gtk_box_pack_start(GTK_BOX(vbox2), title_id3_box, FALSE,
                       FALSE, 0);

    title_id3_label = gtk_label_new(_("ID3 format:"));
    gtk_box_pack_start(GTK_BOX(title_id3_box), title_id3_label, FALSE,
                       FALSE, 0);

    title_id3_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(title_id3_entry), audmad_config.id3_format);
    gtk_box_pack_start(GTK_BOX(title_id3_box), title_id3_entry, TRUE, TRUE,
                       0);

    title_tag_desc = xmms_titlestring_descriptions("pafFetnygc", 2);
    gtk_widget_set_sensitive(title_tag_desc, audmad_config.title_override);
    gtk_box_pack_start(GTK_BOX(vbox2), title_tag_desc, FALSE,
                       FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox2,
                             gtk_label_new(_("Title")));

    bbox = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

    cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    g_signal_connect_swapped(G_OBJECT(cancel), "clicked",
                              G_CALLBACK(gtk_widget_destroy),
                              G_OBJECT(configure_win));
    gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);

    ok = gtk_button_new_from_stock(GTK_STOCK_OK);
    g_signal_connect(G_OBJECT(ok), "clicked",
                       G_CALLBACK(configure_win_ok), NULL);
    gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 0);
    gtk_widget_grab_default(ok);

    gtk_widget_show_all(configure_win);
}
