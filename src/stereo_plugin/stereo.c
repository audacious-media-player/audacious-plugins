/* Extra Stereo Plugin for Audacious
 * Written by Johan Levin, 1999
 * Ported to new effect API by John Lindgren, 2009 */

#include "config.h"
#include <gtk/gtk.h>

#include <audacious/configdb.h>
#include <audacious/gtk-compat.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

static gboolean init (void);
static void about(void);
static void configure(void);

static void stereo_start (gint * channels, gint * rate);
static void stereo_process (gfloat * * data, gint * samples);
static void stereo_flush ();
static void stereo_finish (gfloat * * data, gint * samples);
static gint stereo_decoder_to_output_time (gint time);
static gint stereo_output_to_decoder_time (gint time);

AUD_EFFECT_PLUGIN
(
    .name = "Extra Stereo",
    .init = init,
    .about = about,
    .configure = configure,
    .start = stereo_start,
    .process = stereo_process,
    .flush = stereo_flush,
    .finish = stereo_finish,
    .decoder_to_output_time = stereo_decoder_to_output_time,
    .output_to_decoder_time = stereo_output_to_decoder_time,
    .preserves_format = TRUE,
)

static const char *about_text = N_("Extra Stereo Plugin\n\n"
                                   "By Johan Levin 1999.");

static GtkWidget *conf_dialog = NULL;
static gdouble value;

static gboolean init (void)
{
	value = 2.5;

	mcs_handle_t * db = aud_cfg_db_open ();
	if (! db)
		return TRUE;

	aud_cfg_db_get_double(db, "extra_stereo", "intensity", &value);
	aud_cfg_db_close(db);
	return TRUE;
}

static void about (void)
{
	static GtkWidget * about_dialog = NULL;

    audgui_simple_message (& about_dialog, GTK_MESSAGE_INFO,
     _("About Extra Stereo Plugin"), _(about_text));
}

static void conf_ok_cb (GtkButton * button, GtkAdjustment * adj)
{
	value = gtk_adjustment_get_value (adj);

	mcs_handle_t * db = aud_cfg_db_open ();
	if (db)
	{
		aud_cfg_db_set_double (db, "extra_stereo", "intensity", value);
		aud_cfg_db_close (db);
	}

	gtk_widget_destroy(conf_dialog);
}

static void conf_cancel_cb(GtkButton * button, gpointer data)
{
	gtk_widget_destroy(conf_dialog);
}

static void conf_apply_cb (GtkButton * button, GtkAdjustment * adj)
{
	value = gtk_adjustment_get_value (adj);
}

static void configure(void)
{
	GtkWidget *hbox, *label, *scale, *button, *bbox;

	if (conf_dialog != NULL)
		return;

	conf_dialog = gtk_dialog_new();
	g_signal_connect (conf_dialog, "destroy", (GCallback)
	 gtk_widget_destroyed, & conf_dialog);
	gtk_window_set_title(GTK_WINDOW(conf_dialog), _("Configure Extra Stereo"));

	label = gtk_label_new(_("Effect intensity:"));
	gtk_box_pack_start ((GtkBox *) gtk_dialog_get_content_area
	 ((GtkDialog *) conf_dialog), label, TRUE, TRUE, 0);
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 10);
	gtk_box_pack_start ((GtkBox *) gtk_dialog_get_content_area
	 ((GtkDialog *) conf_dialog), hbox, TRUE, TRUE, 10);
	gtk_widget_show(hbox);

	GtkAdjustment * adjustment = (GtkAdjustment *) gtk_adjustment_new
	 (value, 0, 15 + 1, 0.1, 1.0, 1.0);
	scale = gtk_hscale_new(GTK_ADJUSTMENT(adjustment));
	gtk_box_pack_start(GTK_BOX(hbox), scale, TRUE, TRUE, 10);
	gtk_widget_show(scale);

	bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_start ((GtkBox *) gtk_dialog_get_action_area ((GtkDialog *)
	 conf_dialog), bbox, TRUE, TRUE, 0);

	button = gtk_button_new_with_label(_("Ok"));
	gtk_widget_set_can_default (button, TRUE);
	gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
	g_signal_connect (button, "clicked", (GCallback) conf_ok_cb, adjustment);
	gtk_widget_grab_default(button);
	gtk_widget_show(button);

	button = gtk_button_new_with_label(_("Cancel"));
	gtk_widget_set_can_default (button, TRUE);
	gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
	g_signal_connect (button, "clicked", (GCallback) conf_cancel_cb, NULL);
	gtk_widget_show(button);

	button = gtk_button_new_with_label(_("Apply"));
	gtk_widget_set_can_default (button, TRUE);
	gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
	g_signal_connect (button, "clicked", (GCallback) conf_apply_cb,
	 adjustment);
	gtk_widget_show(button);

	gtk_widget_show(bbox);

	gtk_widget_show(conf_dialog);
}

static gint stereo_channels;

static void stereo_start (gint * channels, gint * rate)
{
    stereo_channels = * channels;
}

static void stereo_process (gfloat * * data, gint * samples)
{
    gfloat * f, * end;
    gfloat center;

    if (stereo_channels != 2 || samples == 0)
        return;

    end = (* data) + (* samples);

    for (f = * data; f < end; f += 2)
    {
        center = (f[0] + f[1]) / 2;
        f[0] = center + (f[0] - center) * value;
        f[1] = center + (f[1] - center) * value;
    }
}

static void stereo_flush ()
{
}

static void stereo_finish (gfloat * * data, gint * samples)
{
    stereo_process (data, samples);
}

static gint stereo_decoder_to_output_time (gint time)
{
    return time;
}

static gint stereo_output_to_decoder_time (gint time)
{
    return time;
}
