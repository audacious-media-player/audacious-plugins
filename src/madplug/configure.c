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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* #define AUD_DEBUG 1 */

#include "plugin.h"

#include <gtk/gtk.h>
#include <math.h>

static GtkWidget *configure_win = NULL;
static audmad_config_t *oldconfig = NULL; // undo storage

static audmad_config_t *
duplicate_config(audmad_config_t *orig)
{
    audmad_config_t *copy = g_memdup(orig, sizeof(audmad_config_t));

    copy->id3_format = g_strdup(orig->id3_format);

    return copy;
}

static void
dispose_config(audmad_config_t *config)
{
    g_free(config->id3_format);
    g_free(config);
}

static void
update_config(gpointer widgets)
{
    const gchar *text = NULL;

    AUDDBG("updating\n");

    GtkWidget *reopen = g_object_get_data(widgets, "reopen");
    GtkWidget *fast_playback = g_object_get_data(widgets, "fast_playback");
    GtkWidget *use_xing = g_object_get_data(widgets, "use_xing");
    GtkWidget *sjis = g_object_get_data(widgets, "sjis");
    GtkWidget *show_avg = g_object_get_data(widgets, "show_avg");
    GtkWidget *title_override = g_object_get_data(widgets, "title_override");
    GtkWidget *title_id3_entry = g_object_get_data(widgets, "title_id3_entry");

    //audio
    audmad_config->force_reopen_audio =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(reopen));

    //metadata
    audmad_config->fast_play_time_calc =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fast_playback));
    audmad_config->use_xing =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(use_xing));
    audmad_config->sjis =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sjis));

    //misc
    audmad_config->show_avg_vbr_bitrate =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(show_avg));

    //text
    audmad_config->title_override =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(title_override));

    text = gtk_entry_get_text(GTK_ENTRY(title_id3_entry));
    g_free(audmad_config->id3_format);
    audmad_config->id3_format = g_strdup(text);

}

static void
save_config(void)
{
    mcs_handle_t *db = aud_cfg_db_open();

    AUDDBG("saving\n");

    //audio
    aud_cfg_db_set_bool(db, "MAD", "force_reopen_audio",
                            audmad_config->force_reopen_audio);
    //metadata
    aud_cfg_db_set_bool(db, "MAD", "fast_play_time_calc",
                        audmad_config->fast_play_time_calc);
    aud_cfg_db_set_bool(db, "MAD", "use_xing", audmad_config->use_xing);
    aud_cfg_db_set_bool(db, "MAD", "sjis", audmad_config->sjis);

    //misc
    aud_cfg_db_set_bool(db, "MAD", "show_avg_vbr_bitrate",
                            audmad_config->show_avg_vbr_bitrate);

    //text
    aud_cfg_db_set_bool(db, "MAD", "title_override", audmad_config->title_override);
    aud_cfg_db_set_string(db, "MAD", "id3_format", audmad_config->id3_format);

    aud_cfg_db_close(db);
}

static void
configure_win_cancel(GtkWidget *widget, gpointer widgets)
{
    AUDDBG("cancel\n");
    dispose_config(audmad_config);
    audmad_config = oldconfig;
    oldconfig = NULL;
    save_config();
    gtk_widget_destroy(configure_win);
    g_object_unref(widgets);
}

static void
configure_win_ok(GtkWidget *widget, gpointer widgets)
{
    AUDDBG("ok\n");
    update_config(widgets);
    save_config();
    gtk_widget_destroy(configure_win);
    g_object_unref(widgets);
    dispose_config(oldconfig);
    oldconfig = NULL;
}

static void
configure_destroy(GtkWidget *w, gpointer data)
{
}

static void
simple_update_cb(GtkWidget *w, gpointer widgets)
{
    update_config(widgets);
    save_config();
}

static void
entry_changed_cb(GtkWidget *w, gpointer widgets)
{
    simple_update_cb(w, widgets);
}

