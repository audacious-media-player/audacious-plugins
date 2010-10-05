/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007 Tomasz Moń
 * Copyright (c) 2009 John Lindgren
 *
 * Based on:
 * BMP - Cross-platform multimedia player
 * Copyright (C) 2003-2004  BMP development team.
 * XMMS:
 * Copyright (C) 1998-2003  XMMS development team.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 * The Audacious team does not consider modular code linking to
 * Audacious or using our public API to be a derived work.
 */

#ifndef AUDACIOUS_UI_SKINNED_PLAYLIST_H
#define AUDACIOUS_UI_SKINNED_PLAYLIST_H

#include <gtk/gtk.h>

#include <cairo.h>
#include <pango/pangocairo.h>

#include "ui_skin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_SKINNED_PLAYLIST(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, ui_skinned_playlist_get_type (), UiSkinnedPlaylist)
#define UI_SKINNED_PLAYLIST_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, ui_skinned_playlist_get_type (), UiSkinnedPlaylistClass)
#define UI_SKINNED_IS_PLAYLIST(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, ui_skinned_playlist_get_type ())

typedef struct _UiSkinnedPlaylist        UiSkinnedPlaylist;
typedef struct _UiSkinnedPlaylistClass   UiSkinnedPlaylistClass;

struct _UiSkinnedPlaylist {
    GtkWidget   widget;
};

struct _UiSkinnedPlaylistClass {
    GtkWidgetClass    parent_class;
};

GtkWidget * ui_skinned_playlist_new (GtkWidget * fixed, gint x, gint y, gint
 width, gint height, const gchar * font);
void ui_skinned_playlist_set_slider (GtkWidget * list, GtkWidget * slider);
GType ui_skinned_playlist_get_type(void);
void ui_skinned_playlist_resize_relative(GtkWidget *widget, gint w, gint h);
void ui_skinned_playlist_set_font (GtkWidget * list, const gchar * font);
void ui_skinned_playlist_update (GtkWidget * widget);
gboolean ui_skinned_playlist_key (GtkWidget * widget, GdkEventKey * event);
void ui_skinned_playlist_row_info (GtkWidget * widget, gint * rows, gint *
 first, gint * focused);
void ui_skinned_playlist_scroll_to (GtkWidget * widget, gint row);
void ui_skinned_playlist_set_focused (GtkWidget * widget, gint row);
void ui_skinned_playlist_hover (GtkWidget * widget, gint x, gint y);
int ui_skinned_playlist_hover_end (GtkWidget * widget);

#ifdef __cplusplus
}
#endif

#endif /* AUDACIOUS_UI_SKINNED_PLAYLIST_H */
