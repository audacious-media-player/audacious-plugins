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
#include <libaudcore/playlist.h>
#include <libaudcore/runtime.h>
#include <libaudcore/tuple.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>
#include <libaudgui/list.h>

#include "gtkui.h"
#include "playlist_util.h"
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
    G_TYPE_STRING,  // filename
    G_TYPE_STRING,  // custom title
    G_TYPE_STRING   // bitrate
};

static const gboolean pw_col_min_widths[PW_COLS] = {
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
    10,  // filename
    10,  // custom title
    3    // bitrate
};

static const gboolean pw_col_label[PW_COLS] = {
    FALSE,  // entry number
    TRUE,   // title
    TRUE,   // artist
    TRUE,   // year
    TRUE,   // album
    TRUE,   // album artist
    FALSE,  // track
    TRUE,   // genre
    FALSE,  // queue position
    FALSE,  // length
    TRUE,   // path
    TRUE,   // filename
    TRUE,   // custom title
    FALSE   // bitrate
};

typedef struct {
    int list;
    Index<int> queue;
    int popup_source, popup_pos;
    gboolean popup_shown;
} PlaylistWidgetData;

static void set_int_from_tuple (GValue * value, const Tuple & tuple, Tuple::Field field)
{
    int i = tuple ? tuple.get_int (field) : 0;
    if (i > 0)
        g_value_take_string (value, g_strdup_printf ("%d", i));
    else
        g_value_set_string (value, "");
}

static void set_string_from_tuple (GValue * value, const Tuple & tuple, Tuple::Field field)
{
    String str = tuple ? tuple.get_str (field) : String ();
    g_value_set_string (value, str);
}

static void set_queued (GValue * value, int list, int row)
{
    int q = aud_playlist_queue_find_entry (list, row);
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
    g_return_if_fail (row >= 0 && row < aud_playlist_entry_count (data->list));

    column = pw_cols[column];

    Tuple tuple;

    switch (column)
    {
    case PW_COL_TITLE:
    case PW_COL_ARTIST:
    case PW_COL_ALBUM:
    case PW_COL_YEAR:
    case PW_COL_ALBUM_ARTIST:
    case PW_COL_TRACK:
    case PW_COL_GENRE:
    case PW_COL_LENGTH:
    case PW_COL_FILENAME:
    case PW_COL_PATH:
    case PW_COL_CUSTOM:
    case PW_COL_BITRATE:
        tuple = aud_playlist_entry_get_tuple (data->list, row, Playlist::Guess);
        break;
    }

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
    }
}

static bool get_selected (void * user, int row)
{
    return aud_playlist_entry_get_selected (((PlaylistWidgetData *) user)->list,
     row);
}

static void set_selected (void * user, int row, bool selected)
{
    aud_playlist_entry_set_selected (((PlaylistWidgetData *) user)->list, row,
     selected);
}

static void select_all (void * user, bool selected)
{
    aud_playlist_select_all (((PlaylistWidgetData *) user)->list, selected);
}

static void focus_change (void * user, int row)
{
    aud_playlist_set_focus (((PlaylistWidgetData *) user)->list, row);
}

static void activate_row (void * user, int row)
{
    int list = ((PlaylistWidgetData *) user)->list;
    aud_playlist_set_position (list, row);
    aud_playlist_play (list);
}

static void right_click (void * user, GdkEventButton * event)
{
    popup_menu_rclick (event->button, event->time);
}

static void shift_rows (void * user, int row, int before)
{
    int list = ((PlaylistWidgetData *) user)->list;

    /* Adjust the shift amount so that the selected entry closest to the
     * destination ends up at the destination. */
    if (before > row)
        before -= playlist_count_selected_in_range (list, row, before - row);
    else
        before += playlist_count_selected_in_range (list, before, row - before);

    aud_playlist_shift (list, row, before - row);
}

static gboolean popup_show (PlaylistWidgetData * data)
{
    audgui_infopopup_show (data->list, data->popup_pos);
    data->popup_shown = TRUE;

    g_source_remove (data->popup_source);
    data->popup_source = 0;
    return FALSE;
}

static void popup_hide (PlaylistWidgetData * data)
{
    if (data->popup_source)
    {
        g_source_remove (data->popup_source);
        data->popup_source = 0;
    }

    if (data->popup_shown)
    {
        audgui_infopopup_hide ();
        data->popup_shown = FALSE;
    }

    data->popup_pos = -1;
}

