/*
 * menu-ops-gtk.cc
 * Copyright 2016 John Lindgren
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

#include "menu-ops.h"

#include <string.h>
#include <gtk/gtk.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/interface.h>
#include <libaudcore/playlist.h>
#include <libaudgui/libaudgui.h>

static void uri_get_func (GtkClipboard *, GtkSelectionData * sel, unsigned, void * data)
    { gtk_selection_data_set_uris (sel, (char * *) data); }
static void uri_clear_func (GtkClipboard *, void * data)
    { g_strfreev ((char * *) data); }

void pl_copy ()
{
    auto list = Playlist::active_playlist ();
    int entries = list.n_entries ();
    int selected = list.n_selected ();
    int fetched = 0;

    if (! selected)
        return;

    list.cache_selected ();

    char * * uris = g_new (char *, selected + 1);

    for (int i = 0; i < entries && fetched < selected; i ++)
    {
        if (list.entry_selected (i))
            uris[fetched ++] = g_strdup (list.entry_filename (i));
    }

    uris[fetched] = nullptr;

    GtkTargetList * tlist = gtk_target_list_new (nullptr, 0);
    gtk_target_list_add_uri_targets (tlist, 0);

    GtkTargetEntry * targets; int n_targets;
    targets = gtk_target_table_new_from_list (tlist, & n_targets);

    gtk_clipboard_set_with_data (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
     targets, n_targets, uri_get_func, uri_clear_func, uris);

    gtk_target_table_free (targets, n_targets);
    gtk_target_list_unref (tlist);
}

void pl_cut ()
{
    pl_copy ();
    pl_remove_selected ();
}

static void paste_to (Playlist list, int pos)
{
    char * * uris = gtk_clipboard_wait_for_uris (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD));
    if (! uris)
        return;

    Index<PlaylistAddItem> items;
    for (int i = 0; uris[i]; i ++)
        items.append (String (uris[i]));

    list.insert_items (pos, std::move (items), false);
    g_strfreev (uris);
}

void pl_paste ()
{
    auto list = Playlist::active_playlist ();
    paste_to (list, list.get_focus ());
}

void pl_paste_end ()
{
    paste_to (Playlist::active_playlist (), -1);
}

void pl_song_info ()
{
    auto list = Playlist::active_playlist ();
    int focus = list.get_focus ();
    if (focus >= 0)
        audgui_infowin_show (list, focus);
}

void pl_open_folder ()
{
    auto list = Playlist::active_playlist ();
    int focus = list.get_focus ();

    String filename = list.entry_filename (focus);
    if (! filename)
        return;

    const char * slash = strrchr (filename, '/');
    if (! slash)
        return;

    /* don't trim trailing slash, it may be important */
    StringBuf folder = str_copy (filename, slash + 1 - filename);

    GError * error = nullptr;
    gtk_show_uri (gdk_screen_get_default (), folder, GDK_CURRENT_TIME, & error);

    if (error)
    {
        aud_ui_show_error (error->message);
        g_error_free (error);
    }
}
