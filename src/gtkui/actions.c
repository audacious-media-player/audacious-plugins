/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2009  Audacious development team.
 *
 *  BMP - Cross-platform multimedia player
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* GDK including */
#include <gtk/gtk.h>

#if defined(USE_REGEX_ONIGURUMA)
#include <onigposix.h>
#elif defined(USE_REGEX_PCRE)
#include <pcreposix.h>
#else
#include <regex.h>
#endif

#include <audacious/plugin.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "ui_gtk.h"
#include "util.h"
#include "playlist_util.h"

static GtkWidget *mainwin_jtt = NULL;

static int ab_position_a = -1;
static int ab_position_b = -1;

/* toggleactionentries actions */

void action_playback_noplaylistadvance(GtkToggleAction * action)
{
    aud_cfg->no_playlist_advance = gtk_toggle_action_get_active(action);
}

void action_playback_repeat(GtkToggleAction * action)
{
    aud_cfg->repeat = gtk_toggle_action_get_active(action);
}

void action_playback_shuffle(GtkToggleAction * action)
{
    aud_cfg->shuffle = gtk_toggle_action_get_active(action);
}

void action_stop_after_current_song(GtkToggleAction * action)
{
    aud_cfg->stopaftersong = gtk_toggle_action_get_active(action);
}

/* actionentries actions */

void action_about_audacious(void)
{
    audgui_show_about_window();
}

void action_play_file(void)
{
    audgui_run_filebrowser(TRUE);
}

void action_play_location(void)
{
    audgui_show_add_url_window();
}

void action_ab_set(void)
{
    if (audacious_drct_get_length() > 0)
    {
        if (ab_position_a == -1)
        {
            ab_position_a = audacious_drct_get_time();
            ab_position_b = -1;
            /* info-text: Loop-Point A position set */
        }
        else if (ab_position_b == -1)
        {
            int time = audacious_drct_get_time();
            if (time > ab_position_a)
                ab_position_b = time;
            /* release info text */
        }
        else
        {
            ab_position_a = audacious_drct_get_time();
            ab_position_b = -1;
            /* info-text: Loop-Point A position reset */
        }
    }
}

void action_ab_clear(void)
{
    ab_position_a = -1;
    ab_position_b = -1;
    /* release info text */
}

void action_current_track_info(void)
{
    audgui_infowin_show_current();
}

void action_jump_to_file(void)
{
    audgui_jump_to_track();
}

void action_jump_to_playlist_start(void)
{
    audacious_drct_pl_set_pos(0);
}

static void mainwin_jump_to_time_cb(GtkWidget * widget, GtkWidget * entry)
{
    guint min = 0, sec = 0, params;
    gint time;

    params = sscanf(gtk_entry_get_text(GTK_ENTRY(entry)), "%u:%u", &min, &sec);

    if (params == 2)
        time = (min * 60) + sec;
    else if (params == 1)
        time = min;
    else
        return;

    audacious_drct_seek(time);
    gtk_widget_destroy(mainwin_jtt);
}


void mainwin_jump_to_time(void)
{
    GtkWidget *vbox, *hbox_new, *hbox_total;
    GtkWidget *time_entry, *label, *bbox, *jump, *cancel;
    GtkWidget *dialog;
    guint tindex;
    gchar time_str[10];

    if (!audacious_drct_get_playing())
    {
        dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, _("Can't jump to time when no track is being played.\n"));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    if (mainwin_jtt)
    {
        gtk_window_present(GTK_WINDOW(mainwin_jtt));
        return;
    }

    mainwin_jtt = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_type_hint(GTK_WINDOW(mainwin_jtt), GDK_WINDOW_TYPE_HINT_DIALOG);

    gtk_window_set_title(GTK_WINDOW(mainwin_jtt), _("Jump to Time"));
    gtk_window_set_position(GTK_WINDOW(mainwin_jtt), GTK_WIN_POS_CENTER);

    g_signal_connect(mainwin_jtt, "destroy", G_CALLBACK(gtk_widget_destroyed), &mainwin_jtt);
    gtk_container_set_border_width(GTK_CONTAINER(mainwin_jtt), 10);

    vbox = gtk_vbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(mainwin_jtt), vbox);

    hbox_new = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_new, TRUE, TRUE, 5);

    time_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox_new), time_entry, FALSE, FALSE, 5);
    g_signal_connect(time_entry, "activate", G_CALLBACK(mainwin_jump_to_time_cb), time_entry);

    gtk_widget_set_size_request(time_entry, 70, -1);
    label = gtk_label_new(_("minutes:seconds"));
    gtk_box_pack_start(GTK_BOX(hbox_new), label, FALSE, FALSE, 5);

    hbox_total = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_total, TRUE, TRUE, 5);
    gtk_widget_show(hbox_total);

    /* FIXME: Disable display of current track length. It's not
       updated when track changes */
