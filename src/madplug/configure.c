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

/* #define AUD_DEBUG 1 */

#include "plugin.h"

#include <gtk/gtk.h>
#include <math.h>
#include <audacious/configdb.h>

static GtkWidget *configure_win = NULL;
static audmad_config_t *oldconfig = NULL; // undo storage

static audmad_config_t *
duplicate_config(audmad_config_t *orig)
{
    audmad_config_t *copy = g_memdup(orig, sizeof(audmad_config_t));

    copy->replaygain.preamp0_db = g_strdup(orig->replaygain.preamp0_db);
    copy->replaygain.preamp1_db = g_strdup(orig->replaygain.preamp1_db);
    copy->replaygain.preamp2_db = g_strdup(orig->replaygain.preamp2_db);
    copy->id3_format = g_strdup(orig->id3_format);

    return copy;
}

static void
dispose_config(audmad_config_t *config)
{
    g_free(config->replaygain.preamp0_db);
    g_free(config->replaygain.preamp1_db);
    g_free(config->replaygain.preamp2_db);
    g_free(config->id3_format);
    g_free(config);
}

static void
update_config(gpointer widgets)
{
    const gchar *text = NULL;

    AUDDBG("updating\n");

    GtkWidget *dither = g_object_get_data(widgets, "dither");
    GtkWidget *reopen = g_object_get_data(widgets, "reopen");
    GtkWidget *fast_playback = g_object_get_data(widgets, "fast_playback");
    GtkWidget *use_xing = g_object_get_data(widgets, "use_xing");
    GtkWidget *sjis = g_object_get_data(widgets, "sjis");
    GtkWidget *show_avg = g_object_get_data(widgets, "show_avg");
    GtkWidget *RG_enable = g_object_get_data(widgets, "RG_enable");
    GtkWidget *preamp0 = g_object_get_data(widgets, "preamp0");
    GtkWidget *preamp1 = g_object_get_data(widgets, "preamp1");
    GtkWidget *preamp2 = g_object_get_data(widgets, "preamp2");
    GtkWidget *trackMode = g_object_get_data(widgets, "trackMode");
    GtkWidget *adaptive_scaler = g_object_get_data(widgets, "adaptive_scaler");
    GtkWidget *anti_clip = g_object_get_data(widgets, "anti_clip");
    GtkWidget *title_override = g_object_get_data(widgets, "title_override");
    GtkWidget *title_id3_entry = g_object_get_data(widgets, "title_id3_entry");

    //audio
    audmad_config->dither =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dither));
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

    //gain control
    text = gtk_entry_get_text(GTK_ENTRY(preamp0));
    g_free(audmad_config->replaygain.preamp0_db);
    if(atof(text) > 12.0)
        audmad_config->replaygain.preamp0_db = g_strdup("+12.0");
    else if(atof(text) < -12.0)
        audmad_config->replaygain.preamp0_db = g_strdup("-12.0");
    else
        audmad_config->replaygain.preamp0_db = g_strdup(text);

    gtk_entry_set_text(GTK_ENTRY(preamp0), audmad_config->replaygain.preamp0_db);

    audmad_config->replaygain.enable =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(RG_enable));
    audmad_config->replaygain.track_mode =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(trackMode));

    text = gtk_entry_get_text(GTK_ENTRY(preamp1));
    g_free(audmad_config->replaygain.preamp1_db);
    if(atof(text) > 12.0)
        audmad_config->replaygain.preamp1_db = g_strdup("+12.0");
    else if(atof(text) < -12.0)
        audmad_config->replaygain.preamp1_db = g_strdup("-12.0");
    else
        audmad_config->replaygain.preamp1_db = g_strdup(text);

    gtk_entry_set_text(GTK_ENTRY(preamp1), audmad_config->replaygain.preamp1_db);

    text = gtk_entry_get_text(GTK_ENTRY(preamp2));
    g_free(audmad_config->replaygain.preamp2_db);
    if(atof(text) > 12.0)
        audmad_config->replaygain.preamp2_db = g_strdup("+12.0");
    else if(atof(text) < -12.0)
        audmad_config->replaygain.preamp2_db = g_strdup("-12.0");
    else
        audmad_config->replaygain.preamp2_db = g_strdup(text);

    gtk_entry_set_text(GTK_ENTRY(preamp2), audmad_config->replaygain.preamp2_db);

    audmad_config->replaygain.anti_clip =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(anti_clip));
    audmad_config->replaygain.adaptive_scaler =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(adaptive_scaler));

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
    ConfigDb *db = aud_cfg_db_open();

    AUDDBG("saving\n");

    audmad_config_compute(audmad_config);

    //audio
    aud_cfg_db_set_bool(db, "MAD", "dither", audmad_config->dither);
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

    //gain control
    aud_cfg_db_set_string(db, "MAD", "RG.preamp0_db",
                          audmad_config->replaygain.preamp0_db);
    aud_cfg_db_set_bool(db, "MAD", "RG.enable",
                        audmad_config->replaygain.enable);
    aud_cfg_db_set_bool(db, "MAD", "RG.track_mode",
                        audmad_config->replaygain.track_mode);
    aud_cfg_db_set_string(db, "MAD", "RG.preamp1_db",
                          audmad_config->replaygain.preamp1_db);
    aud_cfg_db_set_string(db, "MAD", "RG.preamp2_db",
                          audmad_config->replaygain.preamp2_db);
    aud_cfg_db_set_bool(db, "MAD", "RG.anti_clip",
                        audmad_config->replaygain.anti_clip);
    aud_cfg_db_set_bool(db, "MAD", "RG.adaptive_scaler",
                        audmad_config->replaygain.adaptive_scaler);

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
RG_enable_cb(GtkWidget *w, gpointer widgets)
{
    GtkWidget *type_vbox = g_object_get_data(widgets, "type_vbox");
    GtkWidget *rgtypeSet = g_object_get_data(widgets, "rgtypeSet");
    GtkWidget *preamp1_hbox = g_object_get_data(widgets, "preamp1_hbox");
    GtkWidget *preamp2_hbox = g_object_get_data(widgets, "preamp2_hbox");
    GtkWidget *anti_clip = g_object_get_data(widgets, "anti_clip");
    gboolean enabled;

    update_config(widgets);
    save_config();

    enabled = audmad_config->replaygain.enable;

    gtk_widget_set_sensitive(type_vbox, enabled);
    gtk_widget_set_sensitive(rgtypeSet, enabled);
    gtk_widget_set_sensitive(preamp1_hbox, enabled);
    gtk_widget_set_sensitive(preamp2_hbox, enabled);
    gtk_widget_set_sensitive(anti_clip, enabled);
}

