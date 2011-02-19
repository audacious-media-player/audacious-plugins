/*  FileWriter-Plugin
 *  (C) copyright 2007 merging of Disk Writer and Out-Lame by Michael Färber
 *
 *  Original Out-Lame-Plugin:
 *  (C) copyright 2002 Lars Siebold <khandha5@gmx.net>
 *  (C) copyright 2006-2007 porting to audacious by Yoshiki Yazawa <yaz@cc.rim.or.jp>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* #define AUD_DEBUG 1 */

#include "plugins.h"

#ifdef FILEWRITER_MP3

#include <lame/lame.h>

#include <audacious/configdb.h>
#include <audacious/debug.h>
#include <audacious/gtk-compat.h>

#define MODES 4
enum {MODE_AUTO = 4, MODE_JOINT = 1, MODE_STEREO = 0, MODE_MONO = 3};
static const gint modes[MODES] = {MODE_AUTO, MODE_JOINT, MODE_STEREO, MODE_MONO};
static const gchar * const mode_names[MODES] = {N_("Auto"), N_("Joint Stereo"),
 N_("Stereo"), N_("Mono")};

static void mp3_init(write_output_callback write_output_func);
static void mp3_configure(void);
static gint mp3_open(void);
static void mp3_write(void *ptr, gint length);
static void mp3_close(void);
static gint (*write_output)(void *ptr, gint length);

FileWriter mp3_plugin =
{
    .init = mp3_init,
    .configure = mp3_configure,
    .open = mp3_open,
    .write = mp3_write,
    .close = mp3_close,
    .format_required = FMT_S16_NE,
};

static GtkWidget *configure_win = NULL;
static GtkWidget *configure_bbox, *configure_ok, *configure_cancel;
static GtkWidget *alg_quality_spin;
static GtkWidget *alg_quality_hbox;
static GtkAdjustment * alg_quality_adj;
static GtkWidget *vbox, *notebook;
static GtkWidget *quality_vbox, *quality_hbox1, *alg_quality_frame;
static GtkWidget *enc_quality_frame, *enc_quality_label1, *enc_quality_label2;
static GtkWidget * enc_radio1, * enc_radio2;
static GtkWidget *compression_spin;
static GtkAdjustment * compression_adj;
static GtkWidget * mode_hbox, * mode_frame;
static GtkWidget * samplerate_hbox, * samplerate_label, * samplerate_frame;
static GtkWidget *misc_frame, *misc_vbox, *enforce_iso_toggle,
                 *error_protection_toggle;
static GtkWidget *vbr_vbox, *vbr_toggle, *vbr_options_vbox, *vbr_type_frame,
                 *vbr_type_hbox, *vbr_type_radio1, *vbr_type_radio2;
static GtkWidget * abr_frame, * abr_hbox, * abr_label;
static GtkWidget *vbr_frame, *vbr_options_vbox2;
static GtkWidget * vbr_options_hbox1, * vbr_min_label;
static GtkWidget * vbr_options_hbox2, * vbr_max_label, * enforce_min_toggle;
static GtkWidget *vbr_options_hbox3, *vbr_quality_spin, *vbr_quality_label;
static GtkAdjustment * vbr_quality_adj;
static GtkWidget *xing_header_toggle;
static GtkWidget *tags_vbox, *tags_frames_frame, *tags_frames_hbox,
                 *tags_copyright_toggle, *tags_original_toggle;
static GtkWidget *tags_id3_frame, *tags_id3_vbox, *tags_id3_hbox,
                 *tags_force_id3v2_toggle, *tags_only_v1_toggle, *tags_only_v2_toggle;

static GtkWidget *enc_quality_vbox, *hbox1, *hbox2;

static unsigned long numsamples = 0;
static int inside;

static gint vbr_on = 0;
static gint vbr_type = 0;
static gint vbr_min_val = 32;
static gint vbr_max_val = 320;
static gint enforce_min_val = 0;
static gint vbr_quality_val = 4;
static gint abr_val = 128;
static gint toggle_xing_val = 1;
static gint mark_original_val = 1;
static gint mark_copyright_val = 0;
static gint force_v2_val = 0;
static gint only_v1_val = 0;
static gint only_v2_val = 0;
static gint algo_quality_val = 5;
static gint out_samplerate_val = 0;
static gint bitrate_val = 128;
static gfloat compression_val = 11;
static gint enc_toggle_val = 0;
static gint audio_mode_val = MODE_AUTO;
static gint enforce_iso_val = 0;
static gint error_protect_val = 0;

static gint available_samplerates[] =
{ 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 } ;

static gint available_bitrates[] =
{ 8, 16, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 } ;

typedef struct {
    gchar *track_name;
    gchar *album_name;
    gchar *performer;
    gchar *genre;
    gchar *year;
    gchar *track_number;
} lameid3_t;

static lameid3_t lameid3;

static lame_global_flags *gfp;
static unsigned char encbuffer[LAME_MAXMP3BUFFER];
static int id3v2_size;

static guchar * write_buffer;
static gint write_buffer_size;

static void free_lameid3(lameid3_t *p)
{
    g_free(p->track_name);
    g_free(p->album_name);
    g_free(p->performer);
    g_free(p->genre);
    g_free(p->year);
    g_free(p->track_number);

    p->track_name = NULL;
    p->album_name = NULL;
    p->performer = NULL;
    p->genre = NULL;
    p->year = NULL;
    p->track_number = NULL;
}

static void lame_debugf(const char *format, va_list ap)
{
    (void) vfprintf(stdout, format, ap);
}

