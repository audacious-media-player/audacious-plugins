/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2007  Audacious development team.
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

#ifndef AUDACIOUS_UI_MANAGER_H
#define AUDACIOUS_UI_MANAGER_H

#include <gtk/gtk.h>

enum
{
    UI_MENU_MAIN,
    UI_MENU_PLAYBACK,
    UI_MENU_PLAYLIST,
    UI_MENU_SONGNAME,
    UI_MENU_VIEW,
    UI_MENU_VISUALIZATION,
    UI_MENU_PLAYLIST_ADD,
    UI_MENU_PLAYLIST_REMOVE,
    UI_MENU_PLAYLIST_SELECT,
    UI_MENU_PLAYLIST_SORT,
    UI_MENU_PLAYLIST_GENERAL,
    UI_MENU_PLAYLIST_CONTEXT,
    UI_MENU_EQUALIZER_PRESET,
    UI_MENUS
};

extern GtkActionGroup *toggleaction_group_others;
extern GtkActionGroup *radioaction_group_anamode; /* Analyzer mode */
extern GtkActionGroup *radioaction_group_anatype; /* Analyzer type */
extern GtkActionGroup *radioaction_group_scomode; /* Scope mode */
extern GtkActionGroup *radioaction_group_vprmode; /* Voiceprint mode */
extern GtkActionGroup *radioaction_group_wshmode; /* WindowShade VU mode */
extern GtkActionGroup *radioaction_group_anafoff; /* Analyzer Falloff */
extern GtkActionGroup *radioaction_group_peafoff; /* Peak Falloff */
extern GtkActionGroup *radioaction_group_vismode; /* Visualization mode */
extern GtkActionGroup *radioaction_group_viewtime; /* View time (remaining/elapsed) */

void ui_manager_init ( void );
void ui_manager_create_menus ( void );
GtkAccelGroup * ui_manager_get_accel_group ( void );
void ui_popup_menu_show(gint id, gint x, gint y, gboolean leftward, gboolean
 upward, gint button, gint time);
void ui_manager_destroy( void );

#endif /* AUDACIOUS_UI_MANAGER_H */
