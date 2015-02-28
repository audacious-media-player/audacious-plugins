/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2014  Audacious development team.
 *
 *  Based on BMP:
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#include "preset-list.h"

#include <string.h>
#include <gtk/gtk.h>

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudgui/libaudgui-gtk.h>

#include "ui_equalizer.h"

static GtkWidget *equalizerwin_load_window = nullptr;
static GtkWidget *equalizerwin_load_auto_window = nullptr;
static GtkWidget *equalizerwin_save_window = nullptr;
static GtkWidget *equalizerwin_save_entry = nullptr;
static GtkWidget *equalizerwin_save_auto_window = nullptr;
static GtkWidget *equalizerwin_save_auto_entry = nullptr;
static GtkWidget *equalizerwin_delete_window = nullptr;
static GtkWidget *equalizerwin_delete_auto_window = nullptr;

static void
equalizerwin_delete_selected_presets(GtkTreeView *view, const char *filename)
{
    char *text;

    GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
    GtkTreeModel *model = gtk_tree_view_get_model(view);

    /*
     * first we are making a list of the selected rows, then we convert this
     * list into a list of GtkTreeRowReferences, so that when you delete an
     * item you can still access the other items
     * finally we iterate through all GtkTreeRowReferences, convert them to
     * GtkTreeIters and delete those one after the other
     */

    GList *list = gtk_tree_selection_get_selected_rows(selection, &model);
    GList *rrefs = nullptr;
    GList *litr;

    for (litr = list; litr; litr = litr->next)
    {
        GtkTreePath *path = (GtkTreePath *) litr->data;
        rrefs = g_list_append(rrefs, gtk_tree_row_reference_new(model, path));
    }

    for (litr = rrefs; litr; litr = litr->next)
    {
        GtkTreeRowReference *ref = (GtkTreeRowReference *) litr->data;
        GtkTreePath *path = gtk_tree_row_reference_get_path(ref);
        GtkTreeIter iter;
        gtk_tree_model_get_iter(model, &iter, path);
        gtk_tree_path_free(path);

        gtk_tree_model_get(model, &iter, 0, &text, -1);

        if (!strcmp(filename, "eq.preset"))
            equalizerwin_delete_preset (equalizer_presets, text, filename);
        else if (!strcmp(filename, "eq.auto_preset"))
            equalizerwin_delete_preset (equalizer_auto_presets, text, filename);

        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
    }
}

static void
equalizerwin_save_ok(GtkWidget * widget, void * data)
{
    const char *text;

    text = gtk_entry_get_text(GTK_ENTRY(equalizerwin_save_entry));

    if (text[0])
        equalizerwin_save_preset (equalizer_presets, text, "eq.preset");

    gtk_widget_destroy(equalizerwin_save_window);
}

static void
equalizerwin_save_select(GtkTreeView *treeview, GtkTreePath *path,
                         GtkTreeViewColumn *col, void * data)
{
    char *text;

    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (selection)
    {
        if (gtk_tree_selection_get_selected(selection, &model, &iter))
        {
            gtk_tree_model_get(model, &iter, 0, &text, -1);
            gtk_entry_set_text(GTK_ENTRY(equalizerwin_save_entry), text);
            equalizerwin_save_ok(nullptr, nullptr);

            g_free(text);
        }
    }
}

static void
equalizerwin_load_ok(GtkWidget *widget, void * data)
{
    char *text;

    GtkTreeView* view = GTK_TREE_VIEW(data);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (selection)
    {
        if (gtk_tree_selection_get_selected(selection, &model, &iter))
        {
            gtk_tree_model_get(model, &iter, 0, &text, -1);
            equalizerwin_load_preset(equalizer_presets, text);

            g_free(text);
        }
    }
    gtk_widget_destroy(equalizerwin_load_window);
}

static void
equalizerwin_load_select(GtkTreeView *treeview, GtkTreePath *path,
                         GtkTreeViewColumn *col, void * data)
{
    equalizerwin_load_ok(nullptr, treeview);
}

static void
equalizerwin_delete_delete(GtkWidget *widget, void * data)
{
    equalizerwin_delete_selected_presets(GTK_TREE_VIEW(data), "eq.preset");
}

static void
equalizerwin_save_auto_ok(GtkWidget *widget, void * data)
{
    const char *text;

    text = gtk_entry_get_text(GTK_ENTRY(equalizerwin_save_auto_entry));

    if (text[0])
        equalizerwin_save_preset (equalizer_auto_presets, text, "eq.auto_preset");

    gtk_widget_destroy(equalizerwin_save_auto_window);
}

