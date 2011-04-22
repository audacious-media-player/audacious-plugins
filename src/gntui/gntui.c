/*  Audacious GNT interface
 *  Copyright (C) 2009 Tomasz Moń <desowin@gmail.com>
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
 */

#include <glib/gi18n.h>
#include <glib.h>
#include <audacious/plugin.h>
#include <gnt/gnt.h>
#include <gnt/gntbox.h>
#include <gnt/gntwindow.h>
#include <gnt/gntlabel.h>
#include <gnt/gntbutton.h>
#include "fileselector.h"

static GntWidget *window = NULL;
static GntWidget *playback_hbox = NULL;
static GntWidget *status_widget = NULL;
static GntWidget *time_widget = NULL;
static GntWidget *title_widget = NULL;
static GntWidget *playlist_widget = NULL;

static gint update_song_timeout_source = 0;

static gchar *playing_status()
{
    if (aud_drct_get_playing())
    {
        if (!aud_drct_get_paused())
        {
            return ("|>");
        }
        else
        {
            return ("||");
        }
    }
    else
    {
        return ("[]");
    }
}

static void fill_playlist()
{
    gint playlist = aud_playlist_get_active();
    gint pos = aud_playlist_get_position(playlist);
    gint i, count;

    count = aud_playlist_entry_count(playlist);

    gnt_tree_remove_all(GNT_TREE(playlist_widget));

    g_object_set_data(G_OBJECT(playlist_widget), "playlist", GINT_TO_POINTER(playlist));
    for (i = 0; i < count; i++) {
        GntTreeRow * row;
        gint length = aud_playlist_entry_get_length(playlist, i) / 1000;
        gchar * len = g_strdup_printf("%02i:%02i", length / 60, length % 60);

        gchar * title = aud_playlist_entry_get_title (playlist, i);
        row = gnt_tree_create_row(GNT_TREE(playlist_widget),
                                  title, len, NULL);
        g_free (title);

        gnt_tree_add_row_after(GNT_TREE(playlist_widget),
                               GINT_TO_POINTER(i+1),
                               row,
                               NULL,
                               GINT_TO_POINTER(i));

        g_free(len);
    }

    gnt_tree_set_selected(GNT_TREE(playlist_widget), GINT_TO_POINTER(pos+1));
}

static gboolean update_song_time(gpointer data)
{
    gint time = aud_drct_get_output_time() / 1000;
    gchar *text;

    text = g_strdup_printf("[%02i:%02i]", time / 60, time % 60);
    gnt_label_set_text(GNT_LABEL(time_widget), text);
    g_free(text);

    return TRUE;
}

static void update_playback_status()
{
    const gchar *text = playing_status();

    gnt_label_set_text(GNT_LABEL(status_widget), text);
}

static void update_playback_title()
{
    gint playlist = aud_playlist_get_active();
    gint pos = aud_playlist_get_position(playlist);
    gchar * title = aud_playlist_entry_get_title (playlist, pos);
    gnt_label_set_text(GNT_LABEL(title_widget), title);
    g_free (title);
}

static void set_song_info(gpointer hook_data, gpointer user_data)
{
    update_song_time(NULL);
    update_playback_title();
}

static gboolean mainwin_keypress_cb(GntWidget *widget, const char *text, gpointer data)
{
    g_return_val_if_fail(text != NULL, FALSE);

    switch (text[0]) {
      case 'z':
          aud_drct_pl_prev();
          update_playback_title();
          break;
      case 'x':
          aud_drct_play();
          break;
      case 'c':
          aud_drct_pause();
          break;
      case 'v':
          aud_drct_stop();
          break;
      case 'b':
          aud_drct_pl_next();
          update_playback_title();
          break;
      default:
          return FALSE;
    }
    return TRUE;
}

static gboolean playlist_keypress_cb(GntTree *widget, const gchar * text, gpointer data)
{
    if (text != NULL && text[0] == ' ' && text[1] == 0) {
        gint playlist = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "playlist"));
        gint pos = GPOINTER_TO_INT(gnt_tree_get_selection_data(widget)) - 1;

        aud_playlist_set_position(playlist, pos);

        if (!aud_drct_get_playing())
            aud_drct_play();

        return TRUE;
    }
    return FALSE;
}