#if 0
    label = gtk_label_new(_("Track length:"));
    gtk_box_pack_start(GTK_BOX(hbox_total), label, FALSE, FALSE, 5);

    len = aud_playlist_get_current_length() / 1000;
    g_snprintf(time_str, sizeof(time_str), "%u:%2.2u", len / 60, len % 60);
    label = gtk_label_new(time_str);

    gtk_box_pack_start(GTK_BOX(hbox_total), label, FALSE, FALSE, 10);
#endif

    bbox = gtk_hbutton_box_new();
    gtk_box_pack_start(GTK_BOX(vbox), bbox, TRUE, TRUE, 0);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(bbox), 5);

    cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    GTK_WIDGET_SET_FLAGS(cancel, GTK_CAN_DEFAULT);
    gtk_container_add(GTK_CONTAINER(bbox), cancel);
    g_signal_connect_swapped(cancel, "clicked", G_CALLBACK(gtk_widget_destroy), mainwin_jtt);

    jump = gtk_button_new_from_stock(GTK_STOCK_JUMP_TO);
    GTK_WIDGET_SET_FLAGS(jump, GTK_CAN_DEFAULT);
    gtk_container_add(GTK_CONTAINER(bbox), jump);
    g_signal_connect(jump, "clicked", G_CALLBACK(mainwin_jump_to_time_cb), time_entry);

    tindex = audacious_drct_get_time() / 1000;
    g_snprintf(time_str, sizeof(time_str), "%u:%2.2u", tindex / 60, tindex % 60);
    gtk_entry_set_text(GTK_ENTRY(time_entry), time_str);

    gtk_editable_select_region(GTK_EDITABLE(time_entry), 0, strlen(time_str));

    gtk_widget_show_all(mainwin_jtt);

    gtk_widget_grab_focus(time_entry);
    gtk_widget_grab_default(jump);
}

void action_jump_to_time(void)
{
    mainwin_jump_to_time();
}

void action_playback_next(void)
{
    audacious_drct_pl_next();
}

void action_playback_previous(void)
{
    audacious_drct_pl_prev();
}

void action_playback_play(void)
{
    if (ab_position_a != -1)
        audacious_drct_seek(ab_position_a);
    else if (audacious_drct_get_paused())
        audacious_drct_pause();
    else
        audacious_drct_play();
}

void action_playback_pause(void)
{
    audacious_drct_pause();
}

void action_playback_stop(void)
{
    audacious_drct_stop();
}

void action_preferences(void)
{
    show_preferences_window(TRUE);
}

void action_quit(void)
{
    audacious_drct_quit();
}

void action_playlist_track_info(void)
{
    gint playlist = aud_playlist_get_active();

    if (aud_playlist_selected_count(playlist) == 0)
        audgui_infowin_show_current ();
    else
    {
        gint entries = aud_playlist_entry_count(playlist);
        gint count;

        for (count = 0; count < entries; count++)
        {
            if (aud_playlist_entry_get_selected(playlist, count))
                break;
        }

        audgui_infowin_show (playlist, count);
    }
}

void action_queue_toggle(void)
{
    gint playlist = aud_playlist_get_active();
    gint queued = aud_playlist_queue_count(playlist);

    if (queued == 0)
        aud_playlist_queue_insert_selected(playlist, 0);
    else
        aud_playlist_queue_delete(playlist, 0, queued);
}

void action_playlist_sort_by_track_number(void)
{
    aud_playlist_sort_by_scheme(aud_playlist_get_active(), PLAYLIST_SORT_TRACK);
}

void action_playlist_sort_by_title(void)
{
    aud_playlist_sort_by_scheme(aud_playlist_get_active(), PLAYLIST_SORT_TITLE);
}

