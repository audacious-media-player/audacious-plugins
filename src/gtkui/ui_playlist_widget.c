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

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/playlist.h>
#include <libaudcore/audstrings.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>
#include <libaudgui/list.h>

#include "gtkui.h"
#include "playlist_util.h"
#include "ui_playlist_widget.h"

static const GType pw_col_types[PW_COLS] = {G_TYPE_INT, G_TYPE_STRING,
 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
 G_TYPE_STRING};
static const gboolean pw_col_widths[PW_COLS] = {7, -1, -1, 4, -1, 2, -1, 3, 7,
 -1, -1, -1, 3};
static const gboolean pw_col_label[PW_COLS] = {FALSE, TRUE, TRUE, TRUE, TRUE,
 FALSE, TRUE, FALSE, FALSE, TRUE, TRUE, TRUE, FALSE};

typedef struct {
    gint list;
    GList * queue;
    gint popup_source, popup_pos;
    gboolean popup_shown;
} PlaylistWidgetData;

static void set_int_from_tuple (GValue * value, const Tuple * tuple, gint field)
{
    gint i = tuple ? tuple_get_int (tuple, field) : 0;
    if (i > 0)
        g_value_take_string (value, g_strdup_printf ("%d", i));
    else
        g_value_set_string (value, "");
}

static void set_string_from_tuple (GValue * value, const Tuple * tuple,
 gint field)
{
    gchar *str = tuple ? tuple_get_str (tuple, field) : NULL;
    g_value_set_string (value, str);
    str_unref(str);
}

static void set_queued (GValue * value, gint list, gint row)
{
    int q = aud_playlist_queue_find_entry (list, row);
    if (q < 0)
        g_value_set_string (value, "");
    else
        g_value_take_string (value, g_strdup_printf ("#%d", 1 + q));
}

static void set_length (GValue * value, gint list, gint row)
{
    gint len = aud_playlist_entry_get_length (list, row, TRUE);
    if (len)
    {
        len /= 1000;

        gchar s[16];
        if (len < 3600)
            snprintf (s, sizeof s, aud_get_bool (NULL, "leading_zero") ?
             "%02d:%02d" : "%d:%02d", len / 60, len % 60);
        else
            snprintf (s, sizeof s, "%d:%02d:%02d", len / 3600, (len / 60) % 60,
             len % 60);

        g_value_set_string (value, s);
    }
    else
        g_value_set_string (value, "");
}

static void get_value (void * user, gint row, gint column, GValue * value)
{
    PlaylistWidgetData * data = user;
    g_return_if_fail (column >= 0 && column < pw_num_cols);
    g_return_if_fail (row >= 0 && row < aud_playlist_entry_count (data->list));

    column = pw_cols[column];

    gchar * title = NULL, * artist = NULL, * album = NULL;
    Tuple * tuple = NULL;

    if (column == PW_COL_TITLE || column == PW_COL_ARTIST || column ==
     PW_COL_ALBUM)
        aud_playlist_entry_describe (data->list, row, & title, & artist,
         & album, TRUE);
    else if (column == PW_COL_YEAR || column == PW_COL_TRACK || column ==
     PW_COL_GENRE || column == PW_COL_FILENAME || column == PW_COL_PATH ||
     column == PW_COL_BITRATE)
        tuple = aud_playlist_entry_get_tuple (data->list, row, TRUE);

    switch (column)
    {
    case PW_COL_NUMBER:
        g_value_set_int (value, 1 + row);
        break;
    case PW_COL_TITLE:
        g_value_set_string (value, title);
        break;
    case PW_COL_ARTIST:
        g_value_set_string (value, artist);
        break;
    case PW_COL_YEAR:
        set_int_from_tuple (value, tuple, FIELD_YEAR);
        break;
    case PW_COL_ALBUM:
        g_value_set_string (value, album);
        break;
    case PW_COL_TRACK:
        set_int_from_tuple (value, tuple, FIELD_TRACK_NUMBER);
        break;
    case PW_COL_GENRE:
        set_string_from_tuple (value, tuple, FIELD_GENRE);
        break;
    case PW_COL_QUEUED:
        set_queued (value, data->list, row);
        break;
    case PW_COL_LENGTH:
        set_length (value, data->list, row);
        break;
    case PW_COL_FILENAME:
        set_string_from_tuple (value, tuple, FIELD_FILE_NAME);
        break;
    case PW_COL_PATH:
        set_string_from_tuple (value, tuple, FIELD_FILE_PATH);
        break;
    case PW_COL_CUSTOM:;
        gchar * custom = aud_playlist_entry_get_title (data->list, row, TRUE);
        g_value_set_string (value, custom);
        str_unref (custom);
        break;
    case PW_COL_BITRATE:
        set_int_from_tuple (value, tuple, FIELD_BITRATE);
        break;
    }

    str_unref (title);
    str_unref (artist);
    str_unref (album);
    if (tuple)
        tuple_unref (tuple);
}