static void
equalizerwin_save_auto_select(GtkTreeView *treeview, GtkTreePath *path,
                              GtkTreeViewColumn *col, void * data)
{
    char *text;

    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (selection)
    {
        if (gtk_tree_selection_get_selected(selection, &model, &iter))
        {
            gtk_tree_model_get(model, &iter, 0, &text, -1);
            gtk_entry_set_text(GTK_ENTRY(equalizerwin_save_auto_entry), text);
            equalizerwin_save_auto_ok(nullptr, nullptr);

            g_free(text);
        }
    }
}

static void
equalizerwin_load_auto_ok(GtkWidget *widget, void * data)
{
    char *text;

    GtkTreeView *view = GTK_TREE_VIEW(data);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (selection)
    {
        if (gtk_tree_selection_get_selected(selection, &model, &iter))
        {
            gtk_tree_model_get(model, &iter, 0, &text, -1);
            equalizerwin_load_preset(equalizer_auto_presets, text);

            g_free(text);
        }
    }
    gtk_widget_destroy(equalizerwin_load_auto_window);
}

static void
equalizerwin_load_auto_select(GtkTreeView *treeview, GtkTreePath *path,
                              GtkTreeViewColumn *col, void * data)
{
    equalizerwin_load_auto_ok(nullptr, treeview);
}

static void
equalizerwin_delete_auto_delete(GtkWidget *widget, void * data)
{
    equalizerwin_delete_selected_presets(GTK_TREE_VIEW(data), "eq.auto_preset");
}

static GtkWidget * equalizerwin_create_list_window
 (const Index<EqualizerPreset> & preset_list, const char * title,
 GtkWidget * * window, GtkSelectionMode sel_mode, GtkWidget * * entry,
 GtkWidget * button_action, GCallback action_func, GCallback select_row_func)
{
    GtkWidget *vbox, *scrolled_window, *bbox, *view;
    GtkWidget *button_cancel;

    GtkListStore *store;
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkCellRenderer *renderer;
    GtkTreeSelection *selection;
    GtkTreeSortable *sortable;

    *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(*window), title);
    gtk_window_set_type_hint(GTK_WINDOW(*window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_default_size(GTK_WINDOW(*window), 350, 300);
    gtk_container_set_border_width(GTK_CONTAINER(*window), 10);
    gtk_window_set_transient_for(GTK_WINDOW(*window),
                                 GTK_WINDOW(equalizerwin));
    g_signal_connect(*window, "destroy",
                     G_CALLBACK(gtk_widget_destroyed), window);

    audgui_destroy_on_escape (* window);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(*window), vbox);

    scrolled_window = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) scrolled_window, GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

    /* fill the store with the names of all available presets */
    store = gtk_list_store_new(1, G_TYPE_STRING);
    for (const EqualizerPreset & preset : preset_list)
    {
        gtk_list_store_append (store, & iter);
        gtk_list_store_set (store, & iter, 0, (const char *) preset.name, -1);
    }
    model = GTK_TREE_MODEL(store);

    sortable = GTK_TREE_SORTABLE(store);
    gtk_tree_sortable_set_sort_column_id(sortable, 0, GTK_SORT_ASCENDING);

    view = gtk_tree_view_new();
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1,
                                                _("Presets"), renderer,
                                                "text", 0, nullptr);
    gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);
    g_object_unref(model);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    gtk_tree_selection_set_mode(selection, sel_mode);

    gtk_container_add(GTK_CONTAINER(scrolled_window), view);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    if (entry) {
        *entry = gtk_entry_new();
        g_signal_connect(*entry, "activate", action_func, nullptr);
        gtk_box_pack_start(GTK_BOX(vbox), *entry, FALSE, FALSE, 0);
    }

    bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(bbox), 5);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

    button_cancel = audgui_button_new (_("Cancel"), "process-stop", nullptr, nullptr);
    g_signal_connect_swapped (button_cancel, "clicked", (GCallback)
     gtk_widget_destroy, * window);
    gtk_box_pack_start(GTK_BOX(bbox), button_cancel, TRUE, TRUE, 0);

    g_signal_connect(button_action, "clicked", G_CALLBACK(action_func), view);
    gtk_widget_set_can_default (button_action, TRUE);

    if (select_row_func)
        g_signal_connect(view, "row-activated", G_CALLBACK(select_row_func), nullptr);

    gtk_box_pack_start(GTK_BOX(bbox), button_action, TRUE, TRUE, 0);

    gtk_widget_grab_default(button_action);

    gtk_widget_show_all(*window);

    return *window;
}