static void mp3_init(write_output_callback write_output_func)
{
    mcs_handle_t *db = aud_cfg_db_open();
    aud_cfg_db_get_int(db, "filewriter_mp3", "vbr_on", &vbr_on);
    aud_cfg_db_get_int(db, "filewriter_mp3", "vbr_type", &vbr_type);
    aud_cfg_db_get_int(db, "filewriter_mp3", "vbr_min_val", &vbr_min_val);
    aud_cfg_db_get_int(db, "filewriter_mp3", "vbr_max_val", &vbr_max_val);
    aud_cfg_db_get_int(db, "filewriter_mp3", "enforce_min_val",
                       &enforce_min_val);
    aud_cfg_db_get_int(db, "filewriter_mp3", "vbr_quality_val",
                       &vbr_quality_val);
    aud_cfg_db_get_int(db, "filewriter_mp3", "abr_val", &abr_val);
    aud_cfg_db_get_int(db, "filewriter_mp3", "toggle_xing_val",
                       &toggle_xing_val);
    aud_cfg_db_get_int(db, "filewriter_mp3", "mark_original_val",
                       &mark_original_val);
    aud_cfg_db_get_int(db, "filewriter_mp3", "mark_copyright_val",
                       &mark_copyright_val);
    aud_cfg_db_get_int(db, "filewriter_mp3", "force_v2_val", &force_v2_val);
    aud_cfg_db_get_int(db, "filewriter_mp3", "only_v1_val", &only_v1_val);
    aud_cfg_db_get_int(db, "filewriter_mp3", "only_v2_val", &only_v2_val);
    aud_cfg_db_get_int(db, "filewriter_mp3", "algo_quality_val",
                       &algo_quality_val);
    aud_cfg_db_get_int(db, "filewriter_mp3", "out_samplerate_val",
                       &out_samplerate_val);
    aud_cfg_db_get_int(db, "filewriter_mp3", "bitrate_val", &bitrate_val);
    aud_cfg_db_get_float(db, "filewriter_mp3", "compression_val",
                         &compression_val);
    aud_cfg_db_get_int(db, "filewriter_mp3", "enc_toggle_val", &enc_toggle_val);
    aud_cfg_db_get_int(db, "filewriter_mp3", "audio_mode_val", &audio_mode_val);
    aud_cfg_db_get_int(db, "filewriter_mp3", "enforce_iso_val",
                       &enforce_iso_val);
    aud_cfg_db_get_int(db, "filewriter_mp3", "error_protect_val",
                       &error_protect_val);
    aud_cfg_db_close(db);
    if (write_output_func)
        write_output=write_output_func;
}

static gint mp3_open(void)
{
    int imp3;

    gfp = lame_init();
    if (gfp == NULL)
        return 0;

    /* setup id3 data */
    id3tag_init(gfp);

    if (tuple) {
        /* XXX write UTF-8 even though libmp3lame does id3v2.3. --yaz */
        lameid3.track_name =
            g_strdup(tuple_get_string(tuple, FIELD_TITLE, NULL));
        id3tag_set_title(gfp, lameid3.track_name);

        lameid3.performer =
            g_strdup(tuple_get_string(tuple, FIELD_ARTIST, NULL));
        id3tag_set_artist(gfp, lameid3.performer);

        lameid3.album_name =
            g_strdup(tuple_get_string(tuple, FIELD_ALBUM, NULL));
        id3tag_set_album(gfp, lameid3.album_name);

        lameid3.genre =
            g_strdup(tuple_get_string(tuple, FIELD_GENRE, NULL));
        id3tag_set_genre(gfp, lameid3.genre);

        lameid3.year =
            g_strdup_printf("%d", tuple_get_int(tuple, FIELD_YEAR, NULL));
        id3tag_set_year(gfp, lameid3.year);

        lameid3.track_number =
            g_strdup_printf("%d", tuple_get_int(tuple, FIELD_TRACK_NUMBER, NULL));
        id3tag_set_track(gfp, lameid3.track_number);

        if (force_v2_val) {
            id3tag_add_v2(gfp);
        }
        if (only_v1_val) {
            id3tag_v1_only(gfp);
        }
        if (only_v2_val) {
            id3tag_v2_only(gfp);
        }
    }

    /* input stream description */

    lame_set_in_samplerate(gfp, input.frequency);
    lame_set_num_channels(gfp, input.channels);
    /* Maybe implement this? */
    /* lame_set_scale(lame_global_flags *, float); */
    lame_set_out_samplerate(gfp, out_samplerate_val);

    /* general control parameters */

    lame_set_bWriteVbrTag(gfp, toggle_xing_val);
    lame_set_quality(gfp, algo_quality_val);
    if (audio_mode_val != 4) {
        AUDDBG("set mode to %d\n", audio_mode_val);
        lame_set_mode(gfp, audio_mode_val);
    }

    lame_set_errorf(gfp, lame_debugf);
    lame_set_debugf(gfp, lame_debugf);
    lame_set_msgf(gfp, lame_debugf);

    if (enc_toggle_val == 0 && vbr_on == 0)
        lame_set_brate(gfp, bitrate_val);
    else if (vbr_on == 0)
        lame_set_compression_ratio(gfp, compression_val);

    /* frame params */

    lame_set_copyright(gfp, mark_copyright_val);
    lame_set_original(gfp, mark_original_val);
    lame_set_error_protection(gfp, error_protect_val);
    lame_set_strict_ISO(gfp, enforce_iso_val);

    if (vbr_on != 0) {
        if (vbr_type == 0)
            lame_set_VBR(gfp, 2);
        else
            lame_set_VBR(gfp, 3);
        lame_set_VBR_q(gfp, vbr_quality_val);
        lame_set_VBR_mean_bitrate_kbps(gfp, abr_val);
        lame_set_VBR_min_bitrate_kbps(gfp, vbr_min_val);
        lame_set_VBR_max_bitrate_kbps(gfp, vbr_max_val);
        lame_set_VBR_hard_min(gfp, enforce_min_val);
    }

    /* not to write id3 tag automatically. */
    lame_set_write_id3tag_automatic(gfp, 0);

    if (lame_init_params(gfp) == -1)
        return 0;

    /* write id3v2 header */
    imp3 = lame_get_id3v2_tag(gfp, encbuffer, sizeof(encbuffer));

    if (imp3 > 0) {
        write_output(encbuffer, imp3);
        id3v2_size = imp3;
    }
    else {
        id3v2_size = 0;
    }

    write_buffer = NULL;
    write_buffer_size = 0;

    return 1;
}

