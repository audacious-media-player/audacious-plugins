/*
 * ui_playlist_widget.c
 * Copyright 2011-2012 John Lindgren, William Pitcock, and Michał Lipski
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
#include <libaudcore/i18n.h>
#include <libaudcore/mainloop.h>
#include <libaudcore/playlist.h>
#include <libaudcore/runtime.h>
#include <libaudcore/tuple.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>
#include <libaudgui/list.h>

#include "gtkui.h"
#include "ui_playlist_widget.h"

static const GType pw_col_types[PW_COLS] =
{
    G_TYPE_INT,     // entry number
    G_TYPE_STRING,  // title
    G_TYPE_STRING,  // artist
    G_TYPE_STRING,  // year
    G_TYPE_STRING,  // album
    G_TYPE_STRING,  // album artist
    G_TYPE_STRING,  // track
    G_TYPE_STRING,  // genre
    G_TYPE_STRING,  // queue position
    G_TYPE_STRING,  // length
    G_TYPE_STRING,  // path
    G_TYPE_STRING,  // file name
    G_TYPE_STRING,  // custom title
    G_TYPE_STRING,  // bitrate
    G_TYPE_STRING   // comment
};

static const int pw_col_min_widths[PW_COLS] = {
    7,   // entry number
    10,  // title
    10,  // artist
    4,   // year
    10,  // album
    10,  // album artist
    2,   // track
    10,  // genre
    3,   // queue position
    7,   // length
    10,  // path
    10,  // file name
    10,  // custom title
    3,   // bitrate
    10   // comment
};

static const bool pw_col_label[PW_COLS] = {
    false,  // entry number
    true,   // title
    true,   // artist
    true,   // year
    true,   // album
    true,   // album artist
    false,  // track
    true,   // genre
    false,  // queue position
    false,  // length
    true,   // path
    true,   // file name
    true,   // custom title
    false,  // bitrate
    true    // comment
};

struct PlaylistWidgetData
{
    Playlist list;
    int popup_pos = -1;
    QueuedFunc popup_timer;

    void show_popup ()
        { audgui_infopopup_show (list, popup_pos); }
};

static void set_int_from_tuple (GValue * value, const Tuple & tuple, Tuple::Field field)
{
    int i = tuple.get_int (field);
    if (i > 0)
        g_value_take_string (value, g_strdup_printf ("%d", i));
    else
        g_value_set_string (value, "");
}

static void set_string_from_tuple (GValue * value, const Tuple & tuple, Tuple::Field field)
{
    g_value_set_string (value, tuple.get_str (field));
}

static void set_queued (GValue * value, Playlist list, int row)
{
    int q = list.queue_find_entry (row);
    if (q < 0)
        g_value_set_string (value, "");
    else
        g_value_take_string (value, g_strdup_printf ("#%d", 1 + q));
}

static void set_length (GValue * value, const Tuple & tuple)
{
    int len = tuple.get_int (Tuple::Length);
    if (len >= 0)
        g_value_set_string (value, str_format_time (len));
    else
        g_value_set_string (value, "");
}

static void get_value (void * user, int row, int column, GValue * value)
{
    PlaylistWidgetData * data = (PlaylistWidgetData *) user;
    g_return_if_fail (column >= 0 && column < pw_num_cols);
    g_return_if_fail (row >= 0 && row < data->list.n_entries ());

    column = pw_cols[column];

    Tuple tuple;

    if (column != PW_COL_NUMBER && column != PW_COL_QUEUED)
        tuple = data->list.entry_tuple (row, Playlist::NoWait);

    switch (column)
    {
    case PW_COL_NUMBER:
        g_value_set_int (value, 1 + row);
        break;
    case PW_COL_TITLE:
        set_string_from_tuple (value, tuple, Tuple::Title);
        break;
    case PW_COL_ARTIST:
        set_string_from_tuple (value, tuple, Tuple::Artist);
        break;
    case PW_COL_YEAR:
        set_int_from_tuple (value, tuple, Tuple::Year);
        break;
    case PW_COL_ALBUM:
        set_string_from_tuple (value, tuple, Tuple::Album);
        break;
    case PW_COL_ALBUM_ARTIST:
        set_string_from_tuple (value, tuple, Tuple::AlbumArtist);
        break;
    case PW_COL_TRACK:
        set_int_from_tuple (value, tuple, Tuple::Track);
        break;
    case PW_COL_GENRE:
        set_string_from_tuple (value, tuple, Tuple::Genre);
        break;
    case PW_COL_QUEUED:
        set_queued (value, data->list, row);
        break;
    case PW_COL_LENGTH:
        set_length (value, tuple);
        break;
    case PW_COL_FILENAME:
        set_string_from_tuple (value, tuple, Tuple::Basename);
        break;
    case PW_COL_PATH:
        set_string_from_tuple (value, tuple, Tuple::Path);
        break;
    case PW_COL_CUSTOM:
        set_string_from_tuple (value, tuple, Tuple::FormattedTitle);
        break;
    case PW_COL_BITRATE:
        set_int_from_tuple (value, tuple, Tuple::Bitrate);
        break;
    case PW_COL_COMMENT:
        set_string_from_tuple (value, tuple, Tuple::Comment);
        break;
    }
}

static bool get_selected (void * user, int row)
{
    return ((PlaylistWidgetData *) user)->list.entry_selected (row);
}

static void set_selected (void * user, int row, bool selected)
{
    ((PlaylistWidgetData *) user)->list.select_entry (row, selected);
}

static void select_all (void * user, bool selected)
{
    ((PlaylistWidgetData *) user)->list.select_all (selected);
}

static void focus_change (void * user, int row)
{
    ((PlaylistWidgetData *) user)->list.set_focus (row);
}

static void activate_row (void * user, int row)
{
    auto list = ((PlaylistWidgetData *) user)->list;
    list.set_position (row);
    list.start_playback ();
}

static void right_click (void * user, GdkEventButton * event)
{
    popup_menu_rclick (event->button, event->time);
}

static void shift_rows (void * user, int row, int before)
{
    auto list = ((PlaylistWidgetData *) user)->list;

    /* Adjust the shift amount so that the selected entry closest to the
     * destination ends up at the destination. */
    if (before > row)
        before -= list.n_selected (row, before - row);
    else
        before += list.n_selected (before, row - before);

    list.shift_entries (row, before - row);
}

