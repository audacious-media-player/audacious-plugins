/*
 * Audacious
 * Copyright (c) 2006-2007 Audacious development team.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses>.
 *
 * The Audacious team does not consider modular code linking to
 * Audacious or using our public API to be a derived work.
 */

#include "ui_playlist_evlisteners.h"

#include <glib.h>
#if 0
#include "hook.h"
#include "playlist.h"
#endif
#include "ui_playlist.h"
#if 0
#include "ui_playlist_manager.h"
#endif
static void
ui_playlist_evlistener_playlist_update(gpointer hook_data, gpointer user_data)
{
    Playlist *playlist = (Playlist *) hook_data;
    if (playlist != NULL)
        playlistwin_update_list(playlist);
#if 0
    playlist_manager_update();
#endif
}

static void
ui_playlist_evlistener_playlistwin_show(gpointer hook_data, gpointer user_data)
{
    gboolean *show = (gboolean*)hook_data;
    playlistwin_show (* show);
}

void ui_playlist_evlistener_init(void)
{
    aud_hook_associate("playlist update", ui_playlist_evlistener_playlist_update, NULL);
    aud_hook_associate("playlistwin show", ui_playlist_evlistener_playlistwin_show, NULL);
}

void ui_playlist_evlistener_dissociate(void)
{
    aud_hook_dissociate("playlist update", ui_playlist_evlistener_playlist_update);
    aud_hook_dissociate("playlistwin show", ui_playlist_evlistener_playlistwin_show);
}

