/*
 * gtkui.h
 * Copyright 2011 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#ifndef GTKUI_H
#define GTKUI_H

#include <gtk/gtk.h>

/* menus.c */
GtkWidget * make_menu_bar (GtkAccelGroup * accel);
GtkWidget * make_menu_main (GtkAccelGroup * accel);
GtkWidget * make_menu_rclick (GtkAccelGroup * accel);
GtkWidget * make_menu_tab (GtkAccelGroup * accel);
extern int menu_tab_playlist_id;

/* ui_gtk.c */
void set_ab_repeat_a (void);
void set_ab_repeat_b (void);
void clear_ab_repeat (void);
void show_menu (gboolean show);
void show_playlist_tabs (void);
void show_infoarea (gboolean show);
void show_infoarea_vis (gboolean show);
void show_statusbar (gboolean show);
void popup_menu_rclick (guint button, guint32 time);
void popup_menu_tab (guint button, guint32 time, int playlist);
void activate_search_tool (void);

#endif
