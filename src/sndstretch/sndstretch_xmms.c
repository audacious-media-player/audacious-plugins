// sndstretch_xmms.c
//
//    sndstretch_xmms - xmms-output plugin for adjusting
//                      pitch and speed of s16le data
//    Copyright (C) 2001  Florian Berger
//    Email: florian.berger@jk.uni-linz.ac.at
//
//    Ported to new Audacious effect API by John Lindgren, 2009
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License Version 2 as
//    published by the Free Software Foundation;
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//

#include "sndstretch.h"
#include "sndstretch_xmms-logo.xpm"
#include "FB_logo.xpm"

#include "config.h"
#include <gtk/gtk.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <audacious/configdb.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <libaudcore/audio.h>

#define SNDSTRETCH_VERSION_STRING "0.7"

gboolean sndstretch_init (void);
void sndstretch_about          (void);
void sndstretch_config         (void);

static void sndstretch_start (gint * channels, gint * rate);
static void sndstretch_process (gfloat * * data, gint * samples);
static void sndstretch_flush ();
static void sndstretch_finish (gfloat * * data, gint * samples);
static gint sndstretch_decoder_to_output_time (gint time);
static gint sndstretch_output_to_decoder_time (gint time);

EffectPlugin sndstretch_ep = {
	.description = "SndStretch",
	.init = sndstretch_init,
	.about = sndstretch_about,
	.configure = sndstretch_config,
    .start = sndstretch_start,
    .process = sndstretch_process,
    .flush = sndstretch_flush,
    .finish = sndstretch_finish,
    .decoder_to_output_time = sndstretch_decoder_to_output_time,
    .output_to_decoder_time = sndstretch_output_to_decoder_time,
    .preserves_format = TRUE,
};

EffectPlugin *sndstretch_eplist[] = { &sndstretch_ep, NULL };

DECLARE_PLUGIN(sndstretch, NULL, NULL, NULL, NULL, sndstretch_eplist, NULL, NULL, NULL);

struct sndstretch_settings
{
	int handle;    // file handle
	int fragsize;
	int chnr;
	int paused;
	int time_offs;
	int fmtsize;
	int fmt;
	int sampfreq;
	int written;
	int bpsec;
	int vol_l,vol_r;
	int going;
	double pitch;
	double speed;
	double scale;
	int short_overlap;
	int volume_corr;
	GtkObject * pitch_adj;
	GtkObject * speed_adj;
	GtkObject * scale_adj;
};

static struct sndstretch_settings SS;

static const char sndstretch_title_text[] = "SndStretch xmms - " SNDSTRETCH_VERSION_STRING;

static const gchar sndstretch_about_text[] =
	"Copyright (C) 2001 Florian Berger\n<harpin_floh@yahoo.de>\n"
	"Ported to Audacious by Michael Färber\n"
	"http://www.geocities.com/harpin_floh/home.html";

static const gchar sndstretch_GPL_text[] =
	"This program is free software; you can redistribute it and/or modify "
	"it under the terms of the GNU General Public License as published by "
	"the Free Software Foundation; either version 2 of the License, or "
	"(at your option) any later version.\n\n"
	"This program is distributed in the hope that it will be useful, "
	"but WITHOUT ANY WARRANTY; without even the implied warranty of "
	"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
	"GNU General Public License for more details.\n\n"
	"You should have received a copy of the GNU General Public License "
	"along with this program; if not, write to the Free Software "
	"Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, "
	"USA.";

GtkWidget * sndstretch_about_dialog  = NULL;
GtkWidget * sndstretch_config_dialog = NULL;


static gint sndstretch_about_destroy_cb(GtkWidget * w, GdkEventAny * e, gpointer data)
{
	gtk_widget_destroy(sndstretch_about_dialog);
	sndstretch_about_dialog = NULL;
	return TRUE;
}

static void sndstretch_about_ok_cb(GtkButton * button, gpointer data)
{
	gtk_widget_destroy(GTK_WIDGET(sndstretch_about_dialog));
	sndstretch_about_dialog = NULL;
}

