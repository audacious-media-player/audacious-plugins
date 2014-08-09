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

#ifndef SKINS_UI_SKINNED_PLAYSTATUS_H
#define SKINS_UI_SKINNED_PLAYSTATUS_H

#include <gtk/gtk.h>

typedef enum {
    STATUS_STOP, STATUS_PAUSE, STATUS_PLAY
} PStatus;

GtkWidget * ui_skinned_playstatus_new (void);
void ui_skinned_playstatus_set_status(GtkWidget *widget, PStatus status);
void ui_skinned_playstatus_set_size(GtkWidget *widget, int width, int height);

#endif
