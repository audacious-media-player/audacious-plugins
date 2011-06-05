/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007 Tomasz Moń
 * Copyright (c) 2011 John Lindgren
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

#ifndef SKINS_UI_SKINNED_TEXTBOX_H
#define SKINS_UI_SKINNED_TEXTBOX_H

#include <gtk/gtk.h>

GtkWidget * textbox_new (gint width, const gchar * text, const gchar * font,
 gboolean scroll);
void textbox_set_width (GtkWidget * textbox, gint width);
const gchar * textbox_get_text (GtkWidget * textbox);
void textbox_set_text (GtkWidget * textbox, const gchar * text);
void textbox_set_font (GtkWidget * textbox, const gchar * font);
void textbox_set_scroll (GtkWidget * textbox, gboolean scroll);

void textbox_update_all (void);

#endif