static void mp3_write(void *ptr, gint length)
{
    gint encoded;

    if (write_buffer_size == 0)
    {
        write_buffer_size = 8192;
        write_buffer = g_realloc (write_buffer, write_buffer_size);
    }

RETRY:
    if (input.channels == 1)
        encoded = lame_encode_buffer (gfp, ptr, ptr, length / 2, write_buffer,
         write_buffer_size);
    else
        encoded = lame_encode_buffer_interleaved (gfp, ptr, length / 4,
         write_buffer, write_buffer_size);

    if (encoded == -1)
    {
        write_buffer_size *= 2;
        write_buffer = g_realloc (write_buffer, write_buffer_size);
        goto RETRY;
    }

    if (encoded > 0)
        write_output (write_buffer, encoded);

    numsamples += length / (2 * input.channels);
}

static void mp3_close(void)
{
    if (output_file) {
        int imp3, encout;

        /* write remaining mp3 data */
        encout = lame_encode_flush_nogap(gfp, encbuffer, LAME_MAXMP3BUFFER);
        write_output(encbuffer, encout);

        /* set gfp->num_samples for valid TLEN tag */
        lame_set_num_samples(gfp, numsamples);

        /* append v1 tag */
        imp3 = lame_get_id3v1_tag(gfp, encbuffer, sizeof(encbuffer));
        if (imp3 > 0)
            write_output(encbuffer, imp3);

        /* update v2 tag */
        imp3 = lame_get_id3v2_tag(gfp, encbuffer, sizeof(encbuffer));
        if (imp3 > 0) {
            if (vfs_fseek(output_file, 0, SEEK_SET) != 0) {
                AUDDBG("can't rewind\n");
            }
            else {
                write_output(encbuffer, imp3);
            }
        }

        /* update lame tag */
        if (id3v2_size) {
            if (vfs_fseek(output_file, id3v2_size, SEEK_SET) != 0) {
                AUDDBG("fatal error: can't update LAME-tag frame!\n");
            }
            else {
                imp3 = lame_get_lametag_frame(gfp, encbuffer, sizeof(encbuffer));
                write_output(encbuffer, imp3);
            }
        }
    }

    g_free (write_buffer);

    lame_close(gfp);
    AUDDBG("lame_close() done\n");

    free_lameid3(&lameid3);
    numsamples = 0;
}

/*****************/
/* Configuration */
/*****************/

/* Various Signal-Fuctions */

static void algo_qual(GtkAdjustment * adjustment, gpointer user_data)
{

    algo_quality_val =
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON
                                         (alg_quality_spin));

}

static void samplerate_changed (GtkComboBox * combo)
{
    gint i = gtk_combo_box_get_active (combo) - 1;
    
    if (i >= 0 && i < G_N_ELEMENTS (available_samplerates))
        out_samplerate_val = available_samplerates[i];
    else
        out_samplerate_val = 0;
}

static void bitrate_changed (GtkComboBox * combo)
{
    gint i = gtk_combo_box_get_active (combo);
    
    if (i >= 0 && i < G_N_ELEMENTS (available_bitrates))
        bitrate_val = available_bitrates[i];
    else
        bitrate_val = 128;
}

static void compression_change(GtkAdjustment * adjustment,
                               gpointer user_data)
{
    compression_val = gtk_spin_button_get_value ((GtkSpinButton *)
     compression_spin);
}

static void encoding_toggle(GtkToggleButton * togglebutton,
                            gpointer user_data)
{

    enc_toggle_val = GPOINTER_TO_INT(user_data);

}

static void mode_changed (GtkComboBox * combo)
{
    gint i = gtk_combo_box_get_active (combo);
    
    if (i >= 0 && i < MODES)
        audio_mode_val = modes[i];
    else
        audio_mode_val = MODE_AUTO;
}

static void toggle_enforce_iso(GtkToggleButton * togglebutton,
                               gpointer user_data)
{

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enforce_iso_toggle))
        == TRUE)
        enforce_iso_val = 1;
    else
        enforce_iso_val = 0;

}

static void toggle_error_protect(GtkToggleButton * togglebutton,
                                 gpointer user_data)
{

    if (gtk_toggle_button_get_active
        (GTK_TOGGLE_BUTTON(error_protection_toggle)) == TRUE)
        error_protect_val = 1;
    else
        error_protect_val = 0;

}

static void toggle_vbr(GtkToggleButton * togglebutton, gpointer user_data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vbr_toggle)) ==
        TRUE) {
        gtk_widget_set_sensitive(vbr_options_vbox, TRUE);
        gtk_widget_set_sensitive(enc_quality_frame, FALSE);
        vbr_on = 1;

        if (vbr_type == 0) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                         (vbr_type_radio1), TRUE);
            gtk_widget_set_sensitive(abr_frame, FALSE);
            gtk_widget_set_sensitive(vbr_frame, TRUE);
        }
        else if (vbr_type == 1) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                         (vbr_type_radio2), TRUE);
            gtk_widget_set_sensitive(abr_frame, TRUE);
            gtk_widget_set_sensitive(vbr_frame, FALSE);
        }

    }
    else {
        gtk_widget_set_sensitive(vbr_options_vbox, FALSE);
        gtk_widget_set_sensitive(enc_quality_frame, TRUE);
        vbr_on = 0;
    }
}

static void vbr_abr_toggle(GtkToggleButton * togglebutton,
                           gpointer user_data)
{
    if (!strcmp(user_data, "VBR")) {
        gtk_widget_set_sensitive(abr_frame, FALSE);
        gtk_widget_set_sensitive(vbr_frame, TRUE);
        vbr_type = 0;
    }
    else if (!strcmp(user_data, "ABR")) {
        gtk_widget_set_sensitive(abr_frame, TRUE);
        gtk_widget_set_sensitive(vbr_frame, FALSE);
        vbr_type = 1;
    }
}

static void vbr_min_changed (GtkComboBox * combo)
{
    gint i = gtk_combo_box_get_active (combo);
    
    if (i >= 0 && i < G_N_ELEMENTS (available_bitrates))
        vbr_min_val = available_bitrates[i];
    else
        vbr_min_val = 32;
}

static void vbr_max_changed (GtkComboBox * combo)
{
    gint i = gtk_combo_box_get_active (combo);
    
    if (i >= 0 && i < G_N_ELEMENTS (available_bitrates))
        vbr_max_val = available_bitrates[i];
    else
        vbr_max_val = 320;
}

