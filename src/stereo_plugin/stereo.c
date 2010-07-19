/* Extra Stereo Plugin for Audacious
 * Written by Johan Levin, 1999
 * Ported to new effect API by John Lindgren, 2009 */

#include "config.h"
#include <gtk/gtk.h>

#include <audacious/configdb.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

static void init(void);
static void about(void);
static void configure(void);

static void stereo_start (gint * channels, gint * rate);
static void stereo_process (gfloat * * data, gint * samples);
static void stereo_flush ();
static void stereo_finish (gfloat * * data, gint * samples);
static gint stereo_decoder_to_output_time (gint time);
static gint stereo_output_to_decoder_time (gint time);

EffectPlugin stereo_ep =
{
	.description = "Extra Stereo Plugin", /* Description */
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
};

static const char *about_text = N_("Extra Stereo Plugin\n\n"
                                   "By Johan Levin 1999.");

static GtkWidget *conf_dialog = NULL;
static gdouble value;

EffectPlugin *stereo_eplist[] = { &stereo_ep, NULL };

DECLARE_PLUGIN(stereo, NULL, NULL, NULL, NULL, stereo_eplist, NULL, NULL, NULL);

static void init(void)
{
	mcs_handle_t *db;
	db = aud_cfg_db_open();
	if (!aud_cfg_db_get_double(db, "extra_stereo", "intensity", &value))
		value = 2.5;
	aud_cfg_db_close(db);
}

static void about (void)
{
	static GtkWidget * about_dialog = NULL;

    audgui_simple_message (& about_dialog, GTK_MESSAGE_INFO,
     _("About Extra Stereo Plugin"), _(about_text));
}

static void conf_ok_cb(GtkButton * button, gpointer data)
{
	mcs_handle_t *db;

	value = *(gdouble *) data;

	db = aud_cfg_db_open();
	aud_cfg_db_set_double(db, "extra_stereo", "intensity", value);
	aud_cfg_db_close(db);
	gtk_widget_destroy(conf_dialog);
}

static void conf_cancel_cb(GtkButton * button, gpointer data)
{
	gtk_widget_destroy(conf_dialog);
}

static void conf_apply_cb(GtkButton *button, gpointer data)
{
	value = *(gdouble *) data;
}

static void configure(void)
{
	GtkWidget *hbox, *label, *scale, *button, *bbox;
	GtkObject *adjustment;

	if (conf_dialog != NULL)
		return;

	conf_dialog = gtk_dialog_new();
	gtk_signal_connect(GTK_OBJECT(conf_dialog), "destroy",
			   GTK_SIGNAL_FUNC(gtk_widget_destroyed), &conf_dialog);
	gtk_window_set_title(GTK_WINDOW(conf_dialog), _("Configure Extra Stereo"));

	label = gtk_label_new(_("Effect intensity:"));
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(conf_dialog)->vbox), label,
			   TRUE, TRUE, 0);
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, 10);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(conf_dialog)->vbox), hbox,
			   TRUE, TRUE, 10);
	gtk_widget_show(hbox);

	adjustment = gtk_adjustment_new(value, 0.0, 15.0 + 1.0, 0.1, 1.0, 1.0);
	scale = gtk_hscale_new(GTK_ADJUSTMENT(adjustment));
	gtk_box_pack_start(GTK_BOX(hbox), scale, TRUE, TRUE, 10);
	gtk_widget_show(scale);

	bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 5);
	gtk_box_pack_start(GTK_BOX((GTK_DIALOG(conf_dialog)->action_area)),
			   bbox, TRUE, TRUE, 0);

	button = gtk_button_new_with_label(_("Ok"));
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(conf_ok_cb),
			   &GTK_ADJUSTMENT(adjustment)->value);
	gtk_widget_grab_default(button);
	gtk_widget_show(button);

	button = gtk_button_new_with_label(_("Cancel"));
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(conf_cancel_cb), NULL);
	gtk_widget_show(button);

	button = gtk_button_new_with_label(_("Apply"));
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(conf_apply_cb),
			   &GTK_ADJUSTMENT(adjustment)->value);
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