void sndstretch_about(void)
{
	GtkWidget * vbox, * scrolltext, * button;
	GtkWidget * titlelabel, * copylabel;
	GtkWidget * text;
	GtkTextBuffer * textbuffer;
	GtkTextIter iter;

	GdkPixmap * logopix;
	GdkBitmap * logomask;
	GtkWidget * logo;

	GdkPixmap * FBlogopix;
	GdkBitmap * FBlogomask;
	GtkWidget * FBlogo;
	GtkWidget * copyhbox, * copy_rbox, * copy_lbox;


	if (sndstretch_about_dialog != NULL)
		return;

	sndstretch_about_dialog = gtk_dialog_new();
	gtk_widget_show(sndstretch_about_dialog);

	/* title logo */
	logopix = gdk_pixmap_create_from_xpm_d(sndstretch_about_dialog->window, &logomask,
										   NULL,
										   (gchar **) sndstretch_xmms_logo_xpm);
	logo = gtk_pixmap_new(logopix,logomask);

	/* FB-logo */
	FBlogopix = gdk_pixmap_create_from_xpm_d(sndstretch_about_dialog->window, &FBlogomask,
											 NULL,
											 (gchar **) FB_logo_xpm);
	FBlogo = gtk_pixmap_new(FBlogopix,FBlogomask);


	gtk_signal_connect(GTK_OBJECT(sndstretch_about_dialog), "destroy",
					   GTK_SIGNAL_FUNC(sndstretch_about_destroy_cb), NULL);
	gtk_window_set_title(GTK_WINDOW(sndstretch_about_dialog), _("About SndStretch"));


	/* labels */
	titlelabel = gtk_label_new(sndstretch_title_text);
	copylabel  = gtk_label_new(sndstretch_about_text);
	gtk_label_set_justify(GTK_LABEL(copylabel), GTK_JUSTIFY_LEFT);

	copy_lbox = gtk_hbox_new(FALSE,0);
	copy_rbox = gtk_hbox_new(FALSE,0);
	gtk_box_pack_end  (GTK_BOX(copy_lbox), FBlogo,    FALSE, TRUE,  0);
	gtk_box_pack_start(GTK_BOX(copy_rbox), copylabel, FALSE, TRUE,  0);
	copyhbox = gtk_hbox_new(FALSE,0);
	gtk_box_pack_start(GTK_BOX(copyhbox), copy_lbox,    TRUE, TRUE,  5);
	gtk_box_pack_start(GTK_BOX(copyhbox), copy_rbox,    TRUE, TRUE,  5);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(sndstretch_about_dialog)->vbox), vbox,
					   TRUE, TRUE, 5);

	scrolltext = gtk_scrolled_window_new(NULL,NULL);
	text = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
	textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	gtk_text_buffer_get_iter_at_offset(textbuffer, &iter, 0);
	gtk_text_buffer_insert(textbuffer, &iter,
						   sndstretch_GPL_text, strlen(sndstretch_GPL_text));


	scrolltext = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolltext),
								   GTK_POLICY_AUTOMATIC,
								   GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolltext), text);

	gtk_box_pack_start(GTK_BOX(vbox), logo, FALSE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), titlelabel, FALSE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), copyhbox, FALSE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), scrolltext, TRUE, TRUE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
	gtk_widget_set_usize(scrolltext, -1, 110);

	button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(sndstretch_about_dialog)->action_area),
					   button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
					   GTK_SIGNAL_FUNC(sndstretch_about_ok_cb), NULL);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default(button);
	gtk_widget_show(button);
	gtk_widget_show_all(sndstretch_about_dialog);
}



static void speed_change_cb(GtkAdjustment * adj, gpointer data)
{
	SS.speed = pow(2.0, GTK_ADJUSTMENT(adj)->value / (GTK_ADJUSTMENT(adj)->upper-10));
}

static void pitch_change_cb(GtkAdjustment * adj, gpointer data)
{
	SS.pitch = pow(2.0, GTK_ADJUSTMENT(adj)->value / (GTK_ADJUSTMENT(adj)->upper-10));
	gtk_adjustment_set_value(GTK_ADJUSTMENT(SS.scale_adj),
							 (GTK_ADJUSTMENT(SS.scale_adj)->upper-10.0)*log(SS.pitch)/log(2.0));
}

