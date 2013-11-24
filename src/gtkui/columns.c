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
#include <libaudgui/libaudgui-gtk.h>
#include <libaudgui/list.h>

#include "ui_playlist_notebook.h"
#include "ui_playlist_widget.h"

const gchar * const pw_col_names[PW_COLS] = {N_("Entry number"), N_("Title"),
 N_("Artist"), N_("Year"), N_("Album"), N_("Track"), N_("Genre"),
 N_("Queue position"), N_("Length"), N_("File path"), N_("File name"),
 N_("Custom title"), N_("Bitrate")};

gint pw_num_cols;
gint pw_cols[PW_COLS];

static const gchar * const pw_col_keys[PW_COLS] = {"number", "title", "artist",
 "year", "album", "track", "genre", "queued", "length", "path", "filename",
 "custom", "bitrate"};

void pw_col_init (void)
{
    pw_num_cols = 0;

    gchar * columns = aud_get_string ("gtkui", "playlist_columns");
    gchar * * split = g_strsplit (columns, " ", -1);

    for (gchar * * elem = split; * elem && pw_num_cols < PW_COLS; elem ++)
    {
        gint i = 0;
        while (i < PW_COLS && strcmp (* elem, pw_col_keys[i]))
            i ++;

        if (i == PW_COLS)
            break;

        pw_cols[pw_num_cols ++] = i;
    }

    g_strfreev (split);
    g_free (columns);
}

typedef struct {
    gint column;
    gboolean selected;
} Column;

static GtkWidget * window = NULL;
static GtkWidget * chosen_list = NULL, * avail_list = NULL;
static Index * chosen = NULL, * avail = NULL;

static void get_value (void * user, gint row, gint column, GValue * value)
{
    g_return_if_fail (row >= 0 && row < index_count (user));
    Column * c = index_get (user, row);
    g_value_set_string (value, _(pw_col_names[c->column]));
}

static gboolean get_selected (void * user, gint row)
{
    g_return_val_if_fail (row >= 0 && row < index_count (user), FALSE);
    return ((Column *) index_get (user, row))->selected;
}

static void set_selected (void * user, gint row, gboolean selected)
{
    g_return_if_fail (row >= 0 && row < index_count (user));
    ((Column *) index_get (user, row))->selected = selected;
}

static void select_all (void * user, gboolean selected)
{
    gint rows = index_count (user);
    for (gint row = 0; row < rows; row ++)
        ((Column *) index_get (user, row))->selected = selected;
}

static void shift_rows (void * user, gint row, gint before)
{
    gint rows = index_count (user);
    g_return_if_fail (row >= 0 && row < rows);
    g_return_if_fail (before >= 0 && before <= rows);

    if (before == row)
        return;

    Index * move = index_new ();
    Index * others = index_new ();

    gint begin, end;
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

    for (gint i = begin; i < end; i ++)
    {
        Column * c = index_get (user, i);
        index_append (c->selected ? move : others, c);
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

    index_copy_set (move, 0, user, begin, end - begin);
    index_free (move);

    GtkWidget * list = (user == chosen) ? chosen_list : avail_list;
    audgui_list_update_rows (list, begin, end - begin);
    audgui_list_update_selection (list, begin, end - begin);
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

    gint source_rows = index_count (source);
    gint dest_rows = index_count (dest);

    for (gint row = 0; row < source_rows; )
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
        index_append (dest, c);
        audgui_list_insert_rows (dest_list, dest_rows, 1);
        dest_rows ++;
    }
}

static void response_cb (GtkWidget * widget, gint response, void * unused)
{
    if (response == GTK_RESPONSE_ACCEPT)
    {
        gint cols = index_count (chosen);
        g_return_if_fail (cols <= PW_COLS);

        ui_playlist_notebook_empty ();

        for (pw_num_cols = 0; pw_num_cols < cols; pw_num_cols ++)
            pw_cols[pw_num_cols] = ((Column *) index_get (chosen, pw_num_cols))->column;

        aud_set_string ("gtkui", "column_widths", "");
        aud_set_string ("gtkui", "column_expand", "");

        ui_playlist_notebook_populate ();
    }

    gtk_widget_destroy (window);
}

static void destroy_cb (void)
{
    window = NULL;
    chosen_list = NULL;
    avail_list = NULL;

    gint rows = index_count (chosen);
    for (gint row = 0; row < rows; row ++)
        g_slice_free (Column, index_get (chosen, row));
    index_free (chosen);
    chosen = NULL;

    rows = index_count (avail);
    for (gint row = 0; row < rows; row ++)
        g_slice_free (Column, index_get (avail, row));
    index_free (avail);
    avail = NULL;
}