void action_playlist_sort_by_album(void)
{
    aud_playlist_sort_by_scheme(aud_playlist_get_active(), PLAYLIST_SORT_ALBUM);
}

void action_playlist_sort_by_artist(void)
{
    aud_playlist_sort_by_scheme(aud_playlist_get_active(), PLAYLIST_SORT_ARTIST);
}

void action_playlist_sort_by_full_path(void)
{
    aud_playlist_sort_by_scheme(aud_playlist_get_active(), PLAYLIST_SORT_PATH);
}

void action_playlist_sort_by_date(void)
{
    aud_playlist_sort_by_scheme(aud_playlist_get_active(), PLAYLIST_SORT_DATE);
}

void action_playlist_sort_by_filename(void)
{
    aud_playlist_sort_by_scheme(aud_playlist_get_active(), PLAYLIST_SORT_FILENAME);
}

void action_playlist_sort_selected_by_track_number(void)
{
    aud_playlist_sort_selected_by_scheme(aud_playlist_get_active(), PLAYLIST_SORT_TRACK);
}

void action_playlist_sort_selected_by_title(void)
{
    aud_playlist_sort_selected_by_scheme(aud_playlist_get_active(), PLAYLIST_SORT_TITLE);
}

void action_playlist_sort_selected_by_album(void)
{
    aud_playlist_sort_selected_by_scheme(aud_playlist_get_active(), PLAYLIST_SORT_ALBUM);
}

void action_playlist_sort_selected_by_artist(void)
{
    aud_playlist_sort_selected_by_scheme(aud_playlist_get_active(), PLAYLIST_SORT_ARTIST);
}

void action_playlist_sort_selected_by_full_path(void)
{
    aud_playlist_sort_selected_by_scheme(aud_playlist_get_active(), PLAYLIST_SORT_PATH);
}

void action_playlist_sort_selected_by_date(void)
{
    aud_playlist_sort_selected_by_scheme(aud_playlist_get_active(), PLAYLIST_SORT_DATE);
}

void action_playlist_sort_selected_by_filename(void)
{
    aud_playlist_sort_selected_by_scheme(aud_playlist_get_active(), PLAYLIST_SORT_FILENAME);
}

void action_playlist_randomize_list(void)
{
    aud_playlist_randomize(aud_playlist_get_active());
}

void action_playlist_reverse_list(void)
{
    aud_playlist_reverse(aud_playlist_get_active());
}

void action_playlist_clear_queue(void)
{
    gint playlist = aud_playlist_get_active();
    aud_playlist_queue_delete(playlist, 0, aud_playlist_queue_count(playlist));
}

void action_playlist_remove_unavailable(void)
{
    aud_playlist_remove_failed(aud_playlist_get_active());
}

void action_playlist_remove_dupes_by_title(void)
{
    aud_playlist_remove_duplicates_by_scheme(aud_playlist_get_active(), PLAYLIST_SORT_TITLE);
}

void action_playlist_remove_dupes_by_filename(void)
{
    aud_playlist_remove_duplicates_by_scheme(aud_playlist_get_active(), PLAYLIST_SORT_FILENAME);
}

void action_playlist_remove_dupes_by_full_path(void)
{
    aud_playlist_remove_duplicates_by_scheme(aud_playlist_get_active(), PLAYLIST_SORT_PATH);
}

void action_playlist_remove_all(void)
{
    gint playlist = aud_playlist_get_active();

    aud_playlist_entry_delete(playlist, 0, aud_playlist_entry_count(playlist));
}

void action_playlist_remove_selected(GtkAction *act)
{
    aud_playlist_delete_selected(aud_playlist_get_active());
    gint active_playlist_num = aud_playlist_get_active();
    gint sel_pos;
    gboolean clap_sel_pos = FALSE;

    sel_pos = get_active_selected_pos();
    gint max_pos = aud_playlist_entry_count(active_playlist_num) -  aud_playlist_selected_count(active_playlist_num) - 1;
    if (sel_pos > max_pos)
        clap_sel_pos = TRUE;

    aud_playlist_delete_selected(active_playlist_num);

    if (clap_sel_pos)
        treeview_select_pos(get_active_playlist_treeview(), sel_pos);
}

