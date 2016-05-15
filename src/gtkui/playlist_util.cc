/*
 * playlist_util.c
 * Copyright 2010-2011 Michał Lipski and John Lindgren
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

#include <string.h>
#include <gtk/gtk.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/interface.h>
#include <libaudcore/playlist.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/list.h>

#include "playlist_util.h"
#include "ui_playlist_notebook.h"

#include "../ui-common/menu-ops.h"

GtkWidget * playlist_get_treeview (int playlist)
{
    GtkWidget * page = gtk_notebook_get_nth_page (UI_PLAYLIST_NOTEBOOK, playlist);

    if (!page)
        return nullptr;

    return (GtkWidget *) g_object_get_data ((GObject *) page, "treeview");
}

int playlist_count_selected_in_range (int list, int top, int length)
{
    int selected = 0;

    for (int count = 0; count < length; count ++)
    {
        if (aud_playlist_entry_get_selected (list, top + count))
            selected ++;
    }

    return selected;
}

void playlist_shift (int offset)
{
    int list = aud_playlist_get_active ();
    int focus = aud_playlist_get_focus (list);

    if (focus < 0 || ! aud_playlist_entry_get_selected (list, focus))
        return;

    aud_playlist_shift (list, focus, offset);
}