static void gntui_mainwin_show()
{
    if (window) {
        gnt_window_present(window);
        return;
    }

    window = gnt_window_box_new(FALSE, TRUE);
    gnt_box_set_title(GNT_BOX(window), "Audacious2");
    gnt_box_set_fill(GNT_BOX(window), TRUE);
    gnt_box_set_alignment(GNT_BOX(window), GNT_ALIGN_TOP);

    playback_hbox = gnt_hbox_new(FALSE);
    gnt_box_add_widget(GNT_BOX(window), playback_hbox);

    status_widget = gnt_label_new("[]");
    gnt_box_add_widget(GNT_BOX(playback_hbox), status_widget);
    GNT_WIDGET_UNSET_FLAGS(status_widget, GNT_WIDGET_GROW_X);

    time_widget = gnt_label_new("[00:00]");
    gnt_box_add_widget(GNT_BOX(playback_hbox), time_widget);
    GNT_WIDGET_UNSET_FLAGS(time_widget, GNT_WIDGET_GROW_X);

    title_widget = gnt_label_new("");
    gnt_box_add_widget(GNT_BOX(playback_hbox), title_widget);

    playlist_widget = gnt_tree_new();
    g_object_set(G_OBJECT(playlist_widget), "columns", 2, NULL);
    gnt_tree_set_search_column(GNT_TREE(playlist_widget), 0);
    //TODO:    gnt_tree_set_search_function
    gnt_tree_set_show_title(GNT_TREE(playlist_widget), FALSE);
    fill_playlist();
    gnt_tree_adjust_columns(GNT_TREE(playlist_widget));
    gnt_box_add_widget(GNT_BOX(window), playlist_widget);

    g_signal_connect(G_OBJECT(playlist_widget), "key_pressed",
                     G_CALLBACK(playlist_keypress_cb), NULL);
    g_signal_connect(G_OBJECT(window), "key_pressed",
                     G_CALLBACK(mainwin_keypress_cb), NULL);

    set_song_info(NULL, NULL);

    gnt_widget_show(window);
    gnt_window_present(window);
}

static void ui_playback_begin(gpointer hook_data, gpointer user_data)
{
    update_playback_status();
    set_song_info(NULL, NULL);

    /* update song info 4 times a second */
    update_song_timeout_source = g_timeout_add(250, update_song_time, NULL);
}

static void ui_playback_stop(gpointer hook_data, gpointer user_data)
{
    if (update_song_timeout_source)
    {
        g_source_remove(update_song_timeout_source);
        update_song_timeout_source = 0;
    }

    update_playback_status();
    set_song_info(NULL, NULL);
}

static void ui_playback_end(gpointer hook_data, gpointer user_data)
{
    set_song_info(NULL, NULL);
}

static void ui_playlist_update(gpointer hook_data, gpointer user_data)
{
    gnt_tree_remove_all(GNT_TREE(playlist_widget));
    fill_playlist();
}

static void show_error_message(const gchar * markup)
{
    GntWidget *win;
    GntWidget *button;

    /* TODO: strip markup */
    win = gnt_vwindow_new(FALSE);
    gnt_box_add_widget(GNT_BOX(win), gnt_label_new(markup));
    gnt_box_set_title(GNT_BOX(win), "Error");
    gnt_box_set_alignment(GNT_BOX(win), GNT_ALIGN_MID);

    button = gnt_button_new("Close");
    g_signal_connect_swapped(G_OBJECT(button), "activate",
                             G_CALLBACK(gnt_widget_destroy), win);
    gnt_box_add_widget(GNT_BOX(win), button);

    gnt_widget_show(win);
    gnt_window_present(win);
}

static gboolean ui_initialize(InterfaceCbs * cbs)
{
    gnt_init();

    gntui_mainwin_show();

    gnt_register_action(_("Audacious2"), gntui_mainwin_show);
    gnt_register_action(_("Add Files"), add_files);
    gnt_register_action(_("Open Files"), open_files);

    hook_associate("title change", set_song_info, NULL);
    hook_associate("playback begin", ui_playback_begin, NULL);
    hook_associate("playback stop", ui_playback_stop, NULL);
    hook_associate("playback end", ui_playback_end, NULL);
    hook_associate("playlist update", ui_playlist_update, NULL);

    cbs->run_filebrowser = run_fileselector;
    cbs->hide_filebrowser = hide_fileselector;
    cbs->show_error = show_error_message;

    gnt_main();

    return TRUE;
}


static gboolean ui_finalize(void)
{
    hook_dissociate("title change", set_song_info);
    hook_dissociate("playback begin", ui_playback_begin);
    hook_dissociate("playback stop", ui_playback_stop);
    hook_dissociate("playback end", ui_playback_end);
    hook_dissociate("playlist update", ui_playlist_update);

    if (update_song_timeout_source)
    {
        g_source_remove(update_song_timeout_source);
        update_song_timeout_source = 0;
    }

    gnt_quit();

    return TRUE;
}

Interface gntui_interface = {
    .id = "gntui",
    .desc = N_("gnt interface"),
    .init = ui_initialize,
    .fini = ui_finalize,
};

SIMPLE_INTERFACE_PLUGIN("gntui", &gntui_interface);