static gboolean get_selected (void * user, gint row)
{
    return aud_playlist_entry_get_selected (((PlaylistWidgetData *) user)->list,
     row);
}

static void set_selected (void * user, gint row, gboolean selected)
{
    aud_playlist_entry_set_selected (((PlaylistWidgetData *) user)->list, row,
     selected);
}

static void select_all (void * user, gboolean selected)
{
    aud_playlist_select_all (((PlaylistWidgetData *) user)->list, selected);
}

static void focus_change (void * user, gint row)
{
    aud_playlist_set_focus (((PlaylistWidgetData *) user)->list, row);
}

static void activate_row (void * user, gint row)
{
    gint list = ((PlaylistWidgetData *) user)->list;
    aud_playlist_set_position (list, row);
    aud_drct_play_playlist (list);
}

static void right_click (void * user, GdkEventButton * event)
{
    popup_menu_rclick (event->button, event->time);
}

static void shift_rows (void * user, gint row, gint before)
{
    gint list = ((PlaylistWidgetData *) user)->list;

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

static void popup_trigger (PlaylistWidgetData * data, gint pos)
{
    popup_hide (data);

    data->popup_pos = pos;
    data->popup_source = g_timeout_add (aud_get_int (NULL, "filepopup_delay") *
     100, (GSourceFunc) popup_show, data);
}

static void mouse_motion (void * user, GdkEventMotion * event, gint row)
{
    PlaylistWidgetData * data = (PlaylistWidgetData *) user;

    if (row < 0)
    {
        popup_hide (data);
        return;
    }

    if (aud_get_bool (NULL, "show_filepopup_for_tuple") && data->popup_pos != row)
        popup_trigger (data, row);
}

static void mouse_leave (void * user, GdkEventMotion * event, gint row)
{
    popup_hide ((PlaylistWidgetData *) user);
}

static void get_data (void * user, void * * data, gint * length)
{
    gchar * text = audgui_urilist_create_from_selected
     (((PlaylistWidgetData *) user)->list);
    g_return_if_fail (text);
    * data = text;
    * length = strlen (text);
}

static void receive_data (void * user, gint row, const void * data, gint length)
{
    gchar * text = g_malloc (length + 1);
    memcpy (text, data, length);
    text[length] = 0;
    audgui_urilist_insert (((PlaylistWidgetData *) user)->list, row, text);
    g_free (text);
}

static const AudguiListCallbacks callbacks = {
 .get_value = get_value,
 .get_selected = get_selected,
 .set_selected = set_selected,
 .select_all = select_all,
 .focus_change = focus_change,
 .activate_row = activate_row,
 .right_click = right_click,
 .shift_rows = shift_rows,
 .mouse_motion = mouse_motion,
 .mouse_leave = mouse_leave,
 .data_type = "text/uri-list",
 .get_data = get_data,
 .receive_data = receive_data};

static gboolean search_cb (GtkTreeModel * model, gint column, const gchar * key,
 GtkTreeIter * iter, void * user)
{
    GtkTreePath * path = gtk_tree_model_get_path (model, iter);
    g_return_val_if_fail (path, TRUE);
    gint row = gtk_tree_path_get_indices (path)[0];
    g_return_val_if_fail (row >= 0, TRUE);
    gchar * s[3] = {NULL, NULL, NULL};
    aud_playlist_entry_describe (((PlaylistWidgetData *) user)->list, row,
     & s[0], & s[1], & s[2], FALSE);
    gtk_tree_path_free (path);

    gchar * temp = g_utf8_strdown (key, -1);
    gchar * * keys = g_strsplit (temp, " ", 0);
    g_free (temp);

    gint remain = 0; /* number of keys remaining to be matched */
    for (gint j = 0; keys[j]; j ++)
    {
        if (keys[j][0])
            remain ++;
    }
    if (! remain)
        remain ++; /* force non-match if there are no non-blank keys */

    for (gint i = 0; i < G_N_ELEMENTS (s); i ++)
    {
        if (! s[i])
            continue;

        if (remain)
        {
            temp = g_utf8_strdown (s[i], -1);

            for (gint j = 0; keys[j] && remain; j ++)
            {
                if (keys[j][0] && strstr (temp, keys[j]))
                {
                    keys[j][0] = 0; /* don't look for this one again */
                    remain --;
                }
            }

            g_free (temp);
        }

        str_unref (s[i]);
    }

    g_strfreev (keys);
    return remain ? TRUE : FALSE; /* TRUE == not matched, FALSE == matched */
}

static void destroy_cb (PlaylistWidgetData * data)
{
    g_list_free (data->queue);
    g_free (data);
}

GtkWidget * ui_playlist_widget_new (gint playlist)
{
    PlaylistWidgetData * data = g_malloc0 (sizeof (PlaylistWidgetData));
    data->list = playlist;
    data->queue = NULL;
    data->popup_source = 0;
    data->popup_pos = -1;
    data->popup_shown = FALSE;

    GtkWidget * list = audgui_list_new (& callbacks, data,
     aud_playlist_entry_count (playlist));

    gtk_tree_view_set_headers_visible ((GtkTreeView *) list,
     aud_get_bool ("gtkui", "playlist_headers"));
    gtk_tree_view_set_search_equal_func ((GtkTreeView *) list, search_cb, data,
     NULL);
    g_signal_connect_swapped (list, "destroy", (GCallback) destroy_cb, data);

    /* Disable type-to-search because it blocks CTRL-V, causing URI's to be
     * pasted into the search box rather than added to the playlist.  The search
     * box can still be brought up with CTRL-F. */
    gtk_tree_view_set_enable_search ((GtkTreeView *) list, FALSE);

    for (gint i = 0; i < pw_num_cols; i ++)
    {
        gint n = pw_cols[i];
        audgui_list_add_column (list, pw_col_label[n] ? _(pw_col_names[n]) :
         NULL, i, pw_col_types[n], pw_col_widths[n]);
    }

    return list;
}

gint ui_playlist_widget_get_playlist (GtkWidget * widget)
{
    PlaylistWidgetData * data = audgui_list_get_user (widget);
    g_return_val_if_fail (data, -1);
    return data->list;
}

void ui_playlist_widget_set_playlist (GtkWidget * widget, gint list)
{
    PlaylistWidgetData * data = audgui_list_get_user (widget);
    g_return_if_fail (data);
    data->list = list;
}

static void update_queue (GtkWidget * widget, PlaylistWidgetData * data)
{
    for (GList * node = data->queue; node; node = node->next)
        audgui_list_update_rows (widget, GPOINTER_TO_INT (node->data), 1);

    g_list_free (data->queue);
    data->queue = NULL;

    for (gint i = aud_playlist_queue_count (data->list); i --; )
        data->queue = g_list_prepend (data->queue, GINT_TO_POINTER
         (aud_playlist_queue_get_entry (data->list, i)));

    for (GList * node = data->queue; node; node = node->next)
        audgui_list_update_rows (widget, GPOINTER_TO_INT (node->data), 1);
}

void ui_playlist_widget_update (GtkWidget * widget, gint type, gint at,
 gint count)
{
    PlaylistWidgetData * data = audgui_list_get_user (widget);
    g_return_if_fail (data);

    if (type == PLAYLIST_UPDATE_STRUCTURE)
    {
        gint old_entries = audgui_list_row_count (widget);
        gint entries = aud_playlist_entry_count (data->list);

        audgui_list_delete_rows (widget, at, old_entries - (entries - count));
        audgui_list_insert_rows (widget, at, count);

        /* scroll to end of playlist if entries were added there
           (but not if a newly added entry is playing) */
        if (entries > old_entries && at + count == entries &&
         aud_playlist_get_focus (data->list) < old_entries)
            aud_playlist_set_focus (data->list, entries - 1);

        ui_playlist_widget_scroll (widget);
    }
    else if (type == PLAYLIST_UPDATE_METADATA)
        audgui_list_update_rows (widget, at, count);

    audgui_list_update_selection (widget, at, count);
    audgui_list_set_focus (widget, aud_playlist_get_focus (data->list));
    update_queue (widget, data);
}

void ui_playlist_widget_scroll (GtkWidget * widget)
{
    PlaylistWidgetData * data = audgui_list_get_user (widget);
    g_return_if_fail (data);

    gint row = -1;

    if (gtk_widget_get_realized (widget))
    {
        gint x, y;
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

void ui_playlist_widget_get_column_widths (GtkWidget * widget, char * * widths,
 char * * expand)
{
    int w[pw_num_cols], ex[pw_num_cols];

    for (int i = 0; i < pw_num_cols; i ++)
    {
        GtkTreeViewColumn * col = gtk_tree_view_get_column ((GtkTreeView *) widget, i);
        w[i] = gtk_tree_view_column_get_fixed_width (col);
        ex[i] = gtk_tree_view_column_get_expand (col);
    }

    * widths = int_array_to_str (w, pw_num_cols);
    * expand = int_array_to_str (ex, pw_num_cols);
}

void ui_playlist_widget_set_column_widths (GtkWidget * widget,
 const char * widths, const char * expand)
{
    int w[pw_num_cols], ex[pw_num_cols];

    if (! str_to_int_array (widths, w, pw_num_cols) ||
     ! str_to_int_array (expand, ex, pw_num_cols))
        return;

    for (int i = 0; i < pw_num_cols; i ++)
    {
        GtkTreeViewColumn * col = gtk_tree_view_get_column ((GtkTreeView *) widget, i);
        gtk_tree_view_column_set_fixed_width (col, w[i]);
        gtk_tree_view_column_set_expand (col, ex[i]);
     }
}
