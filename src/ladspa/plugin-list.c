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
    g_return_if_fail (row >= 0 && row < index_count (plugins));
    g_return_if_fail (column == 0);

    PluginData * plugin = index_get (plugins, row);
    g_value_set_string (value, plugin->desc->Name);
}

static int get_selected (void * user, int row)
{
    g_return_val_if_fail (row >= 0 && row < index_count (plugins), 0);

    PluginData * plugin = index_get (plugins, row);
    return plugin->selected;
}

static void set_selected (void * user, int row, int selected)
{
    g_return_if_fail (row >= 0 && row < index_count (plugins));

    PluginData * plugin = index_get (plugins, row);
    plugin->selected = selected;
}

static void select_all (void * user, int selected)
{
    int count = index_count (plugins);
    for (int i = 0; i < count; i ++)
    {
        PluginData * plugin = index_get (plugins, i);
        plugin->selected = selected;
    }
}

static const AudguiListCallbacks callbacks = {
 .get_value = get_value,
 .get_selected = get_selected,
 .set_selected = set_selected,
 .select_all = select_all};

GtkWidget * create_plugin_list (void)
{
    GtkWidget * list = audgui_list_new (& callbacks, NULL, index_count (plugins));
    audgui_list_add_column (list, NULL, 0, G_TYPE_STRING, 1);
    gtk_tree_view_set_headers_visible ((GtkTreeView *) list, 0);
    return list;
}

void update_plugin_list (GtkWidget * list)
{
    audgui_list_delete_rows (list, 0, audgui_list_row_count (list));
    audgui_list_insert_rows (list, 0, index_count (plugins));
}
