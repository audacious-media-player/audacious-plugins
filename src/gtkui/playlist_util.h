#ifndef __PLAYLISTUTIL_H__
#define __PLAYLISTUTIL_H__

void insert_drag_list(gint playlist, gint position, const gchar *list);
gint get_active_selected_pos(void);
gint reselect_selected(void);
void treeview_select_pos(GtkTreeView* tv, gint pos);
gint get_first_selected_pos(GtkTreeView *tv);
gint get_last_selected_pos(GtkTreeView *tv);
void playlist_shift_selected(gint playlist_num, gint old_pos, gint new_pos, gint selected_boundary_range);
GtkTreeView *get_active_playlist_treeview(void);
void playlist_get_changed_range(gint *start, gint *end);

static inline void insert_drag_list_into_active(gint position, const gchar *list)
{
    gint active_playlist_nr = aud_playlist_get_active();
    insert_drag_list(active_playlist_nr, position, list);
}

#endif // ifndef __PLAYLISTUTIL_H__

