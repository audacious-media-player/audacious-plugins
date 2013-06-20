/*
 * ui_playlist_notebook.h
 * Copyright 2010-2012 Michał Lipski and John Lindgren
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

#ifndef UI_PLAYLIST_NOTEBOOK_H
#define UI_PLAYLIST_NOTEBOOK_H

#include <gtk/gtk.h>

#define UI_PLAYLIST_NOTEBOOK ui_playlist_get_notebook()

GtkNotebook *ui_playlist_get_notebook(void);
GtkWidget *ui_playlist_notebook_new();
void ui_playlist_notebook_create_tab(gint playlist);
void ui_playlist_notebook_edit_tab_title (int playlist);
void ui_playlist_notebook_populate(void);
void ui_playlist_notebook_empty (void);
void ui_playlist_notebook_update (void * data, void * user);
void ui_playlist_notebook_activate (void * data, void * user);
void ui_playlist_notebook_set_playing (void * data, void * user);
void ui_playlist_notebook_position (void * data, void * user);

void playlist_show_entry_counts (gboolean show);
void playlist_show_close_buttons (gboolean show);
void playlist_show_headers (gboolean show);

#endif
