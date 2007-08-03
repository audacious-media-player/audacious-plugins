/*
 *  Copyright 2006 Christian Birchinger <joker@netswarm.net>
 *
 *  Based on the XMMS plugin:
 *  Copyright 2000 H�vard Kv�len <havardk@sol.no>
 *
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <glib.h>
#include <audacious/i18n.h>
#include <gtk/gtk.h>
#include <audacious/plugin.h>
#include <audacious/util.h>
#include <audacious/configdb.h>
#include "../../config.h"

static GTimer *timer;
static gulong offset_time, written;
static gint bps;
static gboolean real_time = TRUE;
static gboolean paused, started;
static GtkWidget *configurewin;
static struct {
	AFormat format;
	gint frequency;
	gint channels;
} input_format;

#define ELAPSED_TIME (offset_time + g_timer_elapsed(timer, NULL) * 1000)

static void null_init(void)
{
	ConfigDb *db;
	db = bmp_cfg_db_open();
	bmp_cfg_db_get_bool(db, "null", "real_time", &real_time);
	bmp_cfg_db_close(db);
}

static void null_about(void)
{
	static GtkWidget *about;
	gchar *about_text;

	if (about)
		return;

	about_text = g_strjoin("", _("Null output plugin "), VERSION,
			           _(" by Christian Birchinger <joker@netswarm.net>\n"
			             "based on the XMMS plugin by Håvard Kvål <havardk@xmms.org>"), NULL);

	about = xmms_show_message(_("About Null Output"),
				  about_text,
				  _("Ok"), FALSE, NULL, NULL);

	g_signal_connect(G_OBJECT(about), "destroy",
			 G_CALLBACK(gtk_widget_destroyed), &about);
	g_free(about_text);
}

static void null_configure_ok_cb(GtkButton *w, gpointer data)
{
	ConfigDb *db;

	db = bmp_cfg_db_open();
	real_time = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data));
	bmp_cfg_db_set_bool(db, "null", "real_time", real_time);
	bmp_cfg_db_close(db);
	gtk_widget_destroy(configurewin);
}


static void null_configure(void)
{
	GtkWidget *rt_btn, *ok_button, *cancel_button, *vbox, *bbox;

	if (configurewin)
		return;

	configurewin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(configurewin), _("Null output preferences"));
	gtk_window_set_policy(GTK_WINDOW(configurewin), FALSE, FALSE, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(configurewin), 10);
	gtk_signal_connect(GTK_OBJECT(configurewin), "destroy",
			   GTK_SIGNAL_FUNC(gtk_widget_destroyed), &configurewin);

	vbox = gtk_vbox_new(FALSE, 10);
	gtk_container_add(GTK_CONTAINER(configurewin), vbox);

	rt_btn = gtk_check_button_new_with_label(_("Run in real time"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rt_btn), real_time);
	gtk_box_pack_start(GTK_BOX(vbox), rt_btn, FALSE, FALSE, 0);
	bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 5);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);
	ok_button = gtk_button_new_with_label(_("Ok"));
	cancel_button = gtk_button_new_with_label(_("Cancel"));
	GTK_WIDGET_SET_FLAGS(ok_button, GTK_CAN_DEFAULT);
	GTK_WIDGET_SET_FLAGS(cancel_button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default(ok_button);
	gtk_signal_connect_object(GTK_OBJECT(cancel_button), "clicked",
				  GTK_SIGNAL_FUNC(gtk_widget_destroy),
				  GTK_OBJECT(configurewin));
	gtk_signal_connect(GTK_OBJECT(ok_button), "clicked",
			   GTK_SIGNAL_FUNC(null_configure_ok_cb), rt_btn);
	gtk_box_pack_start_defaults(GTK_BOX(bbox), ok_button);
	gtk_box_pack_start_defaults(GTK_BOX(bbox), cancel_button);

	gtk_widget_show_all(configurewin);
}

static int null_open(AFormat fmt, int rate, int nch)
{
	offset_time = 0;
	written = 0;
	started = FALSE;
	paused = FALSE;
	input_format.format = fmt;
	input_format.frequency = rate;
	input_format.channels = nch;
	bps = rate * nch;
	switch (fmt)
	{
		case FMT_U8:
		case FMT_S8:
			break;
		default:
			bps <<= 1;
			break;
	}
	if (real_time)
		timer = g_timer_new();
	return 1;
}

static void null_write(void *ptr, int length)
{
#if 0
	EffectPlugin *ep;
#endif
	if (timer && !started)
	{
		g_timer_start(timer);
		started = TRUE;
	}

#if 0
	if ((ep = get_current_effect_plugin()) != NULL &&
	    effects_enabled() && ep->mod_samples)
		ep->mod_samples(&ptr, length, input_format.format,
				input_format.frequency, input_format.channels);
#endif

	written += length;
}

static void null_close(void)
{
	if (timer)
		g_timer_destroy(timer);
	timer = NULL;
}

static void null_flush(int time)
{
	offset_time = time;
	written = ((double)time * bps) / 1000;
	if (timer)
		g_timer_reset(timer);
}

static void null_pause(short p)
{
	paused = p;
	if (!timer)
		return;

	if (paused)
		g_timer_stop(timer);
	else
	{
		offset_time += g_timer_elapsed(timer, NULL) * 1000;
		g_timer_start(timer);
	}
}

static int null_buffer_free(void)
{
	if (timer)
	{
		return 10240 - (written - (ELAPSED_TIME * bps) / 1000);
	}
	else
	{
		if (!paused)
			return 10000;
		else
			return 0;
	}
}

static int null_playing(void)
{
	if (!timer)
		return FALSE;

	if ((gdouble)(written * 1000) / bps > ELAPSED_TIME)
		return TRUE;
	return FALSE;
}

static int null_get_written_time(void)
{
	if (!bps)
		return 0;
	return ((gint64)written * 1000) / bps;
}

static int null_get_output_time(void)
{
	if (!timer)
		return null_get_written_time();

	return ELAPSED_TIME;
}

OutputPlugin null_op =
{
	NULL,
	NULL,
	"Null Output Plugin",
	null_init,
	NULL,			/* cleanup */
	null_about,
	null_configure,
	NULL,			/* Get volume */
	NULL,			/* Set volume */
	null_open,
	null_write,
	null_close,
	null_flush,
	null_pause,
	null_buffer_free,
	null_playing,
	null_get_output_time,
	null_get_written_time,
	NULL			/* tell */
};

OutputPlugin *null_oplist[] = { &null_op, NULL };

DECLARE_PLUGIN(null, NULL, NULL, NULL, null_oplist, NULL, NULL, NULL, NULL);