void eq_preset_load (void)
{
    if (equalizerwin_load_window) {
        gtk_window_present(GTK_WINDOW(equalizerwin_load_window));
        return;
    }

    equalizerwin_create_list_window(equalizer_presets,
                                    _("Load preset"),
                                    &equalizerwin_load_window,
                                    GTK_SELECTION_SINGLE, nullptr,
                                    audgui_button_new (_("Load"), "document-open", nullptr, nullptr),
                                    G_CALLBACK(equalizerwin_load_ok),
                                    G_CALLBACK(equalizerwin_load_select));
}

void eq_preset_load_auto (void)
{
    if (equalizerwin_load_auto_window) {
        gtk_window_present(GTK_WINDOW(equalizerwin_load_auto_window));
        return;
    }

    equalizerwin_create_list_window(equalizer_auto_presets,
                                    _("Load auto-preset"),
                                    &equalizerwin_load_auto_window,
                                    GTK_SELECTION_SINGLE, nullptr,
                                    audgui_button_new (_("Load"), "document-open", nullptr, nullptr),
                                    G_CALLBACK(equalizerwin_load_auto_ok),
                                    G_CALLBACK(equalizerwin_load_auto_select));
}

void eq_preset_save (void)
{
    if (equalizerwin_save_window) {
        gtk_window_present(GTK_WINDOW(equalizerwin_save_window));
        return;
    }

    equalizerwin_create_list_window(equalizer_presets,
                                    _("Save preset"),
                                    &equalizerwin_save_window,
                                    GTK_SELECTION_SINGLE,
                                    &equalizerwin_save_entry,
                                    audgui_button_new (_("Save"), "document-save", nullptr, nullptr),
                                    G_CALLBACK(equalizerwin_save_ok),
                                    G_CALLBACK(equalizerwin_save_select));
}

void eq_preset_save_auto (void)
{
    if (equalizerwin_save_auto_window)
        gtk_window_present(GTK_WINDOW(equalizerwin_save_auto_window));
    else
        equalizerwin_create_list_window(equalizer_auto_presets,
                                        _("Save auto-preset"),
                                        &equalizerwin_save_auto_window,
                                        GTK_SELECTION_SINGLE,
                                        &equalizerwin_save_auto_entry,
                                        audgui_button_new (_("Save"), "document-save", nullptr, nullptr),
                                        G_CALLBACK(equalizerwin_save_auto_ok),
                                        G_CALLBACK(equalizerwin_save_auto_select));

    String name = aud_drct_get_filename ();

    if (name != nullptr)
    {
        char * base = g_path_get_basename (name);
        gtk_entry_set_text ((GtkEntry *) equalizerwin_save_auto_entry, base);
        g_free (base);
    }
}

void eq_preset_delete (void)
{
    if (equalizerwin_delete_window) {
        gtk_window_present(GTK_WINDOW(equalizerwin_delete_window));
        return;
    }

    equalizerwin_create_list_window(equalizer_presets,
                                    _("Delete preset"),
                                    &equalizerwin_delete_window,
                                    GTK_SELECTION_MULTIPLE, nullptr,
                                    audgui_button_new (_("Delete"), "edit-delete", nullptr, nullptr),
                                    G_CALLBACK(equalizerwin_delete_delete),
                                    nullptr);
}

void eq_preset_delete_auto (void)
{
    if (equalizerwin_delete_auto_window) {
        gtk_window_present(GTK_WINDOW(equalizerwin_delete_auto_window));
        return;
    }

    equalizerwin_create_list_window(equalizer_auto_presets,
                                    _("Delete auto-preset"),
                                    &equalizerwin_delete_auto_window,
                                    GTK_SELECTION_MULTIPLE, nullptr,
                                    audgui_button_new (_("Delete"), "edit-delete", nullptr, nullptr),
                                    G_CALLBACK(equalizerwin_delete_auto_delete),
                                    nullptr);
}

void eq_preset_load_default (void)
{
    if (! equalizerwin_load_preset (equalizer_presets, _("Default")))
        eq_preset_set_zero ();
}

void eq_preset_save_default (void)
{
    equalizerwin_save_preset (equalizer_presets, _("Default"), "eq.preset");
}

void eq_preset_set_zero (void)
{
    equalizerwin_apply_preset (EqualizerPreset ());
}

void eq_preset_list_cleanup (void)
{
    if (equalizerwin_load_window)
        gtk_widget_destroy (equalizerwin_load_window);
    if (equalizerwin_load_auto_window)
        gtk_widget_destroy (equalizerwin_load_auto_window);
    if (equalizerwin_save_window)
        gtk_widget_destroy (equalizerwin_save_window);
    if (equalizerwin_save_auto_window)
        gtk_widget_destroy (equalizerwin_save_auto_window);
    if (equalizerwin_delete_window)
        gtk_widget_destroy (equalizerwin_delete_window);
    if (equalizerwin_delete_auto_window)
        gtk_widget_destroy (equalizerwin_delete_auto_window);
}