static void toggle_enforce_min(GtkToggleButton * togglebutton,
                               gpointer user_data)
{

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enforce_min_toggle))
        == TRUE)
        enforce_min_val = 1;
    else
        enforce_min_val = 0;

}

static void vbr_qual(GtkAdjustment * adjustment, gpointer user_data)
{

    vbr_quality_val =
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON
                                         (vbr_quality_spin));

}

static void abr_changed (GtkComboBox * combo)
{
    gint i = gtk_combo_box_get_active (combo);
    
    if (i >= 0 && i < G_N_ELEMENTS (available_bitrates))
        abr_val = available_bitrates[i];
    else
        abr_val = 128;
}

static void toggle_xing(GtkToggleButton * togglebutton, gpointer user_data)
{

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(xing_header_toggle)) == TRUE)
        toggle_xing_val = 0;
    else
        toggle_xing_val = 1;

}

static void toggle_original(GtkToggleButton * togglebutton,
                            gpointer user_data)
{

    if (gtk_toggle_button_get_active
        (GTK_TOGGLE_BUTTON(tags_original_toggle)) == TRUE)
        mark_original_val = 1;
    else
        mark_original_val = 0;

}

static void toggle_copyright(GtkToggleButton * togglebutton,
                             gpointer user_data)
{

    if (gtk_toggle_button_get_active
        (GTK_TOGGLE_BUTTON(tags_copyright_toggle)) == TRUE)
        mark_copyright_val = 1;
    else
        mark_copyright_val = 0;

}

static void force_v2_toggle(GtkToggleButton * togglebutton,
                            gpointer user_data)
{

    if (gtk_toggle_button_get_active
        (GTK_TOGGLE_BUTTON(tags_force_id3v2_toggle)) == TRUE) {
        force_v2_val = 1;
        if (gtk_toggle_button_get_active
            (GTK_TOGGLE_BUTTON(tags_only_v1_toggle)) == TRUE) {
            inside = 1;
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                         (tags_only_v1_toggle), FALSE);
            only_v1_val = 0;
            inside = 0;
        }
    }
    else
        force_v2_val = 0;

}

static void id3_only_version(GtkToggleButton * togglebutton,
                             gpointer user_data)
{
    if (!strcmp(user_data, "v1") && inside != 1) {
        if (gtk_toggle_button_get_active
            (GTK_TOGGLE_BUTTON(tags_only_v1_toggle)) == TRUE);
        {
            inside = 1;
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                         (tags_only_v2_toggle), FALSE);
            only_v1_val = 1;
            only_v2_val = 0;
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                         (tags_force_id3v2_toggle), FALSE);
            inside = 0;
        }
    }
    else if (!strcmp(user_data, "v2") && inside != 1) {
        if (gtk_toggle_button_get_active
            (GTK_TOGGLE_BUTTON(tags_only_v2_toggle)) == TRUE);
        {
            inside = 1;
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                         (tags_only_v1_toggle), FALSE);
            only_v1_val = 0;
            only_v2_val = 1;
            inside = 0;
        }
    }

}



/* Save Configuration */

static void configure_ok_cb(gpointer data)
{
    mcs_handle_t *db;

    if (vbr_min_val > vbr_max_val)
        vbr_max_val = vbr_min_val;

    db = aud_cfg_db_open();

    aud_cfg_db_set_int(db, "filewriter_mp3", "vbr_on", vbr_on);
    aud_cfg_db_set_int(db, "filewriter_mp3", "vbr_type", vbr_type);
    aud_cfg_db_set_int(db, "filewriter_mp3", "vbr_min_val", vbr_min_val);
    aud_cfg_db_set_int(db, "filewriter_mp3", "vbr_max_val", vbr_max_val);
    aud_cfg_db_set_int(db, "filewriter_mp3", "enforce_min_val", enforce_min_val);
    aud_cfg_db_set_int(db, "filewriter_mp3", "vbr_quality_val", vbr_quality_val);
    aud_cfg_db_set_int(db, "filewriter_mp3", "abr_val", abr_val);
    aud_cfg_db_set_int(db, "filewriter_mp3", "toggle_xing_val", toggle_xing_val);
    aud_cfg_db_set_int(db, "filewriter_mp3", "mark_original_val",
                       mark_original_val);
    aud_cfg_db_set_int(db, "filewriter_mp3", "mark_copyright_val",
                       mark_copyright_val);
    aud_cfg_db_set_int(db, "filewriter_mp3", "force_v2_val", force_v2_val);
    aud_cfg_db_set_int(db, "filewriter_mp3", "only_v1_val", only_v1_val);
    aud_cfg_db_set_int(db, "filewriter_mp3", "only_v2_val", only_v2_val);
    aud_cfg_db_set_int(db, "filewriter_mp3", "algo_quality_val",
                       algo_quality_val);
    aud_cfg_db_set_int(db, "filewriter_mp3", "out_samplerate_val",
                       out_samplerate_val);
    aud_cfg_db_set_int(db, "filewriter_mp3", "bitrate_val", bitrate_val);
    aud_cfg_db_set_float(db, "filewriter_mp3", "compression_val",
                         compression_val);
    aud_cfg_db_set_int(db, "filewriter_mp3", "enc_toggle_val", enc_toggle_val);
    aud_cfg_db_set_int(db, "filewriter_mp3", "audio_mode_val", audio_mode_val);
    aud_cfg_db_set_int(db, "filewriter_mp3", "enforce_iso_val", enforce_iso_val);
    aud_cfg_db_set_int(db, "filewriter_mp3", "error_protect_val",
                       error_protect_val);
    aud_cfg_db_close(db);


    gtk_widget_destroy(configure_win);
}


/************************/
/* Configuration Widget */
/************************/


