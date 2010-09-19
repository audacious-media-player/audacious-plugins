/*  Audacious - Cross-platform multimedia player
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
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#ifndef UI_PLAYLIST_MODEL_H
#define UI_PLAYLIST_MODEL_H

#include <gtk/gtk.h>

extern gboolean multi_column_view;

enum
{
    PLAYLIST_COLUMN_NUM = 0,
    PLAYLIST_COLUMN_TEXT,
    PLAYLIST_COLUMN_QUEUED,
    PLAYLIST_COLUMN_TIME,
    PLAYLIST_COLUMN_WEIGHT,  /* PANGO_WEIGHT_BOLD or PANGO_WEIGHT_NORMAL, denotes currently played song */
    PLAYLIST_N_COLUMNS,
};

/* Multi column view */
enum
{
    PLAYLIST_MULTI_COLUMN_NUM = 0,
    PLAYLIST_MULTI_COLUMN_ARTIST,
    PLAYLIST_MULTI_COLUMN_YEAR,
    PLAYLIST_MULTI_COLUMN_ALBUM,
    PLAYLIST_MULTI_COLUMN_TRACK_NUM,
    PLAYLIST_MULTI_COLUMN_TITLE,
    PLAYLIST_MULTI_COLUMN_QUEUED,
    PLAYLIST_MULTI_COLUMN_TIME,
    PLAYLIST_MULTI_COLUMN_WEIGHT,  /* PANGO_WEIGHT_BOLD or PANGO_WEIGHT_NORMAL, denotes currently played song */
    PLAYLIST_N_MULTI_COLUMNS,
};

typedef struct _UiPlaylistModel      UiPlaylistModel;
typedef struct _UiPlaylistModelClass UiPlaylistModelClass;

UiPlaylistModel *ui_playlist_model_new(gint playlist);
gint ui_playlist_model_get_playlist (UiPlaylistModel * model);
void ui_playlist_model_set_playlist (UiPlaylistModel * model, gint playlist);

void treeview_update (GtkTreeView * tree, gint type, gint at, gint count);
void treeview_update_position (GtkTreeView * tree);
void treeview_set_focus_on_update (GtkTreeView * tree, gint focus);

#endif /* UI_PLAYLIST_MODEL_H */
