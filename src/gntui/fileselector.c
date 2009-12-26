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

#include <audacious/plugin.h>
#include <glib/gi18n.h>
#include <gnt/gntfilesel.h>
#include <gnt/gntbox.h>
#include "fileselector.h"

static GntWidget * window = NULL;

static void
file_add_cb(GntWidget *widget, GntFileSel **selector)
{
    gchar *file = gnt_file_sel_get_selected_file(*selector);
    gchar *uri = g_filename_to_uri((const gchar *) file, NULL, NULL);
    gchar *path = g_path_get_dirname(file);
    gint playlist = aud_playlist_get_active();

    g_free(file);

    g_free(aud_cfg->filesel_path);
    aud_cfg->filesel_path = path;

    if (uri == NULL)
        return;

    audacious_drct_pl_add_url_string (uri);

    gnt_widget_destroy(GNT_WIDGET(*selector));
    *selector = NULL;
}

static void
file_open_cb(GntWidget *widget, GntFileSel **selector)
{
    gint playlist = aud_playlist_get_active();

    aud_playlist_entry_delete(playlist, 0, aud_playlist_entry_count(playlist));
    aud_playlist_set_playing(playlist);

    file_add_cb(widget, selector);
}

static void
cancel_cb(GntWidget *widget, GntFileSel **selector)
{
    gnt_widget_destroy(GNT_WIDGET(*selector));
    *selector = NULL;
}

void
run_fileselector(gboolean play_button)
{
    if (window != NULL) {
        gnt_window_present(window);
        return;
    }

    window = gnt_file_sel_new();
    gnt_box_set_title(GNT_BOX(window),
                      play_button ? _("Open Files") : _("Add Files"));

    if (aud_cfg->filesel_path)
        gnt_file_sel_set_current_location(GNT_FILE_SEL(window),
                                          aud_cfg->filesel_path);

    gnt_file_sel_set_must_exist(GNT_FILE_SEL(window), TRUE);

    g_signal_connect(G_OBJECT(GNT_FILE_SEL(window)->select), "activate",
                     G_CALLBACK(play_button ? file_open_cb : file_add_cb),
                     &window);
    g_signal_connect(G_OBJECT(GNT_FILE_SEL(window)->cancel), "activate",
                     G_CALLBACK(cancel_cb), &window);

    gnt_widget_show(window);
}

void
open_files()
{
    run_fileselector(TRUE);
}

void
add_files()
{
    run_fileselector(FALSE);
}

void
hide_fileselector()
{
    gnt_widget_destroy(window);
    window = NULL;
}
