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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

GtkWidget *playlistwin_popup_menu;
GtkWidget *playlist_tab_menu;

GtkActionGroup *toggleaction_group_others;

void ui_manager_init(void);
void ui_manager_create_menus(void);
GtkAccelGroup *ui_manager_get_accel_group(void);
GtkWidget *ui_manager_get_popup_menu(GtkUIManager *, const gchar *);
void ui_manager_popup_menu_show(GtkMenu *, gint, gint, guint, guint);
void ui_manager_destroy(void);

GtkWidget *ui_manager_get_menus(void);

G_END_DECLS
#endif /* AUDACIOUS_UI_MANAGER_H */
