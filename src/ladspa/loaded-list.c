#include <libaudgui/list.h>

#include "plugin.h"

/* TODO: reorder */
/* TODO: configure plugin on double-click */

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

static const AudguiListCallbacks callbacks = {
 .get_value = get_value,
 .get_selected = get_selected,
 .set_selected = set_selected,
 .select_all = select_all};

GtkWidget * create_loaded_list (void)
{
    GtkWidget * list = audgui_list_new (& callbacks, NULL, index_count (loadeds));
    audgui_list_add_column (list, NULL, 0, G_TYPE_STRING, 1);
    gtk_tree_view_set_headers_visible ((GtkTreeView *) list, 0);
    return list;
}

void update_loaded_list (GtkWidget * list)
{
    audgui_list_delete_rows (list, 0, audgui_list_row_count (list));
    audgui_list_insert_rows (list, 0, index_count (loadeds));
}
