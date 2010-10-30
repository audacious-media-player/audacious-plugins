/************************************************************************
 * libnotify-aosd_event.c						*
 *									*
 * Copyright (C) 2010 Maximilian Bogner	<max@mbogner.de>		*
 *									*
 * This program is free software; you can redistribute it and/or modify	*
 * it under the terms of the GNU General Public License as published by	*
 * the Free Software Foundation; either version 3 of the License,	*
 * or (at your option) any later version.				*
 *									*
 * This program is distributed in the hope that it will be useful,	*
 * but WITHOUT ANY WARRANTY; without even the implied warranty of	*
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the		*
 * GNU General Public License for more details.				*
 *									*
 * You should have received a copy of the GNU General Public License	*
 * along with this program; if not, see <http://www.gnu.org/licenses/>.	*
 ************************************************************************/

#include <glib.h>

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <audacious/playlist.h>
#include <audacious/debug.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "config.h"
#include "libnotify-aosd_common.h"

void event_init() {
	AUDDBG("started!\n");
	hook_associate("playback begin", event_playback_begin, NULL);
	hook_associate("title change", event_playback_begin, NULL);
	AUDDBG("done!");
}

void event_uninit() {
	AUDDBG("started!\n");
	hook_dissociate("playback begin", event_playback_begin);
	hook_dissociate("title change", event_playback_begin);
	AUDDBG("done!\n");
}

void event_playback_begin (void * a, void * b)
{
	gint list = aud_playlist_get_playing ();
	gint entry = aud_playlist_get_position (list);

	gchar * title, * artist, * album;
	audgui_three_strings (list, entry, & title, & artist, & album);

	gchar * message;
	if (artist)
	{
		if (album)
			message = g_strdup_printf ("%s\n%s", artist, album);
		else
		{
			message = artist;
			artist = NULL;
		}
	}
	else if (album)
	{
		message = album;
		album = NULL;
	}
	else
		message = g_strdup ("");

	g_free (artist);
	g_free (album);

	GdkPixbuf * pb = audgui_pixbuf_for_entry (list, entry);
	if (pb != NULL)
		audgui_pixbuf_scale_within(&pb, 128);

	osd_show(title, message, "audio-x-generic", pb);
	g_free(title);
	g_free(message);

	if (pb != NULL)
		g_object_unref(pb);
}
