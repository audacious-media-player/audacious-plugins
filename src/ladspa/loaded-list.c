/*
 * LADSPA Host for Audacious
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

#include <libaudgui/list.h>

#include "plugin.h"

static void get_value (void * user, int row, int column, GValue * value)
{
    g_return_if_fail (row >= 0 && row < index_count (loadeds));
    g_return_if_fail (column == 0);

    LoadedPlugin * loaded = index_get (loadeds, row);
    g_value_set_string (value, loaded->plugin->desc->Name);
}

static int get_selected (void * user, int row)
{
    g_return_val_if_fail (row >= 0 && row < index_count (loadeds), 0);

    LoadedPlugin * loaded = index_get (loadeds, row);
    return loaded->selected;
}

static void set_selected (void * user, int row, int selected)
{
    g_return_if_fail (row >= 0 && row < index_count (loadeds));

    LoadedPlugin * loaded = index_get (loadeds, row);
    loaded->selected = selected;
}

static void select_all (void * user, int selected)
{
    int count = index_count (loadeds);
    for (int i = 0; i < count; i ++)
    {
        LoadedPlugin * loaded = index_get (loadeds, i);
        loaded->selected = selected;
    }
}

static void shift_rows (void * user, int row, int before)
{
    int rows = index_count (loadeds);
    g_return_if_fail (row >= 0 && row < rows);
    g_return_if_fail (before >= 0 && before <= rows);

    if (before == row)
        return;

    pthread_mutex_lock (& mutex);

    Index * move = index_new ();
    Index * others = index_new ();

    int begin, end;
    if (before < row)
    {
        begin = before;
        end = row + 1;
        while (end < rows && ((LoadedPlugin *) index_get (loadeds, end))->selected)
            end ++;
    }
    else
    {
        begin = row;
        while (begin > 0 && ((LoadedPlugin *) index_get (loadeds, begin - 1))->selected)
            begin --;
        end = before;
    }

    for (gint i = begin; i < end; i ++)
    {
        LoadedPlugin * loaded = index_get (loadeds, i);
        index_append (loaded->selected ? move : others, loaded);
    }

    if (before < row)
    {
        index_merge_append (move, others);
        index_free (others);
    }
    else
    {
        index_merge_append (others, move);
        index_free (move);
        move = others;
    }

    index_copy_set (move, 0, loadeds, begin, end - begin);
    index_free (move);

    pthread_mutex_unlock (& mutex);

    if (loaded_list)
        update_loaded_list (loaded_list);
}

static const AudguiListCallbacks callbacks = {
 .get_value = get_value,
 .get_selected = get_selected,
 .set_selected = set_selected,
 .select_all = select_all,
 .shift_rows = shift_rows};

GtkWidget * create_loaded_list (void)
{
    GtkWidget * list = audgui_list_new (& callbacks, NULL, index_count (loadeds));
    audgui_list_add_column (list, NULL, 0, G_TYPE_STRING, -1);
    gtk_tree_view_set_headers_visible ((GtkTreeView *) list, 0);
    return list;
}

void update_loaded_list (GtkWidget * list)
{
    audgui_list_delete_rows (list, 0, audgui_list_row_count (list));
    audgui_list_insert_rows (list, 0, index_count (loadeds));
}
