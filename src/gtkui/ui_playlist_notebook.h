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

class Playlist;

extern GtkWidget * pl_notebook;

GtkWidget * pl_notebook_new ();

void pl_notebook_grab_focus ();
void pl_notebook_populate ();
void pl_notebook_purge ();

void pl_notebook_update (void * data, void * user);
void pl_notebook_activate (void * data, void * user);
void pl_notebook_set_playing (void * data, void * user);
void pl_notebook_set_position (void * data, void * user);

void start_rename_playlist (Playlist playlist);
void show_hide_playlist_tabs ();

#endif