void pw_col_choose (void)
{
    if (window)
    {
        gtk_window_present ((GtkWindow *) window);
        return;
    }

    chosen = index_new ();
    avail = index_new ();

    gboolean added[PW_COLS];
    memset (added, 0, sizeof added);

    for (gint i = 0; i < pw_num_cols; i ++)
    {
        if (added[pw_cols[i]])
            continue;
        added[pw_cols[i]] = TRUE;
        Column * column = g_slice_new (Column);
        column->column = pw_cols[i];
        column->selected = 0;
        index_append (chosen, column);
    }

    for (gint i = 0; i < PW_COLS; i ++)
    {
        if (added[i])
            continue;
        Column * column = g_slice_new (Column);
        column->column = i;
        column->selected = 0;
        index_append (avail, column);
    }

    window = gtk_dialog_new ();
    gtk_window_set_title ((GtkWindow *) window, _("Set Columns"));
    gtk_window_set_default_size ((GtkWindow *) window, 400, 300);
    gtk_dialog_set_default_response ((GtkDialog *) window, GTK_RESPONSE_ACCEPT);

    GtkWidget * cancel_button = audgui_button_new (_("_Cancel"), "window-close", NULL, NULL);
    gtk_dialog_add_action_widget ((GtkDialog *) window, cancel_button, GTK_RESPONSE_CANCEL);

    GtkWidget * apply_button = audgui_button_new (_("_Set"), "system-run", NULL, NULL);
    gtk_dialog_add_action_widget ((GtkDialog *) window, apply_button, GTK_RESPONSE_ACCEPT);

    g_signal_connect (window, "response", (GCallback) response_cb, NULL);
    g_signal_connect (window, "destroy", (GCallback) destroy_cb, NULL);

    GtkWidget * hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start ((GtkBox *) gtk_dialog_get_content_area ((GtkDialog *)
     window), hbox, TRUE, TRUE, 0);

    GtkWidget * vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
    gtk_box_pack_start ((GtkBox *) hbox, vbox, TRUE, TRUE, 0);

    GtkWidget * label = gtk_label_new (_("Available:"));
    g_object_set ((GObject *) label, "xalign", (gfloat) 0, NULL);
    gtk_box_pack_start ((GtkBox *) vbox, label, FALSE, FALSE, 0);

    GtkWidget * scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy ((GtkScrolledWindow *) scroll,
     GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) scroll,
     GTK_SHADOW_IN);
    gtk_box_pack_start ((GtkBox *) vbox, scroll, TRUE, TRUE, 0);

    avail_list = audgui_list_new (& callbacks, avail, index_count (avail));
    gtk_tree_view_set_headers_visible ((GtkTreeView *) avail_list, FALSE);
    audgui_list_add_column (avail_list, NULL, 0, G_TYPE_STRING, -1);
    gtk_container_add ((GtkContainer *) scroll, avail_list);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
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

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
    gtk_box_pack_start ((GtkBox *) hbox, vbox, TRUE, TRUE, 0);

    label = gtk_label_new (_("Chosen:"));
    g_object_set ((GObject *) label, "xalign", (gfloat) 0, NULL);
    gtk_box_pack_start ((GtkBox *) vbox, label, FALSE, FALSE, 0);

    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy ((GtkScrolledWindow *) scroll,
     GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) scroll,
     GTK_SHADOW_IN);
    gtk_box_pack_start ((GtkBox *) vbox, scroll, TRUE, TRUE, 0);

    chosen_list = audgui_list_new (& callbacks, chosen, index_count (chosen));
    gtk_tree_view_set_headers_visible ((GtkTreeView *) chosen_list, FALSE);
    audgui_list_add_column (chosen_list, NULL, 0, G_TYPE_STRING, -1);
    gtk_container_add ((GtkContainer *) scroll, chosen_list);

    gtk_widget_show_all (window);
}

void pw_col_save (void)
{
    GString * s = g_string_new_len (NULL, 0);
    for (gint i = 0; ; )
    {
        g_string_append (s, pw_col_keys[pw_cols[i]]);
        if (++ i < pw_num_cols)
            g_string_append_c (s, ' ');
        else
            break;
    }

    aud_set_string ("gtkui", "playlist_columns", s->str);
    g_string_free (s, TRUE);
}

void pw_col_cleanup (void)
{
    if (window)
        gtk_widget_destroy (window);
}
