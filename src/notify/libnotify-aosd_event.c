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
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>

#include "libnotify-aosd_common.h"

void event_init() {
	DEBUG_PRINT("[%s] event_init: started!\n", __FILE__);
	hook_associate("playback begin", event_playback_begin, NULL);
	hook_associate("playback pause", event_playback_pause, NULL);
	hook_associate("playback unpause", event_playback_begin, NULL);
	DEBUG_PRINT("[%s] event_init: done!\n", __FILE__);
}

void event_uninit() {
	DEBUG_PRINT("[%s] event_uninit: started!\n", __FILE__);
	hook_dissociate("playback begin", event_playback_begin);
	hook_dissociate("playback pause", event_playback_pause);
	hook_dissociate("playback unpause", event_playback_begin);
	DEBUG_PRINT("[%s] event_uninit: done!\n", __FILE__);
}

void event_playback_begin(gpointer p1, gpointer p2) {
	gchar *aud_title = NULL, *title = NULL;
	DEBUG_PRINT("[%s] event_playback_begin: started!\n", __FILE__);

	aud_title = aud_drct_pl_get_title(aud_drct_pl_get_pos());

	if(aud_title != NULL) {
		title = str_to_utf8(aud_title);
		if(g_utf8_validate(title, -1, NULL)) {
			osd_show(title, "notification-audio-play");
		} else {
			DEBUG_PRINT("[%s] event_playback_begin: unvalid utf8 title!\n", __FILE__);
		}
		g_free(title);
	} else {
		DEBUG_PRINT("[%s] event_playback_begin: no aud title!\n", __FILE__);
	}

	DEBUG_PRINT("[%s] event_playback_begin: done!\n", __FILE__);
}

void event_playback_pause(gpointer p1, gpointer p2) {
	DEBUG_PRINT("[%s] event_playback_pause: started!\n", __FILE__);
	osd_show("Playback paused", "notification-audio-pause");
	DEBUG_PRINT("[%s] event_playback_pause: done!\n", __FILE__);
}