static void popup_trigger (PlaylistWidgetData * data, int pos)
{
    popup_hide (data);

    data->popup_pos = pos;
    data->popup_source = g_timeout_add (aud_get_int (nullptr, "filepopup_delay") *
     100, (GSourceFunc) popup_show, data);
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
    int playlist = ((PlaylistWidgetData *) user)->list;
    return audgui_urilist_create_from_selected (playlist);
}

static void receive_data (void * user, int row, const char * data, int length)
{
    int playlist = ((PlaylistWidgetData *) user)->list;
    audgui_urilist_insert (playlist, row, str_copy (data, length));
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
    g_return_val_if_fail (path, TRUE);
    int row = gtk_tree_path_get_indices (path)[0];
    g_return_val_if_fail (row >= 0, TRUE);
    gtk_tree_path_free (path);

    Index<String> keys = str_list_to_index (search, " ");

    bool matched = false;

    if (keys.len ())
    {
        int list = ((PlaylistWidgetData *) user)->list;
        Tuple tuple = aud_playlist_entry_get_tuple (list, row);

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

GtkWidget * ui_playlist_widget_new (int playlist)
{
    PlaylistWidgetData * data = new PlaylistWidgetData;
    data->list = playlist;
    data->popup_source = 0;
    data->popup_pos = -1;
    data->popup_shown = FALSE;

    GtkWidget * list = audgui_list_new (& callbacks, data,
     aud_playlist_entry_count (playlist));

    gtk_tree_view_set_headers_visible ((GtkTreeView *) list,
     aud_get_bool ("gtkui", "playlist_headers"));
    gtk_tree_view_set_search_equal_func ((GtkTreeView *) list, search_cb, data,
     nullptr);
    g_signal_connect_swapped (list, "destroy", (GCallback) destroy_cb, data);

    /* Disable type-to-search because it blocks CTRL-V, causing URI's to be
     * pasted into the search box rather than added to the playlist.  The search
     * box can still be brought up with CTRL-F. */
    gtk_tree_view_set_enable_search ((GtkTreeView *) list, FALSE);

    for (int i = 0; i < pw_num_cols; i ++)
    {
        int n = pw_cols[i];
        audgui_list_add_column (list, pw_col_label[n] ? _(pw_col_names[n]) :
         nullptr, i, pw_col_types[n], pw_col_min_widths[n]);
    }

    return list;
}

int ui_playlist_widget_get_playlist (GtkWidget * widget)
{
    PlaylistWidgetData * data = (PlaylistWidgetData *) audgui_list_get_user (widget);
    g_return_val_if_fail (data, -1);
    return data->list;
}

void ui_playlist_widget_set_playlist (GtkWidget * widget, int list)
{
    PlaylistWidgetData * data = (PlaylistWidgetData *) audgui_list_get_user (widget);
    g_return_if_fail (data);
    data->list = list;
}

void ui_playlist_widget_update (GtkWidget * widget, Playlist::Update level, int at, int count)
{
    PlaylistWidgetData * data = (PlaylistWidgetData *) audgui_list_get_user (widget);
    g_return_if_fail (data);

    for (int entry : data->queue)
        audgui_list_update_rows (widget, entry, 1);

    data->queue.remove (0, -1);

    if (level == Playlist::Structure)
    {
        int old_entries = audgui_list_row_count (widget);
        int entries = aud_playlist_entry_count (data->list);

        audgui_list_delete_rows (widget, at, old_entries - (entries - count));
        audgui_list_insert_rows (widget, at, count);

        /* scroll to end of playlist if entries were added there
           (but not if a newly added entry is playing) */
        if (entries > old_entries && at + count == entries &&
         aud_playlist_get_focus (data->list) < old_entries)
            aud_playlist_set_focus (data->list, entries - 1);

        ui_playlist_widget_scroll (widget);
    }
    else if (level == Playlist::Metadata)
        audgui_list_update_rows (widget, at, count);

    audgui_list_update_selection (widget, at, count);
    audgui_list_set_focus (widget, aud_playlist_get_focus (data->list));

    for (int i = aud_playlist_queue_count (data->list); i --; )
    {
        int entry = aud_playlist_queue_get_entry (data->list, i);
        audgui_list_update_rows (widget, entry, 1);
        data->queue.append (entry);
    }
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
    if (row >= 0 && (data->popup_source || data->popup_shown))
        popup_trigger (data, row);
    else
        popup_hide (data);
}