static void mp3_configure(void)
{
    gint i;

    if (!configure_win) {
        configure_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_type_hint(GTK_WINDOW(configure_win), GDK_WINDOW_TYPE_HINT_DIALOG);

        g_signal_connect (configure_win, "destroy", (GCallback)
         gtk_widget_destroyed, & configure_win);
        gtk_window_set_title(GTK_WINDOW(configure_win),
                             _("MP3 Configuration"));
        gtk_window_set_position(GTK_WINDOW(configure_win),
                                GTK_WIN_POS_MOUSE);
        gtk_container_set_border_width(GTK_CONTAINER(configure_win), 5);

        vbox = gtk_vbox_new(FALSE, 5);
        gtk_container_add(GTK_CONTAINER(configure_win), vbox);

        notebook = gtk_notebook_new();
        gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);


        /* Quality */

        quality_vbox = gtk_vbox_new(FALSE, 5);
        gtk_container_set_border_width(GTK_CONTAINER(quality_vbox), 5);

        quality_hbox1 = gtk_hbox_new(FALSE, 5);
        gtk_box_pack_start(GTK_BOX(quality_vbox), quality_hbox1, FALSE,
                           FALSE, 0);

        /* Algorithm Quality */

        alg_quality_frame = gtk_frame_new(_("Algorithm Quality:"));
        gtk_container_set_border_width(GTK_CONTAINER(alg_quality_frame),
                                       5);
        gtk_box_pack_start(GTK_BOX(quality_hbox1), alg_quality_frame,
                           FALSE, FALSE, 0);

        alg_quality_hbox = gtk_hbox_new(FALSE, 5);
        gtk_container_set_border_width(GTK_CONTAINER(alg_quality_hbox),
                                       10);
        gtk_container_add(GTK_CONTAINER(alg_quality_frame),
                          alg_quality_hbox);

        alg_quality_adj = (GtkAdjustment *) gtk_adjustment_new (5, 0, 9, 1, 1, 0);
        alg_quality_spin =
            gtk_spin_button_new(GTK_ADJUSTMENT(alg_quality_adj), 8, 0);
        gtk_box_pack_start(GTK_BOX(alg_quality_hbox), alg_quality_spin,
                           TRUE, TRUE, 0);
        g_signal_connect (alg_quality_adj, "value-changed", (GCallback)
         algo_qual, NULL);

        gtk_spin_button_set_value(GTK_SPIN_BUTTON(alg_quality_spin),
                                  algo_quality_val);

        /* Output Samplerate */

        samplerate_frame = gtk_frame_new(_("Output Samplerate:"));
        gtk_container_set_border_width(GTK_CONTAINER(samplerate_frame), 5);
        gtk_box_pack_start(GTK_BOX(quality_hbox1), samplerate_frame, FALSE,
                           FALSE, 0);

        samplerate_hbox = gtk_hbox_new(TRUE, 5);
        gtk_container_set_border_width(GTK_CONTAINER(samplerate_hbox), 10);
        gtk_container_add(GTK_CONTAINER(samplerate_frame),
                          samplerate_hbox);

        GtkWidget * combo = gtk_combo_box_text_new ();
        gtk_combo_box_text_append_text ((GtkComboBoxText *) combo, _("Auto"));
        if (! out_samplerate_val)
            gtk_combo_box_set_active ((GtkComboBox *) combo, 0);

        for (i = 0; i < G_N_ELEMENTS (available_samplerates); i ++)
        {
            gchar buf[10];
            snprintf (buf, sizeof buf, "%d", available_samplerates[i]);
            gtk_combo_box_text_append_text ((GtkComboBoxText *) combo, buf);
            
            if (out_samplerate_val == available_samplerates[i])
                gtk_combo_box_set_active ((GtkComboBox *) combo, 1 + i);
        }

        gtk_box_pack_start ((GtkBox *) samplerate_hbox, combo, FALSE, FALSE, 0);
        g_signal_connect (combo, "changed", (GCallback) samplerate_changed, NULL);

        samplerate_label = gtk_label_new(_("(Hz)"));
        gtk_misc_set_alignment(GTK_MISC(samplerate_label), 0, 0.5);
        gtk_box_pack_start(GTK_BOX(samplerate_hbox), samplerate_label,
                           FALSE, FALSE, 0);

        /* Encoder Quality */

        enc_quality_frame = gtk_frame_new(_("Bitrate / Compression ratio:"));
        gtk_container_set_border_width(GTK_CONTAINER(enc_quality_frame),
                                       5);
        gtk_box_pack_start(GTK_BOX(quality_vbox), enc_quality_frame, FALSE,
                           FALSE, 0);

        // vbox sorrounding hbox1 and hbox2
        enc_quality_vbox = gtk_vbox_new(FALSE, 5);
        gtk_container_set_border_width(GTK_CONTAINER(enc_quality_vbox), 10);

        // pack vbox to frame
        gtk_container_add(GTK_CONTAINER(enc_quality_frame), enc_quality_vbox);

        // hbox1 for bitrate
        hbox1 = gtk_hbox_new(FALSE, 5);
        gtk_container_add(GTK_CONTAINER(enc_quality_vbox), hbox1);

        // radio 1
        enc_radio1 = gtk_radio_button_new(NULL);
        if (enc_toggle_val == 0)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enc_radio1), TRUE);
        gtk_box_pack_start(GTK_BOX(hbox1), enc_radio1, FALSE, FALSE, 0);

        // label 1
        enc_quality_label1 = gtk_label_new(_("Bitrate (kbps):"));
        gtk_box_pack_start(GTK_BOX(hbox1), enc_quality_label1, FALSE, FALSE, 0);

        // bitrate menu

        combo = gtk_combo_box_text_new ();

        for (i = 0; i < G_N_ELEMENTS (available_bitrates); i ++)
        {
            gchar buf[10];
            snprintf (buf, sizeof buf, "%d", available_bitrates[i]);
            gtk_combo_box_text_append_text ((GtkComboBoxText *) combo, buf);
            
            if (bitrate_val == available_bitrates[i])
                gtk_combo_box_set_active ((GtkComboBox *) combo, i);
        }

        gtk_box_pack_start ((GtkBox *) hbox1, combo, FALSE, FALSE, 0);
        g_signal_connect (combo, "changed", (GCallback) bitrate_changed, NULL);

        // hbox2 for compression ratio
        hbox2 = gtk_hbox_new(FALSE, 5);
        gtk_container_add(GTK_CONTAINER(enc_quality_vbox), hbox2);

        // radio 2
        enc_radio2 = gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(enc_radio1));
        if (enc_toggle_val == 1)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enc_radio2),
                                         TRUE);
        // pack radio 2
        gtk_box_pack_start(GTK_BOX(hbox2), enc_radio2, FALSE, FALSE, 0);

        // label
        enc_quality_label2 = gtk_label_new(_("Compression ratio:"));
        gtk_box_pack_start(GTK_BOX(hbox2), enc_quality_label2, FALSE, FALSE, 0);

        // comp-ratio spin
        compression_adj = (GtkAdjustment *) gtk_adjustment_new (11, 0, 100, 1,
         1, 0);
        compression_spin =
            gtk_spin_button_new(GTK_ADJUSTMENT(compression_adj), 8, 0);
        gtk_box_pack_end(GTK_BOX(hbox2), compression_spin, FALSE, FALSE, 0);

        g_signal_connect (compression_adj, "value-changed", (GCallback)
         compression_change, NULL);

        gtk_spin_button_set_value(GTK_SPIN_BUTTON(compression_spin),
                                  compression_val);

        // radio button signale connect
        g_signal_connect (enc_radio1, "toggled", (GCallback) encoding_toggle,
         GINT_TO_POINTER (0));
        g_signal_connect (enc_radio2, "toggled", (GCallback) encoding_toggle,
         GINT_TO_POINTER (1));

        /* Audio Mode */

        mode_frame = gtk_frame_new(_("Audio Mode:"));
        gtk_container_set_border_width(GTK_CONTAINER(mode_frame), 5);
        gtk_box_pack_start(GTK_BOX(quality_vbox), mode_frame, FALSE, FALSE,
                           0);

        mode_hbox = gtk_hbox_new(TRUE, 10);
        gtk_container_set_border_width(GTK_CONTAINER(mode_hbox), 10);
        gtk_container_add(GTK_CONTAINER(mode_frame), mode_hbox);

        combo = gtk_combo_box_text_new ();

        for (i = 0; i < MODES; i ++)
        {
            gtk_combo_box_text_append_text ((GtkComboBoxText *) combo,
             _(mode_names[i]));
            
            if (audio_mode_val == modes[i])
                gtk_combo_box_set_active ((GtkComboBox *) combo, i);
        }

        gtk_box_pack_start ((GtkBox *) mode_hbox, combo, FALSE, FALSE, 0);
        g_signal_connect (combo, "changed", (GCallback) mode_changed, NULL);

        /* Misc */

        misc_frame = gtk_frame_new(_("Misc:"));
        gtk_container_set_border_width(GTK_CONTAINER(misc_frame), 5);
        gtk_box_pack_start(GTK_BOX(quality_vbox), misc_frame, FALSE, FALSE,
                           0);

        misc_vbox = gtk_vbox_new(TRUE, 5);
        gtk_container_set_border_width(GTK_CONTAINER(misc_vbox), 5);
        gtk_container_add(GTK_CONTAINER(misc_frame), misc_vbox);

        enforce_iso_toggle =
            gtk_check_button_new_with_label
            (_("Enforce strict ISO complience"));
        gtk_box_pack_start(GTK_BOX(misc_vbox), enforce_iso_toggle, TRUE,
                           TRUE, 2);
        g_signal_connect (enforce_iso_toggle, "toggled", (GCallback)
         toggle_enforce_iso, NULL);

        if (enforce_iso_val == 1)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                         (enforce_iso_toggle), TRUE);

        error_protection_toggle =
            gtk_check_button_new_with_label(_("Error protection"));
        gtk_box_pack_start(GTK_BOX(misc_vbox), error_protection_toggle,
                           TRUE, TRUE, 2);
        g_signal_connect (error_protection_toggle, "toggled", (GCallback)
         toggle_error_protect, NULL);

        if (error_protect_val == 1)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                         (error_protection_toggle), TRUE);

        /* Add the Notebook */
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), quality_vbox,
                                 gtk_label_new(_("Quality")));


        /* VBR/ABR */

        vbr_vbox = gtk_vbox_new(FALSE, 5);
        gtk_container_set_border_width(GTK_CONTAINER(vbr_vbox), 5);

        /* Toggle VBR */

        vbr_toggle = gtk_check_button_new_with_label(_("Enable VBR/ABR"));
        gtk_box_pack_start(GTK_BOX(vbr_vbox), vbr_toggle, FALSE, FALSE, 2);
        g_signal_connect (vbr_toggle, "toggled", (GCallback) toggle_vbr, NULL);

        vbr_options_vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(vbr_vbox), vbr_options_vbox);
        gtk_widget_set_sensitive(vbr_options_vbox, FALSE);

        /* Choose VBR/ABR */

        vbr_type_frame = gtk_frame_new(_("Type:"));
        gtk_container_set_border_width(GTK_CONTAINER(vbr_type_frame), 5);
        gtk_box_pack_start(GTK_BOX(vbr_options_vbox), vbr_type_frame,
                           FALSE, FALSE, 2);

        vbr_type_hbox = gtk_hbox_new(FALSE, 5);
        gtk_container_set_border_width(GTK_CONTAINER(vbr_type_hbox), 5);
        gtk_container_add(GTK_CONTAINER(vbr_type_frame), vbr_type_hbox);

        vbr_type_radio1 = gtk_radio_button_new_with_label(NULL, "VBR");
        gtk_box_pack_start(GTK_BOX(vbr_type_hbox), vbr_type_radio1, TRUE,
                           TRUE, 2);
        if (vbr_type == 0)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                         (vbr_type_radio1), TRUE);

        vbr_type_radio2 =
            gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON
                                                        (vbr_type_radio1),
                                                        "ABR");
        gtk_box_pack_start(GTK_BOX(vbr_type_hbox), vbr_type_radio2, TRUE,
                           TRUE, 2);
        if (vbr_type == 1)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                         (vbr_type_radio2), TRUE);

        g_signal_connect (vbr_type_radio1, "toggled", (GCallback)
         vbr_abr_toggle, "VBR");
        g_signal_connect (vbr_type_radio2, "toggled", (GCallback)
         vbr_abr_toggle, "ABR");

        /* VBR Options */

        vbr_frame = gtk_frame_new(_("VBR Options:"));
        gtk_container_set_border_width(GTK_CONTAINER(vbr_frame), 5);
        gtk_box_pack_start(GTK_BOX(vbr_options_vbox), vbr_frame, FALSE,
                           FALSE, 2);

        vbr_options_vbox2 = gtk_vbox_new(FALSE, 5);
        gtk_container_set_border_width(GTK_CONTAINER(vbr_options_vbox2),
                                       5);
        gtk_container_add(GTK_CONTAINER(vbr_frame), vbr_options_vbox2);

        vbr_options_hbox1 = gtk_hbox_new(FALSE, 5);
        gtk_container_set_border_width(GTK_CONTAINER(vbr_options_hbox1),
                                       5);
        gtk_container_add(GTK_CONTAINER(vbr_options_vbox2),
                          vbr_options_hbox1);

        vbr_min_label = gtk_label_new(_("Minimum bitrate (kbps):"));
        gtk_misc_set_alignment(GTK_MISC(vbr_min_label), 0, 0.5);
        gtk_box_pack_start(GTK_BOX(vbr_options_hbox1), vbr_min_label, TRUE,
                           TRUE, 0);

        combo = gtk_combo_box_text_new ();

        for (i = 0; i < G_N_ELEMENTS (available_bitrates); i ++)
        {
            gchar buf[10];
            snprintf (buf, sizeof buf, "%d", available_bitrates[i]);
            gtk_combo_box_text_append_text ((GtkComboBoxText *) combo, buf);
            
            if (vbr_min_val == available_bitrates[i])
                gtk_combo_box_set_active ((GtkComboBox *) combo, i);
        }

        gtk_box_pack_start ((GtkBox *) vbr_options_hbox1, combo, FALSE, FALSE,
         0);
        g_signal_connect (combo, "changed", (GCallback) vbr_min_changed, NULL);

        vbr_options_hbox2 = gtk_hbox_new(FALSE, 5);
        gtk_container_set_border_width(GTK_CONTAINER(vbr_options_hbox2),
                                       5);
        gtk_container_add(GTK_CONTAINER(vbr_options_vbox2),
                          vbr_options_hbox2);

        vbr_max_label = gtk_label_new(_("Maximum bitrate (kbps):"));
        gtk_misc_set_alignment(GTK_MISC(vbr_max_label), 0, 0.5);
        gtk_box_pack_start(GTK_BOX(vbr_options_hbox2), vbr_max_label, TRUE,
                           TRUE, 0);

        combo = gtk_combo_box_text_new ();

        for (i = 0; i < G_N_ELEMENTS (available_bitrates); i ++)
        {
            gchar buf[10];
            snprintf (buf, sizeof buf, "%d", available_bitrates[i]);
            gtk_combo_box_text_append_text ((GtkComboBoxText *) combo, buf);
            
            if (vbr_max_val == available_bitrates[i])
                gtk_combo_box_set_active ((GtkComboBox *) combo, i);
        }

        gtk_box_pack_start ((GtkBox *) vbr_options_hbox2, combo, FALSE, FALSE,
         0);
        g_signal_connect (combo, "changed", (GCallback) vbr_max_changed, NULL);

        enforce_min_toggle =
            gtk_check_button_new_with_label
            (_("Strictly enforce minimum bitrate"));
        gtk_box_pack_start(GTK_BOX(vbr_options_vbox2), enforce_min_toggle,
                           FALSE, FALSE, 2);
        g_signal_connect (enforce_min_toggle, "toggled", (GCallback)
         toggle_enforce_min, NULL);

        if (enforce_min_val == 1)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                         (enforce_min_toggle), TRUE);

        /* ABR Options */

        abr_frame = gtk_frame_new(_("ABR Options:"));
        gtk_container_set_border_width(GTK_CONTAINER(abr_frame), 5);
        gtk_box_pack_start(GTK_BOX(vbr_options_vbox), abr_frame, FALSE,
                           FALSE, 2);
        gtk_widget_set_sensitive(abr_frame, FALSE);

        abr_hbox = gtk_hbox_new(FALSE, 5);
        gtk_container_set_border_width(GTK_CONTAINER(abr_hbox), 5);
        gtk_container_add(GTK_CONTAINER(abr_frame), abr_hbox);

        abr_label = gtk_label_new(_("Average bitrate (kbps):"));
        gtk_misc_set_alignment(GTK_MISC(abr_label), 0, 0.5);
        gtk_box_pack_start(GTK_BOX(abr_hbox), abr_label, TRUE, TRUE, 0);

        combo = gtk_combo_box_text_new ();

        for (i = 0; i < G_N_ELEMENTS (available_bitrates); i ++)
        {
            gchar buf[10];
            snprintf (buf, sizeof buf, "%d", available_bitrates[i]);
            gtk_combo_box_text_append_text ((GtkComboBoxText *) combo, buf);
            
            if (abr_val == available_bitrates[i])
                gtk_combo_box_set_active ((GtkComboBox *) combo, i);
        }

        gtk_box_pack_start ((GtkBox *) abr_hbox, combo, FALSE, FALSE,
         0);
        g_signal_connect (combo, "changed", (GCallback) abr_changed, NULL);

        /* Quality Level */

        vbr_options_hbox3 = gtk_hbox_new(FALSE, 5);
        gtk_container_set_border_width(GTK_CONTAINER(vbr_options_hbox3),
                                       5);
        gtk_container_add(GTK_CONTAINER(vbr_options_vbox),
                          vbr_options_hbox3);

        vbr_quality_label = gtk_label_new(_("VBR quality level:"));
        gtk_misc_set_alignment(GTK_MISC(vbr_quality_label), 0, 0.5);
        gtk_box_pack_start(GTK_BOX(vbr_options_hbox3), vbr_quality_label,
                           TRUE, TRUE, 0);

        vbr_quality_adj = (GtkAdjustment *) gtk_adjustment_new (4, 0, 9, 1, 1, 0);
        vbr_quality_spin =
            gtk_spin_button_new(GTK_ADJUSTMENT(vbr_quality_adj), 8, 0);
        gtk_box_pack_start(GTK_BOX(vbr_options_hbox3), vbr_quality_spin,
                           TRUE, TRUE, 0);
        g_signal_connect (vbr_quality_adj, "value-changed", (GCallback)
         vbr_qual, NULL);

        gtk_spin_button_set_value(GTK_SPIN_BUTTON(vbr_quality_spin),
                                  vbr_quality_val);

        /* Xing Header */

        xing_header_toggle =
            gtk_check_button_new_with_label(_("Don't write Xing VBR header"));
        gtk_box_pack_start(GTK_BOX(vbr_options_vbox), xing_header_toggle,
                           FALSE, FALSE, 2);
        g_signal_connect (xing_header_toggle, "toggled", (GCallback)
         toggle_xing, NULL);

        if (toggle_xing_val == 0)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                         (xing_header_toggle), TRUE);


        /* Add the Notebook */

        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbr_vbox,
                                 gtk_label_new(_("VBR/ABR")));


        /* Tags */

        tags_vbox = gtk_vbox_new(FALSE, 5);
        gtk_container_set_border_width(GTK_CONTAINER(tags_vbox), 5);

        /* Frame Params */

        tags_frames_frame = gtk_frame_new(_("Frame params:"));
        gtk_container_set_border_width(GTK_CONTAINER(tags_frames_frame),
                                       5);
        gtk_box_pack_start(GTK_BOX(tags_vbox), tags_frames_frame, FALSE,
                           FALSE, 2);

        tags_frames_hbox = gtk_hbox_new(FALSE, 5);
        gtk_container_set_border_width(GTK_CONTAINER(tags_frames_hbox), 5);
        gtk_container_add(GTK_CONTAINER(tags_frames_frame),
                          tags_frames_hbox);

        tags_copyright_toggle =
            gtk_check_button_new_with_label(_("Mark as copyright"));
        gtk_box_pack_start(GTK_BOX(tags_frames_hbox),
                           tags_copyright_toggle, FALSE, FALSE, 2);
        g_signal_connect (tags_copyright_toggle, "toggled", (GCallback)
         toggle_copyright, NULL);

        if (mark_copyright_val == 1)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                         (tags_copyright_toggle), TRUE);

        tags_original_toggle =
            gtk_check_button_new_with_label(_("Mark as original"));
        gtk_box_pack_start(GTK_BOX(tags_frames_hbox), tags_original_toggle,
                           FALSE, FALSE, 2);
        g_signal_connect (tags_original_toggle, "toggled", (GCallback)
         toggle_original, NULL);

        if (mark_original_val == 1)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                         (tags_original_toggle), TRUE);

        /* ID3 Params */

        tags_id3_frame = gtk_frame_new(_("ID3 params:"));
        gtk_container_set_border_width(GTK_CONTAINER(tags_id3_frame), 5);
        gtk_box_pack_start(GTK_BOX(tags_vbox), tags_id3_frame, FALSE,
                           FALSE, 2);

        tags_id3_vbox = gtk_vbox_new(FALSE, 5);
        gtk_container_set_border_width(GTK_CONTAINER(tags_id3_vbox), 5);
        gtk_container_add(GTK_CONTAINER(tags_id3_frame), tags_id3_vbox);

        tags_force_id3v2_toggle =
            gtk_check_button_new_with_label
            (_("Force addition of version 2 tag"));
        gtk_box_pack_start(GTK_BOX(tags_id3_vbox), tags_force_id3v2_toggle,
                           FALSE, FALSE, 2);
        g_signal_connect (tags_force_id3v2_toggle, "toggled", (GCallback)
         force_v2_toggle, NULL);

        tags_id3_hbox = gtk_hbox_new(FALSE, 5);
        gtk_container_add(GTK_CONTAINER(tags_id3_vbox), tags_id3_hbox);

        tags_only_v1_toggle =
            gtk_check_button_new_with_label(_("Only add v1 tag"));
        gtk_box_pack_start(GTK_BOX(tags_id3_hbox), tags_only_v1_toggle,
                           FALSE, FALSE, 2);
        g_signal_connect (tags_only_v1_toggle, "toggled", (GCallback)
         id3_only_version, "v1");

        tags_only_v2_toggle =
            gtk_check_button_new_with_label(_("Only add v2 tag"));
        gtk_box_pack_start(GTK_BOX(tags_id3_hbox), tags_only_v2_toggle,
                           FALSE, FALSE, 2);
        g_signal_connect (tags_only_v2_toggle, "toggled", (GCallback)
         id3_only_version, "v2");

        if (force_v2_val == 1)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                         (tags_force_id3v2_toggle), TRUE);

        if (only_v1_val == 1)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                         (tags_only_v1_toggle), TRUE);

        if (only_v2_val == 1)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                         (tags_only_v2_toggle), TRUE);

        /* Add the Notebook */

        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tags_vbox,
                                 gtk_label_new(_("Tags")));




        /* The Rest */

        /* Buttons */

        configure_bbox = gtk_hbutton_box_new();
        gtk_button_box_set_layout(GTK_BUTTON_BOX(configure_bbox),
                                  GTK_BUTTONBOX_END);
        gtk_box_pack_start(GTK_BOX(vbox), configure_bbox, FALSE, FALSE, 0);

        configure_cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
        g_signal_connect_swapped (configure_cancel, "clicked", (GCallback)
         gtk_widget_destroy, configure_win);
        gtk_box_pack_start(GTK_BOX(configure_bbox), configure_cancel, TRUE,
                           TRUE, 0);

        configure_ok = gtk_button_new_from_stock(GTK_STOCK_OK);
        g_signal_connect (configure_ok, "clicked", (GCallback) configure_ok_cb,
         NULL);
        gtk_box_pack_start(GTK_BOX(configure_bbox), configure_ok, TRUE,
                           TRUE, 0);
        gtk_widget_show(configure_ok);

        /* Set States */

        if (vbr_on == 1)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(vbr_toggle),
                                         TRUE);
        else
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(vbr_toggle),
                                         FALSE);

        /* Show it! */

        gtk_widget_show_all(configure_win);

    }
}

#endif
