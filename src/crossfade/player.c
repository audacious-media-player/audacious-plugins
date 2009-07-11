/*
 *  XMMS Crossfade Plugin
 *  Copyright (C) 2000-2007  Peter Eisenlohr <peter@eisenlohr.org>
 *
 *  based on the original OSS Output Plugin
 *  Copyright (C) 1998-2000  Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson and 4Front Technologies
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 *  USA.
 */

#include <audacious/plugin.h>
#include "player.h"

gboolean xfplayer_input_playing() {
    return audacious_drct_get_playing();
}

gint xfplaylist_get_position() {
    Playlist *playlist = aud_playlist_get_active();
    return aud_playlist_get_position(playlist);
}

gchar *xfplaylist_get_filename(gint pos) {
    Playlist *playlist = aud_playlist_get_active();
    char *uri = aud_playlist_get_filename(playlist, pos);
    return g_strdup(uri);
}

gchar *xfplaylist_get_songtitle(gint pos) {
    Playlist *playlist = aud_playlist_get_active();
    return aud_playlist_get_songtitle(playlist, pos);
}

gint xfplaylist_current_length() {
    Playlist *playlist = aud_playlist_get_active();
    return aud_playlist_get_current_length(playlist);
}

GList *xfplayer_get_output_list() {
    return aud_get_output_list();
}

gboolean xfplayer_check_realtime_priority() {
    return FALSE;
}
