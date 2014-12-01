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
    g_return_if_fail (row >= 0 && row < loadeds.len ());
    g_return_if_fail (column == 0);

    g_value_set_string (value, loadeds[row]->plugin.desc.Name);
}

static bool get_selected (void * user, int row)
{
    g_return_val_if_fail (row >= 0 && row < loadeds.len (), 0);

    return loadeds[row]->selected;
}

static void set_selected (void * user, int row, bool selected)
{
    g_return_if_fail (row >= 0 && row < loadeds.len ());

    loadeds[row]->selected = selected;
}

static void select_all (void * user, bool selected)
{
    for (auto & loaded : loadeds)
        loaded->selected = selected;
}

static void shift_rows (void * user, int row, int before)
{
    int rows = loadeds.len ();
    g_return_if_fail (row >= 0 && row < rows);
    g_return_if_fail (before >= 0 && before <= rows);

    if (before == row)
        return;

    pthread_mutex_lock (& mutex);

    Index<SmartPtr<LoadedPlugin>> move;
    Index<SmartPtr<LoadedPlugin>> others;

    int begin, end;
    if (before < row)
    {
        begin = before;
        end = row + 1;
        while (end < rows && loadeds[end]->selected)
            end ++;
    }
    else
    {
        begin = row;
        while (begin > 0 && loadeds[begin - 1]->selected)
            begin --;
        end = before;
    }

    for (int i = begin; i < end; i ++)
    {
        if (loadeds[i]->selected)
            move.append (std::move (loadeds[i]));
        else
            others.append (std::move (loadeds[i]));
    }

    if (before < row)
        move.move_from (others, 0, -1, -1, true, true);
    else
        move.move_from (others, 0, 0, -1, true, true);

    loadeds.move_from (move, 0, begin, end - begin, false, true);

    pthread_mutex_unlock (& mutex);

    if (loaded_list)
        update_loaded_list (loaded_list);
}

static const AudguiListCallbacks callbacks = {
    get_value,
    get_selected,
    set_selected,
    select_all,
    nullptr,  // activate_row
    nullptr,  // right_click
    shift_rows
};

GtkWidget * create_loaded_list ()
{
    GtkWidget * list = audgui_list_new (& callbacks, nullptr, loadeds.len ());
    audgui_list_add_column (list, nullptr, 0, G_TYPE_STRING, -1);
    gtk_tree_view_set_headers_visible ((GtkTreeView *) list, 0);
    return list;
}

void update_loaded_list (GtkWidget * list)
{
    audgui_list_delete_rows (list, 0, audgui_list_row_count (list));
    audgui_list_insert_rows (list, 0, loadeds.len ());
}
