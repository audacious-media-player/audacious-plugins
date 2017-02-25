/*
 * menus.h
 * Copyright 2009-2014 John Lindgren
 *
 * This file is part of Audacious.
 *
 * Audacious is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2 or version 3 of the License.
 *
 * Audacious is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Audacious. If not, see <http://www.gnu.org/licenses/>.
 *
 * The Audacious team does not consider modular code linking to Audacious or
 * using our public API to be a derived work.
 */

#ifndef SKINS_MENUS_H
#define SKINS_MENUS_H

class QWidget;

enum {
    UI_MENU_MAIN,
    UI_MENU_PLAYBACK,
    UI_MENU_PLAYLIST,
    UI_MENU_VIEW,
    UI_MENU_PLAYLIST_ADD,
    UI_MENU_PLAYLIST_REMOVE,
    UI_MENU_PLAYLIST_SELECT,
    UI_MENU_PLAYLIST_SORT,
    UI_MENU_PLAYLIST_CONTEXT,
    UI_MENUS
};

void menu_init (QWidget * parent);
void menu_popup (int id, int x, int y, bool leftward, bool upward);

#endif /* SKINS_MENUS_H */
