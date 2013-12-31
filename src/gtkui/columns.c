/*
 * columns.c
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

#include <string.h>

#include <gtk/gtk.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <libaudcore/index.h>
#include <libaudcore/audstrings.h>
#include <libaudgui/libaudgui-gtk.h>
#include <libaudgui/list.h>

#include "ui_playlist_notebook.h"
#include "ui_playlist_widget.h"

const char * const pw_col_names[PW_COLS] = {N_("Entry number"), N_("Title"),
 N_("Artist"), N_("Year"), N_("Album"), N_("Track"), N_("Genre"),
 N_("Queue position"), N_("Length"), N_("File path"), N_("File name"),
 N_("Custom title"), N_("Bitrate")};

int pw_num_cols;
int pw_cols[PW_COLS];

static const char * const pw_col_keys[PW_COLS] = {"number", "title", "artist",
 "year", "album", "track", "genre", "queued", "length", "path", "filename",
 "custom", "bitrate"};

void pw_col_init (void)
{
    pw_num_cols = 0;

    char * columns = aud_get_str ("gtkui", "playlist_columns");
    Index * index = str_list_to_index (columns, " ");

    int count = index_count (index);
    if (count > PW_COLS)
        count = PW_COLS;

    for (int c = 0; c < count; c ++)
    {
        char * column = index_get (index, c);

        int i = 0;
        while (i < PW_COLS && strcmp (column, pw_col_keys[i]))
            i ++;

        if (i == PW_COLS)
            break;

        pw_cols[pw_num_cols ++] = i;
    }

    index_free_full (index, (IndexFreeFunc) str_unref);
    str_unref (columns);
}

typedef struct {
    int column;
    bool_t selected;
} Column;

static GtkWidget * chosen_list = NULL, * avail_list = NULL;
static Index * chosen = NULL, * avail = NULL;

static void apply_changes (void)
{
    int cols = index_count (chosen);
    g_return_if_fail (cols <= PW_COLS);

    ui_playlist_notebook_empty ();

    for (pw_num_cols = 0; pw_num_cols < cols; pw_num_cols ++)
        pw_cols[pw_num_cols] = ((Column *) index_get (chosen, pw_num_cols))->column;

    aud_set_str ("gtkui", "column_widths", "");
    aud_set_str ("gtkui", "column_expand", "");

    ui_playlist_notebook_populate ();
}

static void get_value (void * user, int row, int column, GValue * value)
{
    g_return_if_fail (row >= 0 && row < index_count (user));
    Column * c = index_get (user, row);
    g_value_set_string (value, _(pw_col_names[c->column]));
}

static bool_t get_selected (void * user, int row)
{
    g_return_val_if_fail (row >= 0 && row < index_count (user), FALSE);
    return ((Column *) index_get (user, row))->selected;
}

static void set_selected (void * user, int row, bool_t selected)
{
    g_return_if_fail (row >= 0 && row < index_count (user));
    ((Column *) index_get (user, row))->selected = selected;
}

static void select_all (void * user, bool_t selected)
{
    int rows = index_count (user);
    for (int row = 0; row < rows; row ++)
        ((Column *) index_get (user, row))->selected = selected;
}

static void shift_rows (void * user, int row, int before)
{
    int rows = index_count (user);
    g_return_if_fail (row >= 0 && row < rows);
    g_return_if_fail (before >= 0 && before <= rows);

    if (before == row)
        return;

    Index * move = index_new ();
    Index * others = index_new ();

    int begin, end;
    if (before < row)
    {
        begin = before;
        end = row + 1;
        while (end < rows && ((Column *) index_get (user, end))->selected)
            end ++;
    }
    else
    {
        begin = row;
        while (begin > 0 && ((Column *) index_get (user, begin - 1))->selected)
            begin --;
        end = before;
    }

    for (int i = begin; i < end; i ++)
    {
        Column * c = index_get (user, i);
        index_insert (c->selected ? move : others, -1, c);
    }

    if (before < row)
    {
        index_copy_insert (others, 0, move, -1, -1);
        index_free (others);
    }
    else
    {
        index_copy_insert (move, 0, others, -1, -1);
        index_free (move);
        move = others;
    }

    index_copy_set (move, 0, user, begin, end - begin);
    index_free (move);

    GtkWidget * list = (user == chosen) ? chosen_list : avail_list;
    audgui_list_update_rows (list, begin, end - begin);
    audgui_list_update_selection (list, begin, end - begin);

    apply_changes ();
}

static const AudguiListCallbacks callbacks = {
 .get_value = get_value,
 .get_selected = get_selected,
 .set_selected = set_selected,
 .select_all = select_all,
 .shift_rows = shift_rows};

static void transfer (Index * source)
{
    Index * dest;
    GtkWidget * source_list, * dest_list;
    if (source == chosen)
    {
        dest = avail;
        source_list = chosen_list;
        dest_list = avail_list;
    }
    else
    {
        dest = chosen;
        source_list = avail_list;
        dest_list = chosen_list;
    }

    int source_rows = index_count (source);
    int dest_rows = index_count (dest);

    for (int row = 0; row < source_rows; )
    {
        Column * c = index_get (source, row);
        if (! c->selected)
        {
            row ++;
            continue;
        }

        index_delete (source, row, 1);
        audgui_list_delete_rows (source_list, row, 1);
        source_rows --;
        index_insert (dest, -1, c);
        audgui_list_insert_rows (dest_list, dest_rows, 1);
        dest_rows ++;
    }

    apply_changes ();
}

static void destroy_cb (void)
{
    chosen_list = NULL;
    avail_list = NULL;

    int rows = index_count (chosen);
    for (int row = 0; row < rows; row ++)
        g_slice_free (Column, index_get (chosen, row));
    index_free (chosen);
    chosen = NULL;

    rows = index_count (avail);
    for (int row = 0; row < rows; row ++)
        g_slice_free (Column, index_get (avail, row));
    index_free (avail);
    avail = NULL;
}

void * pw_col_create_chooser (void)
{
    chosen = index_new ();
    avail = index_new ();

    bool_t added[PW_COLS];
    memset (added, 0, sizeof added);

    for (int i = 0; i < pw_num_cols; i ++)
    {
        if (added[pw_cols[i]])
            continue;
        added[pw_cols[i]] = TRUE;
        Column * column = g_slice_new (Column);
        column->column = pw_cols[i];
        column->selected = 0;
        index_insert (chosen, -1, column);
    }

    for (int i = 0; i < PW_COLS; i ++)
    {
        if (added[i])
            continue;
        Column * column = g_slice_new (Column);
        column->column = i;
        column->selected = 0;
        index_insert (avail, -1, column);
    }

    GtkWidget * hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_size_request (hbox, -1, 160);

    GtkWidget * scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy ((GtkScrolledWindow *) scroll,
     GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) scroll,
     GTK_SHADOW_IN);
    gtk_box_pack_start ((GtkBox *) hbox, scroll, TRUE, TRUE, 0);

    avail_list = audgui_list_new (& callbacks, avail, index_count (avail));
    audgui_list_add_column (avail_list, _("Available columns"), 0, G_TYPE_STRING, -1);
    gtk_container_add ((GtkContainer *) scroll, avail_list);

    GtkWidget * vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start ((GtkBox *) hbox, vbox, FALSE, FALSE, 0);

    GtkWidget * button = gtk_button_new ();
    gtk_container_add ((GtkContainer *) button, gtk_image_new_from_icon_name
     ("go-next", GTK_ICON_SIZE_BUTTON));
    gtk_box_pack_start ((GtkBox *) vbox, button, TRUE, FALSE, 0);
    g_signal_connect_swapped (button, "clicked", (GCallback) transfer, avail);

    button = gtk_button_new ();
    gtk_container_add ((GtkContainer *) button, gtk_image_new_from_icon_name
     ("go-previous", GTK_ICON_SIZE_BUTTON));
    gtk_box_pack_start ((GtkBox *) vbox, button, TRUE, FALSE, 0);
    g_signal_connect_swapped (button, "clicked", (GCallback) transfer, chosen);

    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy ((GtkScrolledWindow *) scroll,
     GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) scroll,
     GTK_SHADOW_IN);
    gtk_box_pack_start ((GtkBox *) hbox, scroll, TRUE, TRUE, 0);

    chosen_list = audgui_list_new (& callbacks, chosen, index_count (chosen));
    audgui_list_add_column (chosen_list, _("Displayed columns"), 0, G_TYPE_STRING, -1);
    gtk_container_add ((GtkContainer *) scroll, chosen_list);

    g_signal_connect (hbox, "destroy", (GCallback) destroy_cb, NULL);

    return hbox;
}

void pw_col_save (void)
{
    Index * index = index_new ();

    for (int i = 0; i < pw_num_cols; i ++)
        index_insert (index, -1, (void *) pw_col_keys[pw_cols[i]]);

    char * columns = index_to_str_list (index, " ");
    aud_set_str ("gtkui", "playlist_columns", columns);
    str_unref (columns);

    index_free (index);
}
