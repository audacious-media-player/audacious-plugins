/*
 * Playlist Manager Plugin for Audacious
 * Copyright 2006-2014 Giacomo Lozito, John Lindgren, and Thomas Lange
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

#define AUD_PLUGIN_GLIB_ONLY
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>
#include <libaudgui/list.h>

#include <string.h>

class PlaylistManager : public GeneralPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Playlist Manager"),
        PACKAGE
    };

    constexpr PlaylistManager () : GeneralPlugin (info, false) {}

    void * get_gtk_widget ();
    int take_message (const char * code, const void * data, int size);
};

EXPORT PlaylistManager aud_plugin_instance;

static GtkWidget * focus_widget;

static void activate_row (void * user, int row);

static void rename_cb (void * unused)
{
    audgui_show_playlist_rename (aud_playlist_get_active ());
}

static void new_cb (void * unused)
{
    aud_playlist_insert (aud_playlist_get_active () + 1);
    aud_playlist_set_active (aud_playlist_get_active () + 1);
}

static void delete_cb (void * unused)
{
    audgui_confirm_playlist_delete (aud_playlist_get_active ());
}

static void get_value (void * user, int row, int column, GValue * value)
{
    switch (column)
    {
    case 0:
    {
        g_value_set_string (value, aud_playlist_get_title (row));
        break;
    }
    case 1:
        g_value_set_int (value, aud_playlist_entry_count (row));
        break;
    }
}

static bool get_selected (void * user, int row)
{
    return (row == aud_playlist_get_active ());
}

static void set_selected (void * user, int row, bool selected)
{
    if (selected)
        aud_playlist_set_active (row);
}

static void select_all (void * user, bool selected)
{
}

static void activate_row (void * user, int row)
{
    aud_playlist_set_active (row);
    aud_playlist_play (row);
}

static void shift_rows (void * user, int row, int before)
{
    if (before < row)
        aud_playlist_reorder (row, before, 1);
    else if (before - 1 > row)
        aud_playlist_reorder (row, before - 1, 1);
}

static const AudguiListCallbacks callbacks = {
    get_value,
    get_selected,
    set_selected,
    select_all,
    activate_row,
    nullptr,  // right_click
    shift_rows
};

static gboolean search_cb (GtkTreeModel * model, int column, const char * key,
 GtkTreeIter * iter, void * user)
{
    GtkTreePath * path = gtk_tree_model_get_path (model, iter);
    g_return_val_if_fail (path, true);
    int row = gtk_tree_path_get_indices (path)[0];
    gtk_tree_path_free (path);

    String title = aud_playlist_get_title (row);
    g_return_val_if_fail (title, true);

    Index<String> keys = str_list_to_index (key, " ");

    if (! keys.len ())
        return true;  /* not matched */

    for (const String & key : keys)
    {
        if (! strstr_nocase_utf8 (title, key))
            return true;  /* not matched */
    }

    return false;  /* matched */
}

static gboolean position_changed = false;
static gboolean playlist_activated = false;

static void update_hook (void * data, void * list_)
{
    auto level = aud::from_ptr<Playlist::UpdateLevel> (data);
    GtkWidget * list = (GtkWidget *) list_;
    int rows = aud_playlist_count ();

    if (level == Playlist::Structure)
    {
        int old_rows = audgui_list_row_count (list);

        if (rows < old_rows)
            audgui_list_delete_rows (list, rows, old_rows - rows);
        else if (rows > old_rows)
            audgui_list_insert_rows (list, old_rows, rows - old_rows);

        position_changed = true;
        playlist_activated = true;
    }

    if (level >= Playlist::Metadata)
        audgui_list_update_rows (list, 0, rows);

    if (playlist_activated)
    {
        audgui_list_set_focus (list, aud_playlist_get_active ());
        audgui_list_update_selection (list, 0, rows);
        playlist_activated = false;
    }

    if (position_changed)
    {
        audgui_list_set_highlight (list, aud_playlist_get_playing ());
        position_changed = false;
    }
}

static void activate_hook (void * data, void * list_)
{
    GtkWidget * list = (GtkWidget *) list_;

    if (aud_playlist_update_pending ())
        playlist_activated = true;
    else
    {
        audgui_list_set_focus (list, aud_playlist_get_active ());
        audgui_list_update_selection (list, 0, aud_playlist_count ());
    }
}

static void position_hook (void * data, void * list_)
{
    GtkWidget * list = (GtkWidget *) list_;

    if (aud_playlist_update_pending ())
        position_changed = true;
    else
        audgui_list_set_highlight (list, aud_playlist_get_playing ());
}

static void destroy_cb (GtkWidget * window)
{
    hook_dissociate ("playlist update", update_hook);
    hook_dissociate ("playlist activate", activate_hook);
    hook_dissociate ("playlist set playing", position_hook);

    focus_widget = nullptr;
}

void * PlaylistManager::get_gtk_widget ()
{
    GtkWidget * playman_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

    /* ListView */
    GtkWidget * playman_pl_lv = audgui_list_new (& callbacks, nullptr, aud_playlist_count ());
    audgui_list_add_column (playman_pl_lv, _("Title"), 0, G_TYPE_STRING, -1);
    audgui_list_add_column (playman_pl_lv, _("Entries"), 1, G_TYPE_INT, 7);
    audgui_list_set_highlight (playman_pl_lv, aud_playlist_get_playing ());
    gtk_tree_view_set_search_equal_func ((GtkTreeView *) playman_pl_lv,
     search_cb, nullptr, nullptr);
    hook_associate ("playlist update", update_hook, playman_pl_lv);
    hook_associate ("playlist activate", activate_hook, playman_pl_lv);
    hook_associate ("playlist set playing", position_hook, playman_pl_lv);

    GtkWidget * playman_pl_lv_sw = gtk_scrolled_window_new (nullptr, nullptr);
    gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) playman_pl_lv_sw,
     GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy ((GtkScrolledWindow *) playman_pl_lv_sw,
     GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add ((GtkContainer *) playman_pl_lv_sw, playman_pl_lv);
    gtk_box_pack_start ((GtkBox *) playman_vbox, playman_pl_lv_sw, true, true, 0);

    /* ButtonBox */
    GtkWidget * playman_button_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget * new_button = audgui_button_new (_("_New"), "document-new", new_cb, nullptr);
    GtkWidget * delete_button = audgui_button_new (_("_Remove"), "edit-delete", delete_cb, nullptr);
    GtkWidget * rename_button = audgui_button_new (_("Ren_ame"), "insert-text", rename_cb, nullptr);

    gtk_box_pack_start ((GtkBox *) playman_button_hbox, new_button, false, false, 0);
    gtk_box_pack_start ((GtkBox *) playman_button_hbox, delete_button, false, false, 0);
    gtk_box_pack_end ((GtkBox *) playman_button_hbox, rename_button, false, false, 0);
    gtk_box_pack_start ((GtkBox *) playman_vbox, playman_button_hbox, false, false, 0);

    focus_widget = playman_pl_lv;

    g_signal_connect (playman_vbox, "destroy", (GCallback) destroy_cb, nullptr);

    return playman_vbox;
}

int PlaylistManager::take_message (const char * code, const void * data, int size)
{
    if (! strcmp (code, "grab focus"))
    {
        if (focus_widget)
            gtk_widget_grab_focus (focus_widget);

        return 0;
    }

    return -1;
}
