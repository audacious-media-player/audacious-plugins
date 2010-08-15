/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2010  Audacious development team
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

#ifndef UI_PLAYLIST_NOTEBOOK_H
#define UI_PLAYLIST_NOTEBOOK_H

#include <gtk/gtk.h>

#define UI_PLAYLIST_NOTEBOOK ui_playlist_get_notebook()

GtkNotebook *ui_playlist_get_notebook(void);
GtkWidget *ui_playlist_notebook_new();
void ui_playlist_notebook_create_tab(gint playlist);
void ui_playlist_notebook_destroy_tab(gint playlist);
void ui_playlist_notebook_edit_tab_title(GtkWidget *ebox);
void ui_playlist_notebook_populate(void);
void ui_playlist_notebook_update(gpointer hook_data, gpointer user_data);
void ui_playlist_notebook_position (void * data, void * user);
void ui_playlist_notebook_add_tab_label_markup(gint playlist, gboolean new_title);

#endif
