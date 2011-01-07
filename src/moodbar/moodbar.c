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
#include <libaudcore/hook.h>

static GdkColor *parse_mood_file(const gchar *filename)
{
	gchar *data, *it;
	gint64 size;
	GdkColor *ret, *col;

	g_return_val_if_fail(filename != NULL, NULL);

	ret = g_new0(GdkColor, 1000);
	vfs_file_get_contents(filename, (void **) &data, &size);

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

static void render_mood(GtkWidget *widget, const gchar *filename)
{
	gint i;
	GdkColor *mbdata;
	cairo_t *cr;
	GtkAllocation a;

	mbdata = parse_mood_file(filename);

	cr = gdk_cairo_create(widget->window);

	gtk_widget_get_allocation(widget, &a);

	for (i = 0; i < a.width; i++)
	{
		gdk_cairo_set_source_color(cr, &mbdata[(i * 1000) / a.width]);

		cairo_move_to(cr, i, 0);
		cairo_line_to(cr, i, a.height);
		cairo_stroke(cr);
	}

	cairo_destroy(cr);
}

static gboolean moodbar_init(void)
{
	return TRUE;
}

static gboolean expose_event(GtkWidget * widget)
{
	gint playlist, pos;
	gchar *file, *ext;
	gchar *moodfile;

	playlist = aud_playlist_get_playing();
	pos = aud_playlist_get_position(playlist);
	file = g_strdup(aud_playlist_entry_get_filename(playlist, pos));

	ext = strrchr(file, '.');
	if (ext != NULL)
		*ext = '\0';

	moodfile = g_strconcat(file, ".mood", NULL);

	render_mood(widget, moodfile);

	g_free(file);
	g_free(moodfile);

	return TRUE;
}

static void playback_begin(gpointer unused, gpointer userdata)
{
	GtkWidget *area = userdata;

	gtk_widget_queue_draw(area);
}

GtkWidget *area;

static gpointer moodbar_widget(void)
{
	area = gtk_drawing_area_new();
	gtk_widget_show(area);

	hook_associate("playback begin", (HookFunction) playback_begin, area);

	g_signal_connect(area, "expose-event", (GCallback) expose_event, NULL);

	return area;
}

static void moodbar_done(void)
{
	hook_dissociate("playback begin", (HookFunction) playback_begin);

	g_object_unref(area);
}

VisPlugin moodbar_vp = {
	.description = "Moodbar",
	.init = moodbar_init,
	.cleanup = moodbar_done,
	.get_widget = moodbar_widget,
};

VisPlugin *moodbar_vplist[] = { &moodbar_vp, NULL };

SIMPLE_VISUAL_PLUGIN(moodbar, moodbar_vplist);
