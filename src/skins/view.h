/*
 * view.h
 * Copyright 2014 John Lindgren
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

#ifndef SKINS_VIEW_H
#define SKINS_VIEW_H

#include <libaudcore/core.h>

void view_show_player (bool_t show);

void view_set_show_playlist (bool_t show);
void view_apply_show_playlist (void);

void view_set_show_equalizer (bool_t show);
void view_apply_show_equalizer (void);

void view_set_player_shaded (bool_t shaded);
void view_apply_player_shaded (void);

void view_set_playlist_shaded (bool_t shaded);
void view_apply_playlist_shaded (void);

void view_set_equalizer_shaded (bool_t shaded);
void view_apply_equalizer_shaded (void);

void view_set_on_top (bool_t on_top);
void view_apply_on_top (void);

void view_set_sticky (bool_t sticky);
void view_apply_sticky (void);

void view_set_show_remaining (bool_t remaining);
void view_apply_show_remaining (void);

#endif /* SKINS_VIEW_H */