static void popup_hide (PlaylistWidgetData * data)
{
    audgui_infopopup_hide ();

    data->popup_pos = -1;
    data->popup_timer.stop ();
}

static void popup_trigger (PlaylistWidgetData * data, int pos)
{
    audgui_infopopup_hide ();

    data->popup_pos = pos;
    data->popup_timer.queue (aud_get_int (nullptr, "filepopup_delay") * 100,
     aud::obj_member<PlaylistWidgetData, & PlaylistWidgetData::show_popup>, data);
}

static void mouse_motion (void * user, GdkEventMotion * event, int row)
{
    PlaylistWidgetData * data = (PlaylistWidgetData *) user;

    if (row < 0)
    {
        popup_hide (data);
        return;
    }

    if (aud_get_bool (nullptr, "show_filepopup_for_tuple") && data->popup_pos != row)
        popup_trigger (data, row);
}

static void mouse_leave (void * user, GdkEventMotion * event, int row)
{
    popup_hide ((PlaylistWidgetData *) user);
}

static Index<char> get_data (void * user)
{
    auto playlist = ((PlaylistWidgetData *) user)->list;
    return audgui_urilist_create_from_selected (playlist);
}

// length is ignored; GtkSelectionData null-terminates the data for us
static void receive_data (void * user, int row, const char * data, int /*length*/)
{
    auto playlist = ((PlaylistWidgetData *) user)->list;
    audgui_urilist_insert (playlist, row, data);
}

static const AudguiListCallbacks callbacks = {
    get_value,
    get_selected,
    set_selected,
    select_all,
    activate_row,
    right_click,
    shift_rows,
    "text/uri-list",
    get_data,
    receive_data,
    mouse_motion,
    mouse_leave,
    focus_change
};

