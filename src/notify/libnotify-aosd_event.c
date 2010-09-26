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
#include <audacious/plugin.h>
#include <audacious/playlist.h>
#include <audacious/debug.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "libnotify-aosd_common.h"

void event_init() {
	AUDDBG("started!\n");
	hook_associate("playback begin", event_playback_begin, NULL);
	hook_associate("playback pause", event_playback_pause, NULL);
	hook_associate("playback unpause", event_playback_begin, NULL);
	AUDDBG("done!");
}

void event_uninit() {
	AUDDBG("started!\n");
	hook_dissociate("playback begin", event_playback_begin);
	hook_dissociate("playback pause", event_playback_pause);
	hook_dissociate("playback unpause", event_playback_begin);
	AUDDBG("done!\n");
}

void event_playback_begin(gpointer p1, gpointer p2) {
	gint playlist, position;
	const gchar *title;
	const gchar *filename;
	GdkPixbuf *pb;

	AUDDBG("started!\n");

	playlist = aud_playlist_get_playing();
	position = aud_playlist_get_position(playlist);

	filename = aud_playlist_entry_get_filename(playlist, position);
	title = aud_playlist_entry_get_title(playlist, position, TRUE);

	pb = audgui_pixbuf_for_file(filename);
	audgui_pixbuf_scale_within(&pb, 64);

	osd_show(title, "notification-audio-play", pb);

	if (pb != NULL)
		g_object_unref(pb);

	AUDDBG("done!\n");
}

void event_playback_pause(gpointer p1, gpointer p2) {
	AUDDBG("started!\n");
	osd_show("Playback paused", "notification-audio-pause", NULL);
	AUDDBG("done!\n");
}