static void scale_change_cb(GtkAdjustment * adj, gpointer data)
{
	double speed_eff;

	SS.scale = pow(2.0, GTK_ADJUSTMENT(adj)->value / (GTK_ADJUSTMENT(adj)->upper-10));
	speed_eff= SS.speed/SS.pitch;
	SS.pitch = SS.scale;
	SS.speed = speed_eff*SS.scale;
	if (SS.speed>2.0) SS.speed=2.0;
	if (SS.speed<0.5) SS.speed=0.5;
	gtk_adjustment_set_value(GTK_ADJUSTMENT(SS.speed_adj),
							 (GTK_ADJUSTMENT(SS.speed_adj)->upper-10.0)*log(SS.speed)/log(2.0));
	gtk_adjustment_set_value(GTK_ADJUSTMENT(SS.pitch_adj),
							 (GTK_ADJUSTMENT(SS.pitch_adj)->upper-10.0)*log(SS.pitch)/log(2.0));
}

static void overlap_toggle_cb(GtkToggleButton *butt, gpointer user_data)
{
	SS.short_overlap = gtk_toggle_button_get_active(butt);
}

static void volume_toggle_cb(GtkToggleButton *butt, gpointer user_data)
{
	SS.volume_corr = gtk_toggle_button_get_active(butt);
}

static void sndstretch_config_logobutton_cb(GtkButton * button, gpointer data)
{
	sndstretch_about();
}

static gint sndstretch_config_destroy_cb(GtkWidget * w, GdkEventAny * e, gpointer data)
{
	mcs_handle_t *db = aud_cfg_db_open();

	aud_cfg_db_set_double(db, "sndstretch", "pitch", SS.pitch);
	aud_cfg_db_set_double(db, "sndstretch", "speed", SS.speed);

	aud_cfg_db_set_bool(db, "sndstretch", "short_overlap", SS.short_overlap);
	aud_cfg_db_set_bool(db, "sndstretch", "volume_corr", SS.volume_corr);

	aud_cfg_db_close(db);

	gtk_widget_destroy(sndstretch_config_dialog);
	sndstretch_config_dialog = NULL;
	return TRUE;
}