void action_playlist_remove_unselected(void)
{
    gint playlist = aud_playlist_get_active();
    gint entries = aud_playlist_entry_count(playlist);
    gint count;

    for (count = 0; count < entries; count++)
        aud_playlist_entry_set_selected(playlist, count, !aud_playlist_entry_get_selected(playlist, count));

    aud_playlist_delete_selected(playlist);
    aud_playlist_select_all(playlist, TRUE);
}

void action_playlist_add_files(void)
{
    audgui_run_filebrowser(FALSE);
}

void action_playlist_add_url(void)
{
    audgui_show_add_url_window();
}

void action_playlist_new(void)
{
    gint playlist = aud_playlist_count();

    aud_playlist_insert(playlist);
    //aud_playlist_set_active(playlist);
}

void action_playlist_prev(void)
{
    aud_playlist_set_active(aud_playlist_get_active() - 1);
}

void action_playlist_next(void)
{
    aud_playlist_set_active(aud_playlist_get_active() + 1);
}

void action_playlist_delete(void)
{
    aud_playlist_delete(aud_playlist_get_active());
}

#if 0
static void on_static_toggle(GtkToggleButton * button, gpointer data)
{
    Playlist *playlist = aud_playlist_get_active();

    playlist->attribute = gtk_toggle_button_get_active(button) ? playlist->attribute | PLAYLIST_STATIC : playlist->attribute & ~PLAYLIST_STATIC;
}

static void on_relative_toggle(GtkToggleButton * button, gpointer data)
{
    Playlist *playlist = aud_playlist_get_active();

    playlist->attribute = gtk_toggle_button_get_active(button) ? playlist->attribute | PLAYLIST_USE_RELATIVE : playlist->attribute & ~PLAYLIST_USE_RELATIVE;
}
#endif

static gchar *playlist_file_selection_save(const gchar * title, const gchar * default_filename)
{
    GtkWidget *dialog;
    gchar *filename;
#if 0
    GtkWidget *hbox;
    GtkWidget *toggle, *toggle2;
#endif

    g_return_val_if_fail(title != NULL, NULL);

    dialog = make_filebrowser(title, TRUE);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), aud_cfg->playlist_path);
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), default_filename);

#if 0
    hbox = gtk_hbox_new(FALSE, 5);

    /* static playlist */
    toggle = gtk_check_button_new_with_label(_("Save as Static Playlist"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), (aud_playlist_get_active()->attribute & PLAYLIST_STATIC) ? TRUE : FALSE);
    g_signal_connect(G_OBJECT(toggle), "toggled", G_CALLBACK(on_static_toggle), dialog);
    gtk_box_pack_start(GTK_BOX(hbox), toggle, FALSE, FALSE, 0);

    /* use relative path */
    toggle2 = gtk_check_button_new_with_label(_("Use Relative Path"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle2), (aud_playlist_get_active()->attribute & PLAYLIST_USE_RELATIVE) ? TRUE : FALSE);
    g_signal_connect(G_OBJECT(toggle2), "toggled", G_CALLBACK(on_relative_toggle), dialog);
    gtk_box_pack_start(GTK_BOX(hbox), toggle2, FALSE, FALSE, 0);

    gtk_widget_show_all(hbox);
    gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(dialog), hbox);
#endif

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    else
        filename = NULL;

    gtk_widget_destroy(dialog);
    return filename;
}

static void show_playlist_save_error(GtkWindow * parent, const gchar * filename)
{
    GtkWidget *dialog;

    g_return_if_fail(GTK_IS_WINDOW(parent));
    g_return_if_fail(filename);

    dialog = gtk_message_dialog_new(GTK_WINDOW(parent), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, _("Error writing playlist \"%s\": %s"), filename, strerror(errno));

    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);    /* centering */
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static gboolean show_playlist_overwrite_prompt(GtkWindow * parent, const gchar * filename)
{
    GtkWidget *dialog;
    gint result;

    g_return_val_if_fail(GTK_IS_WINDOW(parent), FALSE);
    g_return_val_if_fail(filename != NULL, FALSE);

    dialog = gtk_message_dialog_new(GTK_WINDOW(parent), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, _("%s already exist. Continue?"), filename);

    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);    /* centering */
    result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    return (result == GTK_RESPONSE_YES);
}

