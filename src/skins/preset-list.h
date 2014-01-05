/*
 * preset-list.h
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

#ifndef SKINS_PRESET_LIST_H
#define SKINS_PRESET_LIST_H

void eq_preset_load (void);
void eq_preset_load_auto (void);
void eq_preset_save (void);
void eq_preset_save_auto (void);
void eq_preset_delete (void);
void eq_preset_delete_auto (void);

void eq_preset_load_default (void);
void eq_preset_save_default (void);
void eq_preset_set_zero (void);

void eq_preset_list_cleanup (void);

#endif /* SKINS_PRESET_LIST_H */