static void
RG_type_track_cb(GtkWidget *w, gpointer widgets)
{
    GtkToggleButton *tb = GTK_TOGGLE_BUTTON(g_object_get_data(widgets, "trackMode"));

    if (gtk_toggle_button_get_active(tb))
        audmad_config->replaygain.track_mode = TRUE;
}

static void
RG_type_album_cb(GtkWidget *w, gpointer widgets)
{
    GtkToggleButton *tb = GTK_TOGGLE_BUTTON(g_object_get_data(widgets, "albumMode"));

    if (gtk_toggle_button_get_active(tb))
        audmad_config->replaygain.track_mode = FALSE;
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
    GtkWidget *label, *preamp0_hbox, *preamp1_hbox, *preamp2_hbox;
    GtkWidget *notebook, *vbox2, *title_id3_label, *title_id3_box;

    GtkWidget *fast_playback, *use_xing, *dither, *sjis, *show_avg, *reopen;
    GtkWidget *RG_enable, *type_vbox, *rg_vbox, *preamp0, *preamp1, *preamp2;
    GtkWidget *trackMode, *albumMode, *adaptive_scaler, *anti_clip;
    GtkWidget *title_override, *title_id3_entry;
    GtkWidget *rgtypeFrame, *replaygainFrame, *metadataFrame, *audioFrame, *miscFrame;
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

    dither = gtk_check_button_new_with_label
        (_("Dither output when rounding to 16-bit"));
    g_object_set_data(widgets, "dither", dither);
    gtk_box_pack_start(GTK_BOX(audio_vbox), dither, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dither),
                                 audmad_config->dither);
    g_signal_connect(G_OBJECT(dither), "clicked", G_CALLBACK(simple_update_cb), widgets);

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



    /*********************************************************************************/


    vbox2 = gtk_vbox_new(FALSE, 10);
    gtk_container_border_width(GTK_CONTAINER(vbox2), 5);

    // overall preamp
    label = gtk_label_new(_("Base gain (dB):"));
    preamp0_hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox2), preamp0_hbox, TRUE, TRUE, 0);

    preamp0 = gtk_entry_new();
    g_object_set_data(widgets, "preamp0", preamp0);
    gtk_widget_set_usize(preamp0, 80, -1);

    gtk_entry_set_text(GTK_ENTRY(preamp0), audmad_config->replaygain.preamp0_db);
    gtk_box_pack_start(GTK_BOX(preamp0_hbox), label, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(preamp0_hbox), preamp0, FALSE, TRUE, 0);
    g_signal_connect(preamp0, "changed", G_CALLBACK(entry_changed_cb), widgets);

    // replaygain frame
    replaygainFrame = gtk_frame_new(_("ReplayGain Settings"));
    gtk_container_border_width(GTK_CONTAINER(replaygainFrame), 5);
    g_object_set_data(widgets, "replaygainFrame", replaygainFrame);    

    rg_vbox = gtk_vbox_new(FALSE, 5);
    g_object_set_data(widgets, "rg_vbox", rg_vbox);
    gtk_container_add(GTK_CONTAINER(replaygainFrame), rg_vbox);
    gtk_container_add(GTK_CONTAINER(vbox2), replaygainFrame);


    // enable/disable replaygain
    RG_enable = gtk_check_button_new_with_label(_("Enable ReplayGain processing"));
    g_object_set_data(widgets, "RG_enable", RG_enable);
    gtk_box_pack_start(GTK_BOX(rg_vbox), RG_enable, TRUE, TRUE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(RG_enable),
                                 audmad_config->replaygain.enable);

    g_signal_connect(G_OBJECT(RG_enable), "clicked", G_CALLBACK(RG_enable_cb), widgets);


    // replaygin type radio button
    rgtypeFrame = gtk_frame_new(_("ReplayGain Type"));
    g_object_set_data(widgets, "rgtypeFrame", rgtypeFrame);

    type_vbox = gtk_vbox_new(FALSE, 5);
    g_object_set_data(widgets, "type_vbox", type_vbox);

    gtk_container_set_border_width(GTK_CONTAINER(type_vbox), 5);
    gtk_container_add(GTK_CONTAINER(rgtypeFrame), type_vbox);
    gtk_container_add(GTK_CONTAINER(rg_vbox), rgtypeFrame);

    trackMode = gtk_radio_button_new_with_label(NULL, _("Use Track Gain"));
    g_object_set_data(widgets, "trackMode", trackMode);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(trackMode), audmad_config->replaygain.track_mode);
    gtk_box_pack_start(GTK_BOX(type_vbox), trackMode, FALSE, FALSE, 0);


    albumMode =
        gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(trackMode)),
                                        _("Use Album Gain"));
    g_object_set_data(widgets, "albumMode", albumMode);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(albumMode), !audmad_config->replaygain.track_mode);
    gtk_box_pack_start(GTK_BOX(type_vbox), albumMode, FALSE, FALSE, 0);

    // xxx 
    g_signal_connect(G_OBJECT(trackMode), "toggled", G_CALLBACK(RG_type_track_cb), widgets);
    g_signal_connect(G_OBJECT(albumMode), "toggled", G_CALLBACK(RG_type_album_cb), widgets);



    // preamp for the files with RG info
    label = gtk_label_new(_("Pre-gain with RG info (dB):"));
    preamp1_hbox = gtk_hbox_new(FALSE, 5);
    g_object_set_data(widgets, "preamp1_hbox", preamp1_hbox);
    gtk_box_pack_start(GTK_BOX(rg_vbox), preamp1_hbox, TRUE, TRUE, 0);

    preamp1 = gtk_entry_new();
    g_object_set_data(widgets, "preamp1", preamp1);
    gtk_widget_set_usize(preamp1, 80, -1);
    gtk_entry_set_text(GTK_ENTRY(preamp1),
                       audmad_config->replaygain.preamp1_db);
    gtk_box_pack_start(GTK_BOX(preamp1_hbox), label, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(preamp1_hbox), preamp1, FALSE, TRUE, 0);
    g_signal_connect(preamp1, "changed", G_CALLBACK(entry_changed_cb), widgets);


    // preamp for the files without RG info
    label = gtk_label_new(_("Pre-gain without RG info (dB):"));
    preamp2_hbox = gtk_hbox_new(FALSE, 5);
    g_object_set_data(widgets, "preamp2_hbox", preamp2_hbox);
    gtk_box_pack_start(GTK_BOX(rg_vbox), preamp2_hbox, TRUE, TRUE, 0);

    preamp2 = gtk_entry_new();
    g_object_set_data(widgets, "preamp2", preamp2);
    gtk_widget_set_usize(preamp2, 80, -1);
    gtk_entry_set_text(GTK_ENTRY(preamp2),
                       audmad_config->replaygain.preamp2_db);
    gtk_box_pack_start(GTK_BOX(preamp2_hbox), label, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(preamp2_hbox), preamp2, FALSE, TRUE, 0);
    g_signal_connect(preamp2, "changed", G_CALLBACK(entry_changed_cb), widgets);


    // clipping prevention
    anti_clip = gtk_check_button_new_with_label(_("Enable peak info clip prevention"));
    g_object_set_data(widgets, "anti_clip", anti_clip);
    gtk_box_pack_start(GTK_BOX(rg_vbox), anti_clip, TRUE, TRUE, 0);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(anti_clip),
                                 audmad_config->replaygain.anti_clip);

    g_signal_connect(G_OBJECT(anti_clip), "clicked",
                     G_CALLBACK(simple_update_cb), widgets);

    // sensitivity
    if(!audmad_config->replaygain.enable) {
        gtk_widget_set_sensitive(type_vbox, FALSE);
        gtk_widget_set_sensitive(rgtypeFrame, FALSE);
        gtk_widget_set_sensitive(preamp1_hbox, FALSE);
        gtk_widget_set_sensitive(preamp2_hbox, FALSE);
        gtk_widget_set_sensitive(anti_clip, FALSE);
    }
    /* end of replaygainFrame */


    // adaptive scale
    adaptive_scaler = gtk_check_button_new_with_label(_("Enable adaptive scaler clip prevention"));
    g_object_set_data(widgets, "adaptive_scaler", adaptive_scaler);
    gtk_box_pack_start(GTK_BOX(vbox2), adaptive_scaler, TRUE, TRUE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(adaptive_scaler),
                                 audmad_config->replaygain.adaptive_scaler);
    g_signal_connect(G_OBJECT(adaptive_scaler), "clicked",
                     G_CALLBACK(simple_update_cb), widgets);


    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox2, gtk_label_new(_("Gain Control")));


    /*********************************************************************************/


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
