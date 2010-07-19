/*
 * Copyright (c) 2008 William Pitcock <nenolod@nenolod.net>
 * Copyright (c) 2010 John Lindgren <john.lindgren@tds.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <gtk/gtk.h>

#include <audacious/configdb.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>

static void init(void);
static void configure(void);
static void cryst_start (gint * channels, gint * rate);
static void cryst_process (gfloat * * data, gint * samples);
static void cryst_flush ();
static void cryst_finish (gfloat * * data, gint * samples);
static gint cryst_decoder_to_output_time (gint time);
static gint cryst_output_to_decoder_time (gint time);

EffectPlugin crystalizer_ep =
{
	.description = "Crystalizer", /* Description */
	.init = init,
	.configure = configure,
    .start = cryst_start,
    .process = cryst_process,
    .flush = cryst_flush,
    .finish = cryst_finish,
    .decoder_to_output_time = cryst_decoder_to_output_time,
    .output_to_decoder_time = cryst_output_to_decoder_time,
    .preserves_format = TRUE,
};

static GtkWidget *conf_dialog = NULL;
static gdouble value;
static gint cryst_channels;
static gfloat * cryst_prev;

EffectPlugin *crystalizer_eplist[] = { &crystalizer_ep, NULL };

DECLARE_PLUGIN(crystalizer, NULL, NULL, NULL, NULL, crystalizer_eplist, NULL, NULL, NULL);

static void init(void)
{
	mcs_handle_t *db;
	db = aud_cfg_db_open();
	if (!aud_cfg_db_get_double(db, "crystalizer", "intensity", &value))
		value = 1.0;
	aud_cfg_db_close(db);
}

/* conf dialog stuff stolen from stereo plugin --nenolod */
static void conf_ok_cb(GtkButton * button, gpointer data)
{
	mcs_handle_t *db;

	value = *(gdouble *) data;

	db = aud_cfg_db_open();
	aud_cfg_db_set_double(db, "crystalizer", "intensity", value);
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
	g_signal_connect(GTK_OBJECT(conf_dialog), "destroy",
			   GTK_SIGNAL_FUNC(gtk_widget_destroyed), &conf_dialog);
	gtk_window_set_title(GTK_WINDOW(conf_dialog), _("Configure Crystalizer"));

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
	g_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(conf_ok_cb),
			   &GTK_ADJUSTMENT(adjustment)->value);
	gtk_widget_grab_default(button);
	gtk_widget_show(button);

	button = gtk_button_new_with_label(_("Cancel"));
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
	g_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(conf_cancel_cb), NULL);
	gtk_widget_show(button);

	button = gtk_button_new_with_label(_("Apply"));
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
	g_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(conf_apply_cb),
			   &GTK_ADJUSTMENT(adjustment)->value);
	gtk_widget_show(button);

	gtk_widget_show(bbox);

	gtk_widget_show(conf_dialog);
}

static void cryst_start (gint * channels, gint * rate)
{
    cryst_channels = * channels;
    cryst_prev = g_realloc (cryst_prev, sizeof (gfloat) * cryst_channels);
    memset (cryst_prev, 0, sizeof (gfloat) * cryst_channels);
}

static void cryst_process (gfloat * * data, gint * samples)
{
    gfloat * f = * data;
    gfloat * end = f + (* samples);
    gint channel;

    while (f < end)
    {
        for (channel = 0; channel < cryst_channels; channel ++)
        {
            gfloat current = * f;

            * f ++ = current + (current - cryst_prev[channel]) * value;
            cryst_prev[channel] = current;
        }
    }
}

static void cryst_flush ()
{
    memset (cryst_prev, 0, sizeof (gfloat) * cryst_channels);
}

static void cryst_finish (gfloat * * data, gint * samples)
{
    cryst_process (data, samples);
}

static gint cryst_decoder_to_output_time (gint time)
{
    return time;
}

static gint cryst_output_to_decoder_time (gint time)
{
    return time;
}
