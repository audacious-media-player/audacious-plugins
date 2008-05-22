/*
 * Copyright (c) 2008 William Pitcock <nenolod@nenolod.net>
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
#include <audacious/i18n.h>
#include <audacious/plugin.h>

static void init(void);
static void configure(void);
static int mod_samples(gpointer *d, gint length, AFormat afmt, gint srate, gint nch);
static void query_format(AFormat * fmt, gint * rate, gint * nch);

EffectPlugin crystalizer_ep =
{
	.description = "Crystalizer", /* Description */
	.init = init,
	.configure = configure,
	.mod_samples = mod_samples,
	.query_format = query_format
};

static GtkWidget *conf_dialog = NULL;
static gdouble value;

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

static void query_format(AFormat * fmt, gint * rate, gint * nch)
{
	if (!(*fmt == FMT_S16_NE ||
	      (*fmt == FMT_S16_LE && G_BYTE_ORDER == G_LITTLE_ENDIAN) ||
	      (*fmt == FMT_S16_BE && G_BYTE_ORDER == G_BIG_ENDIAN)))
		*fmt = FMT_S16_NE;
}

static int mod_samples(gpointer *d, gint length, AFormat afmt, gint srate, gint nch)
{
	gint i;
	gdouble tmp;
	static gdouble prev[2], diff[2];

	gint16  *data = (gint16 *)*d;

	if (!(afmt == FMT_S16_NE ||
	      (afmt == FMT_S16_LE && G_BYTE_ORDER == G_LITTLE_ENDIAN) ||
	      (afmt == FMT_S16_BE && G_BYTE_ORDER == G_BIG_ENDIAN)))
		return length;
	
	for (i = 0; i < length / 2; i += 2)
	{
		diff[0] = data[i] - prev[0];
		diff[1] = data[i + 1] - prev[1];
		prev[0] = data[i];
		prev[1] = data[i + 1];

		tmp = data[i] + (diff[0] * value);
		if (tmp < -32768)
			tmp = -32768;
		if (tmp > 32767)
			tmp = 32767;
		data[i] = tmp;

		tmp = data[i + 1] + (diff[1] * value);
		if (tmp < -32768)
			tmp = -32768;
		if (tmp > 32767)
			tmp = 32767;
		data[i + 1] = tmp;
	}

	return length;
}
