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
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "config.h"
#include "libnotify-aosd_common.h"

void event_init() {
	AUDDBG("started!\n");
	hook_associate("playback begin", event_playback_begin, NULL);
	hook_associate("playback pause", event_playback_pause, NULL);
	hook_associate("playback unpause", event_playback_begin, NULL);
	hook_associate("title change", event_playback_begin, NULL);
	AUDDBG("done!");
}

void event_uninit() {
	AUDDBG("started!\n");
	hook_dissociate("playback begin", event_playback_begin);
	hook_dissociate("playback pause", event_playback_pause);
	hook_dissociate("playback unpause", event_playback_begin);
	hook_dissociate("title change", event_playback_begin);
	AUDDBG("done!\n");
}

void event_playback_begin(gpointer p1, gpointer p2) {
	gint playlist, position;
	const gchar *title, *artist, *album;
	const gchar *filename;
	const Tuple *tuple;
	gchar *message;
	GdkPixbuf *pb;

	AUDDBG("started!\n");

	playlist = aud_playlist_get_playing();
	position = aud_playlist_get_position(playlist);

	filename = aud_playlist_entry_get_filename(playlist, position);
	tuple = aud_playlist_entry_get_tuple(playlist, position, FALSE);

	title = tuple_get_string(tuple, FIELD_TITLE, NULL);
	if (title == NULL)
		title = aud_playlist_entry_get_title(playlist, position, FALSE);

	artist = tuple_get_string(tuple, FIELD_ARTIST, NULL);
	album = tuple_get_string(tuple, FIELD_ALBUM, NULL);

	pb = audgui_pixbuf_for_file(filename);
	if (pb != NULL)
		audgui_pixbuf_scale_within(&pb, 128);

	message = g_strdup_printf("%s\n%s", (artist != NULL && artist[0]) ? artist :
	 _("Unknown artist"), (album != NULL && album[0]) ? album : _("Unknown album"));

	osd_show(title, message, "notification-audio-play", pb);
	g_free(message);

	if (pb != NULL)
		g_object_unref(pb);

	AUDDBG("done!\n");
}

void event_playback_pause(gpointer p1, gpointer p2) {
	AUDDBG("started!\n");
	osd_show("Playback paused", NULL, "notification-audio-pause", NULL);
	AUDDBG("done!\n");
}
