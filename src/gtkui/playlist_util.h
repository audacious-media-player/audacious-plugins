/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2011  Audacious development team
 *  Copyright (C) 2010 Michał Lipski <tallica@o2.pl>
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

#ifndef __PLAYLISTUTIL_H__
#define __PLAYLISTUTIL_H__

GtkWidget * playlist_get_treeview (gint playlist);

gint playlist_count_selected_in_range (gint list, gint top, gint length);
gint playlist_get_focus (gint list);
void playlist_song_info (void);
void playlist_queue_toggle (void);
void playlist_delete_selected (void);
void playlist_copy (void);
void playlist_cut (void);
void playlist_paste (void);

/* ui_playlist_notebook.c */
void playlist_follow (gint list, gint row);

#endif