static gboolean search_cb (GtkTreeModel * model, int column, const char * search,
 GtkTreeIter * iter, void * user)
{
    GtkTreePath * path = gtk_tree_model_get_path (model, iter);
    g_return_val_if_fail (path, true);
    int row = gtk_tree_path_get_indices (path)[0];
    g_return_val_if_fail (row >= 0, true);
    gtk_tree_path_free (path);

    Index<String> keys = str_list_to_index (search, " ");

    bool matched = false;

    if (keys.len ())
    {
        auto list = ((PlaylistWidgetData *) user)->list;
        Tuple tuple = list.entry_tuple (row);

        String strings[3] = {
            tuple.get_str (Tuple::Title),
            tuple.get_str (Tuple::Artist),
            tuple.get_str (Tuple::Album)
        };

        for (const String & s : strings)
        {
            if (! s)
                continue;

            auto is_match = [&] (const String & key)
                { return (bool) strstr_nocase_utf8 (s, key); };

            keys.remove_if (is_match);
        }

        matched = ! keys.len ();
    }

    return ! matched;
}

static void destroy_cb (PlaylistWidgetData * data)
{
    delete data;
}

GtkWidget * ui_playlist_widget_new (Playlist playlist)
{
    PlaylistWidgetData * data = new PlaylistWidgetData;
    data->list = playlist;

    GtkWidget * list = audgui_list_new (& callbacks, data,
     playlist.n_entries ());

    gtk_tree_view_set_headers_visible ((GtkTreeView *) list,
     aud_get_bool ("gtkui", "playlist_headers"));
    gtk_tree_view_set_search_equal_func ((GtkTreeView *) list, search_cb, data,
     nullptr);
    g_signal_connect_swapped (list, "destroy", (GCallback) destroy_cb, data);

    /* Disable type-to-search because it blocks CTRL-V, causing URI's to be
     * pasted into the search box rather than added to the playlist.  The search
     * box can still be brought up with CTRL-F. */
    gtk_tree_view_set_enable_search ((GtkTreeView *) list, false);

    for (int i = 0; i < pw_num_cols; i ++)
    {
        int n = pw_cols[i];
        audgui_list_add_column (list, pw_col_label[n] ? _(pw_col_names[n]) :
         nullptr, i, pw_col_types[n], pw_col_min_widths[n]);
    }

    return list;
}

void ui_playlist_widget_update (GtkWidget * widget)
{
    PlaylistWidgetData * data = (PlaylistWidgetData *) audgui_list_get_user (widget);
    g_return_if_fail (data);

    auto update = data->list.update_detail ();

    if (update.level == Playlist::NoUpdate)
        return;

    int entries = data->list.n_entries ();
    int changed = entries - update.before - update.after;

    if (update.level == Playlist::Structure)
    {
        int old_entries = audgui_list_row_count (widget);
        int removed = old_entries - update.before - update.after;

        audgui_list_delete_rows (widget, update.before, removed);
        audgui_list_insert_rows (widget, update.before, changed);

        /* scroll to end of playlist if entries were added there
           (but not if a newly added entry is playing) */
        if (entries > old_entries && ! update.after && data->list.get_focus () < old_entries)
            data->list.set_focus (entries - 1);

        ui_playlist_widget_scroll (widget);
    }
    else if (update.level == Playlist::Metadata || update.queue_changed)
        audgui_list_update_rows (widget, update.before, changed);

    if (update.queue_changed)
    {
        for (int i = data->list.n_queued (); i --; )
        {
            int entry = data->list.queue_get_entry (i);
            if (entry < update.before || entry >= entries - update.after)
                audgui_list_update_rows (widget, entry, 1);
        }
    }

    audgui_list_update_selection (widget, update.before, changed);
    audgui_list_set_highlight (widget, data->list.get_position ());
    audgui_list_set_focus (widget, data->list.get_focus ());
}

void ui_playlist_widget_scroll (GtkWidget * widget)
{
    PlaylistWidgetData * data = (PlaylistWidgetData *) audgui_list_get_user (widget);
    g_return_if_fail (data);

    int row = -1;

    if (gtk_widget_get_realized (widget))
    {
        int x, y;
        audgui_get_mouse_coords (widget, & x, & y);
        row = audgui_list_row_at_point (widget, x, y);
    }

    /* Only update the info popup if it is already shown or about to be shown;
     * this makes sure that it doesn't pop up when the Audacious window isn't
     * even visible. */
    if (row >= 0 && data->popup_pos >= 0)
        popup_trigger (data, row);
    else
        popup_hide (data);
}
