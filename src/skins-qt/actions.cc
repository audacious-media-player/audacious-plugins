/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2011  Audacious development team.
 *
 *  BMP - Cross-platform multimedia player
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#include "actions-mainwin.h"
#include "actions-playlist.h"
#include "main.h"
#include "plugin-window.h"

#include "../ui-common/menu-ops.h"

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugins.h>
#include <libaudqt/libaudqt.h>

#define ACTIVE (Playlist::active_playlist ())

void action_ab_clear ()
{
    mainwin_show_status_message (_("Repeat points cleared."));
    aud_drct_set_ab_repeat (-1, -1);
}

void action_ab_set ()
{
    if (aud_drct_get_length () > 0)
    {
        int a, b;
        aud_drct_get_ab_repeat (a, b);

        if (a < 0 || b >= 0)
        {
            a = aud_drct_get_time ();
            b = -1;
            mainwin_show_status_message (_("Repeat point A set."));
        }
        else
        {
            b = aud_drct_get_time ();
            mainwin_show_status_message (_("Repeat point B set."));
        }

        aud_drct_set_ab_repeat (a, b);
    }
}

void action_play_file ()
    { audqt::fileopener_show (audqt::FileMode::Open); }
void action_play_folder ()
    { audqt::fileopener_show (audqt::FileMode::OpenFolder); }
void action_play_location ()
    { audqt::urlopener_show (true); }

void action_playlist_manager ()
{
    PluginHandle * manager = aud_plugin_lookup_basename ("playlist-manager-qt");
    if (manager)
    {
        aud_plugin_enable (manager, true);
        focus_plugin_window (manager);
    }
}

void action_search_tool ()
{
    PluginHandle * search = aud_plugin_lookup_basename ("search-tool");
    if (search)
    {
        aud_plugin_enable (search, true);
        focus_plugin_window (search);
    }
}

void action_playlist_rename ()
    { audqt::playlist_show_rename (ACTIVE); }
void action_playlist_delete ()
    { audqt::playlist_confirm_delete (ACTIVE); }

void action_playlist_add_url ()
    { audqt::urlopener_show (false); }
void action_playlist_add_files ()
    { audqt::fileopener_show (audqt::FileMode::Add ); }
void action_playlist_add_folder ()
    { audqt::fileopener_show (audqt::FileMode::AddFolder ); }
