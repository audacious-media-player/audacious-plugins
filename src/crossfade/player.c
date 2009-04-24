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

#if defined(COMPILE_FOR_XMMS) /***********************************************/

gboolean get_input_playing();
gboolean xfplayer_input_playing() {
	return get_input_playing();
}

gint get_playlist_position();
gint xfplaylist_get_position() {
	return get_playlist_position();
}

gchar *playlist_get_filename(gint pos);
gchar *xfplaylist_get_filename(gint pos) {
	return playlist_get_filename(pos);
}

gchar *playlist_get_songtitle(gint pos);
gchar *xfplaylist_get_songtitle(gint pos) {
	return playlist_get_songtitle(pos);
}

gint playlist_get_current_length();
gint xfplaylist_current_length() {
	return playlist_get_current_length();
}

GList *get_output_list();
GList *xfplayer_get_output_list () {
	return get_output_list();
}

GList *get_effect_list();
GList *xfplayer_get_effect_list() {
	return get_effect_list();
}

gboolean xfplayer_effects_enabled() {
	return effects_enabled();
}

EffectPlugin *xfplayer_get_current_effect_plugin() {
	return get_current_effect_plugin();
}

gboolean xfplayer_check_realtime_priority() {
	return xmms_check_realtime_priority();
}

#elif defined(COMPILE_FOR_AUDACIOUS) /****************************************/
#if AUDACIOUS_ABI_VERSION >= 2 /*--------------------------------------------*/

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
	return g_filename_from_uri(uri, NULL, NULL);
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

#else /*---------------------------------------------------------------------*/

#if AUDACIOUS_ABI_VERSION >= 1
gboolean playback_get_playing();  /* >= Audacious-1.3.0 */
gboolean xfplayer_input_playing() {
	return playback_get_playing();
}
#else
gboolean bmp_playback_get_playing();  /* Audacious (old) */
gboolean xfplayer_input_playing() {
	return bmp_playback_get_playing();
}
#endif

gint xfplaylist_get_position() {
	return playlist_get_position(playlist_get_active());
}

gchar *xfplaylist_get_filename(gint pos) {
	char *uri = playlist_get_filename(playlist_get_active(), pos);
	return g_filename_from_uri(uri, NULL, NULL);
}

gchar *xfplaylist_get_songtitle(gint pos) {
	return playlist_get_songtitle(playlist_get_active(), pos);
}

gint xfplaylist_current_length() {
	return playlist_get_current_length(playlist_get_active());
}

GList *get_output_list();
GList *xfplayer_get_output_list() {
	return get_output_list();
}

#endif /*--------------------------------------------------------------------*/

GList *xfplayer_get_effect_list() {
	return NULL;
}

EffectPlugin *xfplayer_get_current_effect_plugin() {
	return NULL;
}

gboolean xfplayer_effects_enabled() {
	return FALSE;
}

gboolean xfplayer_check_realtime_priority() {
	return FALSE;
}

#endif /**********************************************************************/