static void
title_override_cb(GtkWidget *w, gpointer widgets)
{
    GtkWidget *title_override = g_object_get_data(widgets, "title_override");
    GtkWidget *title_id3_entry = g_object_get_data(widgets, "title_id3_entry");
    GtkWidget *title_id3_label = g_object_get_data(widgets, "title_id3_label");
    gboolean override = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(title_override));

    update_config(widgets);
    save_config();

    gtk_widget_set_sensitive(title_id3_entry, override);
    gtk_widget_set_sensitive(title_id3_label, override);
}

void
audmad_configure(void)
{
    GtkWidget *vbox;
    GtkWidget *bbox, *ok, *cancel;
    GtkWidget *notebook, *vbox2, *title_id3_label, *title_id3_box;
    GtkWidget *fast_playback, *use_xing, *sjis, *show_avg, *reopen;
    GtkWidget *title_override, *title_id3_entry;
    GtkWidget *metadataFrame, *audioFrame, *miscFrame;
    GtkWidget *metadata_vbox, *audio_vbox, *misc_vbox;

    gpointer widgets = g_object_new(G_TYPE_OBJECT, NULL);

    if(oldconfig) {
        dispose_config(oldconfig);
        oldconfig = NULL;
    }

    oldconfig = duplicate_config(audmad_config); //for undo

    if (configure_win != NULL) {
        gtk_widget_show(configure_win);
        return;
    }

    configure_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_type_hint(GTK_WINDOW(configure_win), GDK_WINDOW_TYPE_HINT_DIALOG);

    g_signal_connect(G_OBJECT(configure_win), "destroy",
                       G_CALLBACK(gtk_widget_destroyed), &configure_win);
    g_signal_connect(G_OBJECT(configure_win), "destroy",
                       G_CALLBACK(configure_destroy), &configure_win);

    gtk_window_set_title(GTK_WINDOW(configure_win),
                         _("MPEG Audio Plugin Configuration"));
    gtk_window_set_policy(GTK_WINDOW(configure_win), FALSE, FALSE, FALSE);
    gtk_container_border_width(GTK_CONTAINER(configure_win), 10);

    vbox = gtk_vbox_new(FALSE, 10);
    gtk_container_add(GTK_CONTAINER(configure_win), vbox);

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), notebook, FALSE, FALSE, 0);


    /********************************************************************************/


    vbox2 = gtk_vbox_new(FALSE, 5);

    // audio frame
    audioFrame = gtk_frame_new(_("Audio Settings"));
    gtk_container_border_width(GTK_CONTAINER(audioFrame), 5);

    audio_vbox = gtk_vbox_new(FALSE, 5);

    gtk_container_add(GTK_CONTAINER(audioFrame), audio_vbox);
    gtk_container_add(GTK_CONTAINER(vbox2), audioFrame);

    reopen = gtk_check_button_new_with_label(_("Force reopen audio when audio type changed"));
    g_object_set_data(widgets, "reopen", reopen);
    gtk_box_pack_start(GTK_BOX(audio_vbox), reopen, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(reopen),
                                 audmad_config->force_reopen_audio);
    g_signal_connect(G_OBJECT(reopen), "clicked", G_CALLBACK(simple_update_cb), widgets);


    // metadata frame
    metadataFrame = gtk_frame_new(_("Metadata Settings"));
    gtk_container_border_width(GTK_CONTAINER(metadataFrame), 5);

    metadata_vbox = gtk_vbox_new(FALSE, 5);

    gtk_container_add(GTK_CONTAINER(metadataFrame), metadata_vbox);
    gtk_container_add(GTK_CONTAINER(vbox2), metadataFrame);

    fast_playback =
        gtk_check_button_new_with_label(_("Enable fast play-length calculation"));
    g_object_set_data(widgets, "fast_playback", fast_playback);
    gtk_box_pack_start(GTK_BOX(metadata_vbox), fast_playback, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fast_playback),
                                 audmad_config->fast_play_time_calc);
    g_signal_connect(G_OBJECT(fast_playback), "clicked", G_CALLBACK(simple_update_cb), widgets);

    use_xing = gtk_check_button_new_with_label(_("Parse XING headers"));
    g_object_set_data(widgets, "use_xing", use_xing);
    gtk_box_pack_start(GTK_BOX(metadata_vbox), use_xing, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_xing),
                                 audmad_config->use_xing);
    g_signal_connect(G_OBJECT(use_xing), "clicked", G_CALLBACK(simple_update_cb), widgets);

    sjis = gtk_check_button_new_with_label(_("Use SJIS to write ID3 tags (not recommended)"));
    g_object_set_data(widgets, "sjis", sjis);
    gtk_box_pack_start(GTK_BOX(metadata_vbox), sjis, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sjis), audmad_config->sjis);
    g_signal_connect(G_OBJECT(sjis), "clicked", G_CALLBACK(simple_update_cb), widgets);

    // misc frame
    miscFrame = gtk_frame_new(_("Miscellaneous Settings"));
    gtk_container_border_width(GTK_CONTAINER(miscFrame), 5);

    misc_vbox = gtk_vbox_new(FALSE, 5);

    gtk_container_add(GTK_CONTAINER(miscFrame), misc_vbox);
    gtk_container_add(GTK_CONTAINER(vbox2), miscFrame);


    show_avg = gtk_check_button_new_with_label(_("Display average bitrate for VBR"));
    g_object_set_data(widgets, "show_avg", show_avg);
    gtk_box_pack_start(GTK_BOX(misc_vbox), show_avg, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_avg),
                                 audmad_config->show_avg_vbr_bitrate);
    g_signal_connect(G_OBJECT(show_avg), "clicked", G_CALLBACK(simple_update_cb), widgets);

    // add to notebook
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox2, gtk_label_new(_("General")));



    vbox2 = gtk_vbox_new(FALSE, 5);

    title_override = gtk_check_button_new_with_label(_("Override generic titles"));
    g_object_set_data(widgets, "title_override", title_override);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(title_override),
                                 audmad_config->title_override);
    gtk_box_pack_start(GTK_BOX(vbox2), title_override, FALSE,
                       FALSE, 0);
    g_signal_connect(G_OBJECT(title_override), "clicked",
                     G_CALLBACK(title_override_cb), widgets);

    title_id3_box = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox2), title_id3_box, FALSE, FALSE, 0);

    title_id3_label = gtk_label_new(_("ID3 format:"));
    g_object_set_data(widgets, "title_id3_label", title_id3_label);
    gtk_box_pack_start(GTK_BOX(title_id3_box), title_id3_label, FALSE, FALSE, 0);
    gtk_widget_set_sensitive(title_id3_label, audmad_config->title_override);

    title_id3_entry = gtk_entry_new();
    g_object_set_data(widgets, "title_id3_entry", title_id3_entry);
    gtk_entry_set_text(GTK_ENTRY(title_id3_entry), audmad_config->id3_format);
    gtk_box_pack_start(GTK_BOX(title_id3_box), title_id3_entry, TRUE, TRUE, 0);
    g_signal_connect(title_id3_entry, "changed", G_CALLBACK(entry_changed_cb), widgets);
    gtk_widget_set_sensitive(title_id3_entry, audmad_config->title_override);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox2, gtk_label_new(_("Title")));



    /*********************************************************************************/


    bbox = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

    cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);

    g_signal_connect(G_OBJECT(cancel), "clicked",
                     G_CALLBACK(configure_win_cancel), widgets);

    gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);

    ok = gtk_button_new_from_stock(GTK_STOCK_OK);

    g_signal_connect(G_OBJECT(ok), "clicked",
                     G_CALLBACK(configure_win_ok), widgets);

    gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 0);
    gtk_widget_grab_default(ok);

    gtk_widget_show_all(configure_win);
}