void sndstretch_config(void)
{
	GtkWidget * vbox;
	GtkWidget * speed_scale, * pitch_scale, * scale_scale;
	GtkWidget * speed_spin,  * pitch_spin,  * scale_spin;
	GtkWidget * speed_hbox,  * pitch_hbox,  * scale_hbox,  * opt_hbox;
	GtkWidget * speed_frame, * pitch_frame, * scale_frame, * opt_frame;
	GdkPixmap * logopix;
	GdkBitmap * logomask;
	GtkWidget * logo;
	GtkWidget * logohbox;
	GtkWidget * logobutton;
	GtkWidget * volume_toggle;
	GtkWidget * overlap_toggle;

	if (sndstretch_config_dialog != NULL)
		return;

	sndstretch_config_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
 	gtk_window_set_type_hint(GTK_WINDOW(sndstretch_config_dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_widget_show(sndstretch_config_dialog);

	logopix = gdk_pixmap_create_from_xpm_d(sndstretch_config_dialog->window, &logomask,
										   NULL, (gchar **)sndstretch_xmms_logo_xpm);

	logo = gtk_pixmap_new(logopix,logomask);

	logobutton = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(logobutton), GTK_RELIEF_NONE);
	gtk_container_add(GTK_CONTAINER(logobutton), logo);
	gtk_signal_connect(GTK_OBJECT(logobutton), "clicked",
					   GTK_SIGNAL_FUNC(sndstretch_config_logobutton_cb), NULL);
	GTK_WIDGET_SET_FLAGS(logobutton, GTK_CAN_DEFAULT);
	gtk_widget_grab_default(logobutton);

	logohbox = gtk_hbox_new(FALSE,0);  // to make it rightbound
	gtk_box_pack_end(GTK_BOX(logohbox), logobutton, FALSE, TRUE, 4);

	SS.speed_adj = gtk_adjustment_new( 100.0*log(SS.speed)/log(2.0),
									   -100, 100+10, 2, 10, 10);
	SS.pitch_adj = gtk_adjustment_new( 120.0*log(SS.pitch)/log(2.0),
									   -120, 120+10, 2, 10, 10);
	SS.scale_adj = gtk_adjustment_new( 100.0*log(SS.scale)/log(2.0),
									   -100, 100+10, 2, 10, 10);

	volume_toggle  = gtk_check_button_new_with_label(_("Volume corr."));
	overlap_toggle = gtk_check_button_new_with_label(_("Short Overlap"));
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(volume_toggle), SS.volume_corr );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(overlap_toggle), SS.short_overlap );


	gtk_signal_connect(GTK_OBJECT(SS.speed_adj), "value-changed",
					   GTK_SIGNAL_FUNC(speed_change_cb), NULL);

	gtk_signal_connect(GTK_OBJECT(SS.pitch_adj), "value-changed",
					   GTK_SIGNAL_FUNC(pitch_change_cb), NULL);

	gtk_signal_connect(GTK_OBJECT(SS.scale_adj), "value-changed",
					   GTK_SIGNAL_FUNC(scale_change_cb), NULL);

	gtk_signal_connect(GTK_OBJECT(volume_toggle), "toggled",
					   GTK_SIGNAL_FUNC(volume_toggle_cb), NULL);

	gtk_signal_connect(GTK_OBJECT(overlap_toggle), "toggled",
					   GTK_SIGNAL_FUNC(overlap_toggle_cb), NULL);

	speed_scale = gtk_hscale_new(GTK_ADJUSTMENT(SS.speed_adj));
	pitch_scale = gtk_hscale_new(GTK_ADJUSTMENT(SS.pitch_adj));
	scale_scale = gtk_hscale_new(GTK_ADJUSTMENT(SS.scale_adj));
	gtk_scale_set_draw_value (GTK_SCALE(speed_scale),FALSE);
	gtk_scale_set_draw_value (GTK_SCALE(pitch_scale),FALSE);
	gtk_scale_set_draw_value (GTK_SCALE(scale_scale),FALSE);

	speed_spin = gtk_spin_button_new(GTK_ADJUSTMENT(SS.speed_adj),1.0,2);
	pitch_spin = gtk_spin_button_new(GTK_ADJUSTMENT(SS.pitch_adj),1.0,2);
	scale_spin = gtk_spin_button_new(GTK_ADJUSTMENT(SS.scale_adj),1.0,2);
	gtk_widget_set_usize (speed_spin,70,20);
	gtk_widget_set_usize (pitch_spin,70,20);
	gtk_widget_set_usize (scale_spin,70,20);
	gtk_entry_set_max_length (GTK_ENTRY(pitch_spin),7);
	gtk_entry_set_max_length (GTK_ENTRY(speed_spin),7);
	gtk_entry_set_max_length (GTK_ENTRY(scale_spin),7);

	speed_hbox = gtk_hbox_new(FALSE,5);
	pitch_hbox = gtk_hbox_new(FALSE,5);
	scale_hbox = gtk_hbox_new(FALSE,5);
	opt_hbox   = gtk_hbox_new(FALSE,5);
	gtk_container_set_border_width(GTK_CONTAINER(speed_hbox), 3);
	gtk_container_set_border_width(GTK_CONTAINER(pitch_hbox), 3);
	gtk_container_set_border_width(GTK_CONTAINER(scale_hbox), 3);
	gtk_container_set_border_width(GTK_CONTAINER(opt_hbox),   3);
	gtk_box_pack_start(GTK_BOX(speed_hbox), speed_spin,  FALSE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(speed_hbox), speed_scale, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(pitch_hbox), pitch_spin,  FALSE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(pitch_hbox), pitch_scale, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(scale_hbox), scale_spin,  FALSE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(scale_hbox), scale_scale, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(opt_hbox), volume_toggle, FALSE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(opt_hbox), overlap_toggle,TRUE, TRUE, 5);

	speed_frame   = gtk_frame_new(_("Speed"));
	pitch_frame   = gtk_frame_new(_("Pitch"));
	scale_frame   = gtk_frame_new(_("Scale"));
	opt_frame     = gtk_frame_new(_("Options"));
	gtk_container_add(GTK_CONTAINER(speed_frame), speed_hbox);
	gtk_container_add(GTK_CONTAINER(pitch_frame), pitch_hbox);
	gtk_container_add(GTK_CONTAINER(scale_frame), scale_hbox);
	gtk_container_add(GTK_CONTAINER(opt_frame),   opt_hbox);
	gtk_container_set_border_width(GTK_CONTAINER(speed_frame), 5);
	gtk_container_set_border_width(GTK_CONTAINER(pitch_frame), 5);
	gtk_container_set_border_width(GTK_CONTAINER(scale_frame), 5);
	gtk_container_set_border_width(GTK_CONTAINER(opt_frame),   5);

	vbox=gtk_vbox_new(FALSE,0);
	gtk_box_pack_start(GTK_BOX(vbox), pitch_frame,   FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), speed_frame,   FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), scale_frame,   FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), opt_frame,     FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), logohbox,      FALSE, TRUE, 0);

	gtk_signal_connect(GTK_OBJECT(sndstretch_config_dialog), "destroy",
					   GTK_SIGNAL_FUNC(sndstretch_config_destroy_cb), NULL);
	gtk_window_set_title(GTK_WINDOW(sndstretch_config_dialog), _("SndStretch - Configuration"));
	gtk_container_add(GTK_CONTAINER(sndstretch_config_dialog), vbox);

	gtk_widget_set_usize(sndstretch_config_dialog, -1, -1);
	gtk_widget_show_all(sndstretch_config_dialog);
}