static void show_playlist_save_format_error(GtkWindow * parent, const gchar * filename)
{
    const gchar *markup = N_("<b><big>Unable to save playlist.</big></b>\n\n" "Unknown file type for '%s'.\n");

    GtkWidget *dialog;

    g_return_if_fail(GTK_IS_WINDOW(parent));
    g_return_if_fail(filename != NULL);

    dialog = gtk_message_dialog_new_with_markup(GTK_WINDOW(parent), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, _(markup), filename);

    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);    /* centering */
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void playlistwin_save_playlist(const gchar * filename)
{
    PlaylistContainer *plc;
    gchar *ext = strrchr(filename, '.') + 1;

    plc = aud_playlist_container_find(ext);
    if (plc == NULL)
    {
        show_playlist_save_format_error(NULL, filename);
        return;
    }

    aud_str_replace_in(&aud_cfg->playlist_path, g_path_get_dirname(filename));

    if (g_file_test(filename, G_FILE_TEST_IS_REGULAR))
        if (!show_playlist_overwrite_prompt(NULL, filename))
            return;

    if (!aud_playlist_save(aud_playlist_get_active(), filename))
        show_playlist_save_error(NULL, filename);
}

void action_playlist_save_list(void)
{
    const gchar *default_filename = aud_playlist_get_filename(aud_playlist_get_active());

    gchar *dot = NULL, *basename = NULL;
    gchar *filename = playlist_file_selection_save(_("Save Playlist"), default_filename);

    if (filename)
    {
        /* Default extension */
        basename = g_path_get_basename(filename);
        dot = strrchr(basename, '.');
        if (dot == NULL || dot == basename)
        {
            gchar *oldname = filename;
#ifdef HAVE_XSPF_PLAYLIST
            filename = g_strconcat(oldname, ".xspf", NULL);
#else
            filename = g_strconcat(oldname, ".m3u", NULL);
#endif
            g_free(oldname);
        }
        g_free(basename);

        playlistwin_save_playlist(filename);
        g_free(filename);
    }
}

void action_playlist_save_default_list(void)
{
#if 0
    Playlist *playlist = aud_playlist_get_active();

    playlist_save(playlist, aud_paths[BMP_PATH_PLAYLIST_FILE]);
#endif
}

static void playlistwin_load_playlist(const gchar * filename)
{
    gint playlist = aud_playlist_get_active();

    g_return_if_fail(filename != NULL);

    aud_str_replace_in(&aud_cfg->playlist_path, g_path_get_dirname(filename));

    aud_playlist_entry_delete(playlist, 0, aud_playlist_entry_count(playlist));
    aud_playlist_insert_playlist(playlist, 0, filename);
    aud_playlist_set_filename(playlist, filename);

    if (aud_playlist_get_title(playlist) == NULL)
        aud_playlist_set_title(playlist, filename);
}

static gchar *playlist_file_selection_load(const gchar * title, const gchar * default_filename)
{
    GtkWidget *dialog;
    gchar *filename;

    g_return_val_if_fail(title != NULL, NULL);

    dialog = make_filebrowser(title, FALSE);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), aud_cfg->playlist_path);
    if (default_filename)
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), default_filename);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);    /* centering */

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    else
        filename = NULL;

    gtk_widget_destroy(dialog);
    return filename;
}

void action_playlist_load_list(void)
{
    const gchar *default_filename = aud_playlist_get_filename(aud_playlist_get_active());
    gchar *filename = playlist_file_selection_load(_("Load Playlist"), default_filename);

    if (filename)
    {
        playlistwin_load_playlist(filename);
        g_free(filename);
    }
}

void action_playlist_refresh_list(void)
{
    aud_playlist_rescan(aud_playlist_get_active());
}

void action_open_list_manager(void)
{
    audgui_playlist_manager_ui_show(window);
}

void action_playlist_search_and_select(void)
{
    //playlistwin_select_search();
}

void action_playlist_invert_selection(void)
{
    g_message("implement me");
}

void action_playlist_select_none(void)
{
    g_message("implement me");
}

void action_playlist_select_all(void)
{
    g_message("implement me");
}

void action_playlist_save_all_playlists(void)
{
    aud_save_all_playlists();
}

