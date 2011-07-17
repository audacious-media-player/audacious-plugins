/*
 * Copyright (c) 2011 William Pitcock <nenolod@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <gtk/gtk.h>
#include <audacious/plugin.h>
#include <audacious/playlist.h>
#include <audacious/drct.h>
#include <libaudcore/hook.h>

#include "draw-compat.h"

static GdkColor *parse_mood_file(const gchar *filename)
{
	gchar *data, *it;
	gint64 size;
	GdkColor *ret, *col;

	g_return_val_if_fail(filename != NULL, NULL);

	ret = g_new0(GdkColor, 1000);

	if (! vfs_file_test (filename, G_FILE_TEST_EXISTS))
		return ret;

	vfs_file_get_contents(filename, (void **) &data, &size);
	if (! data)
		return ret;

	for (it = data, col = ret; (it - data) < size; it += 3, col++)
	{
		gint16 c = *it;
		col->red = c * 256;

		c = *(it + 1);
		col->green = c * 256;

		c = *(it + 2);
		col->blue = c * 256;
	}

	g_free(data);

	return ret;
}

static void render_text (GtkWidget * widget, cairo_t * cr, gint x, gint y, gint
 width, gfloat r, gfloat g, gfloat b, gfloat a, const gchar * font, const
 gchar * text)
{
	PangoLayout *pl;
	PangoFontDescription *desc;
	gchar *str;

	str = g_markup_escape_text(text, -1);

	cairo_move_to(cr, x, y);
	cairo_set_operator(cr, CAIRO_OPERATOR_ATOP);
	cairo_set_source_rgba(cr, r, g, b, a);

	desc = pango_font_description_from_string(font);
	pl = gtk_widget_create_pango_layout (widget, NULL);
	pango_layout_set_markup(pl, str, -1);
	pango_layout_set_font_description(pl, desc);
	pango_font_description_free(desc);
	pango_layout_set_width (pl, width * PANGO_SCALE);
	pango_layout_set_ellipsize (pl, PANGO_ELLIPSIZE_END);

	pango_cairo_show_layout(cr, pl);

	g_object_unref(pl);
	g_free(str);
}

static void render_mood (GtkWidget * widget, cairo_t * cr, const gchar * filename)
{
	gint i;
	GdkColor *mbdata;
	GtkAllocation a;

	gint time = aud_drct_get_time () / 1000;
	gint length = aud_drct_get_length () / 1000;

	mbdata = parse_mood_file(filename);

	gtk_widget_get_allocation(widget, &a);

	for (i = 0; i < a.width; i++)
	{
		gdk_cairo_set_source_color(cr, &mbdata[(i * 1000) / a.width]);

		cairo_move_to(cr, i, 0);
		cairo_line_to(cr, i, a.height);
		cairo_stroke(cr);
	}

	if (mowgli_global_storage_get("gtkui.shbox"))
	{
		gchar *buf;

		buf = g_strdup_printf("%d:%02d / %d:%02d",
				     time / 60, time % 60,
				     length / 60, length % 60);

		render_text (widget, cr, (a.width / 2) - 20, (a.height / 2) - 10,
		 a.width, 1.0, 1.0, 1.0, 1.0, "Sans 9", buf);

		g_free(buf);
	}
}

DRAW_FUNC_BEGIN (expose_event)
	gint playlist, pos;
	gchar *file, *ext;
	gchar *moodfile;

	playlist = aud_playlist_get_playing();
	pos = aud_playlist_get_position(playlist);
	file = aud_playlist_entry_get_filename(playlist, pos);
	g_return_val_if_fail (file, FALSE);

	ext = strrchr(file, '.');
	if (ext != NULL)
		*ext = '\0';

	moodfile = g_strconcat(file, ".mood", NULL);

	render_mood (wid, cr, moodfile);

	g_free(file);
	g_free(moodfile);
DRAW_FUNC_END

static gint update_song_timeout_source = 0;

static gboolean time_counter_cb(gpointer userdata)
{
	GtkWidget *area = userdata;

	gtk_widget_queue_draw(area);

	return TRUE;
}

static void playback_begin(gpointer unused, gpointer userdata)
{
	GtkWidget *area = userdata;

	gtk_widget_queue_draw(area);

	if (mowgli_global_storage_get("gtkui.shbox"))
	{
		GtkWidget *shbox = mowgli_global_storage_get("gtkui.shbox");
		GtkWidget *slider = mowgli_global_storage_get("gtkui.slider");
		GtkWidget *label_time = mowgli_global_storage_get("gtkui.label_time");

		gtk_container_remove(GTK_CONTAINER(shbox), g_object_ref(slider));
		gtk_container_remove(GTK_CONTAINER(shbox), g_object_ref(label_time));

		gtk_box_pack_end(GTK_BOX(shbox), area, TRUE, TRUE, 6);

	        /* update time counter 4 times a second */
		if (!update_song_timeout_source)
			update_song_timeout_source = g_timeout_add(250, (GSourceFunc) time_counter_cb, area);
	}
}

static void playback_end(gpointer unused, gpointer userdata)
{
	GtkWidget *area = userdata;

	if (mowgli_global_storage_get("gtkui.shbox"))
	{
		GtkWidget *shbox = mowgli_global_storage_get("gtkui.shbox");
		GtkWidget *slider = mowgli_global_storage_get("gtkui.slider");
		GtkWidget *label_time = mowgli_global_storage_get("gtkui.label_time");

		gtk_container_remove(GTK_CONTAINER(shbox), area);

		gtk_container_add(GTK_CONTAINER(shbox), slider);
		gtk_container_add(GTK_CONTAINER(shbox), label_time);

		g_source_remove(update_song_timeout_source);
		update_song_timeout_source = 0;
	}
}

GtkWidget *area;

extern VisPlugin moodbar_vp;

static gboolean moodbar_init(void)
{
	area = gtk_drawing_area_new();
	g_object_ref (area);
	gtk_widget_show(area);

	hook_associate("playback begin", (HookFunction) playback_begin, area);
	hook_associate("playback end", (HookFunction) playback_end, area);

	g_signal_connect(area, DRAW_SIGNAL, (GCallback) expose_event, NULL);

	return TRUE;
}

static gpointer moodbar_widget(void)
{
	if (mowgli_global_storage_get("gtkui.shbox"))
	{
		g_print("shbox found\n");
		return NULL;
	}

	return area;
}

static void moodbar_done(void)
{
	hook_dissociate("playback begin", (HookFunction) playback_begin);
	hook_dissociate("playback end", (HookFunction) playback_end);

	g_object_unref(area);
}

AUD_VIS_PLUGIN
(
	.name = "Moodbar",
	.init = moodbar_init,
	.cleanup = moodbar_done,
	.get_widget = moodbar_widget,
)