gboolean sndstretch_init (void)
{
	mcs_handle_t *db;

	db = aud_cfg_db_open();

	SS.fragsize=0;
	SS.chnr=2;
	SS.paused=0;
	SS.time_offs=0;
	SS.fmtsize=2;
	SS.fmt=FMT_S16_NE;
	SS.sampfreq=44100;
	SS.written=0;
	SS.bpsec=176400;
	SS.vol_r=50;
	SS.vol_l=50;
	SS.pitch=1.0;
	SS.speed=1.0;
	SS.scale=1.0;

	gboolean b;
	aud_cfg_db_get_double(db, "sndstretch", "pitch", &SS.pitch);
	aud_cfg_db_get_double(db, "sndstretch", "speed", &SS.speed);
	if (aud_cfg_db_get_bool(db, "sndstretch", "short_overlap", &b))
		SS.short_overlap = b;
	if (aud_cfg_db_get_bool(db, "sndstretch", "volume_corr", &b))
		SS.volume_corr = b;
	aud_cfg_db_close(db);

	return TRUE;
}

static gboolean initted = FALSE;
static PitchSpeedJob job;
static gint current_channels, current_rate;
static struct sndstretch_settings current_settings;

static void sndstretch_start (gint * channels, gint * rate)
{
    if (! initted)
    {
        InitPitchSpeedJob (& job);
        initted = TRUE;
    }

    current_channels = * channels;
    current_rate = * rate;
    memcpy (& current_settings, & SS, sizeof (struct sndstretch_settings));
}

/* FIXME: Find a stretch algorithm that uses floating point. */
/* FIXME: The output buffer should be freed on plugin cleanup. */
static void sndstretch_process (gfloat * * data, gint * samples)
{
    gint new_samples = (* samples) / current_settings.speed + 100;
    gint16 * converted, * stretched;
    static gfloat * reconverted = NULL;

    if (samples == 0)
        return;

    converted = g_malloc (2 * (* samples));
    audio_to_int (* data, converted, FMT_S16_NE, * samples);

    stretched = g_malloc (2 * new_samples);
    snd_pitch_speed_job (converted, current_channels, * samples, FALSE,
     current_settings.pitch, current_settings.speed,
     current_settings.short_overlap ? 882 : 1764, stretched, & new_samples,
     & job, current_settings.volume_corr);
    g_free (converted);

    reconverted = g_realloc (reconverted, sizeof (gfloat) * new_samples);
    audio_from_int (stretched, FMT_S16_NE, reconverted, new_samples);
    g_free (stretched);

    * data = reconverted;
    * samples = new_samples;
}

static void sndstretch_flush ()
{
}

static void sndstretch_finish (gfloat * * data, gint * samples)
{
    sndstretch_process (data, samples);
}

static gint sndstretch_decoder_to_output_time (gint time)
{
    return time / current_settings.speed;
}

static gint sndstretch_output_to_decoder_time (gint time)
{
    return time * current_settings.speed;
}
