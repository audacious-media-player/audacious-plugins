/*
 * view.c
 * Copyright 2014 John Lindgren
 *
 * This file is part of Audacious.
 *
 * Audacious is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2 or version 3 of the License.
 *
 * Audacious is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Audacious. If not, see <http://www.gnu.org/licenses/>.
 *
 * The Audacious team does not consider modular code linking to Audacious or
 * using our public API to be a derived work.
 */

#include "view.h"

#include <audacious/misc.h>
#include <libaudcore/hook.h>

#include "skins_cfg.h"
#include "ui_equalizer.h"
#include "ui_main.h"
#include "ui_main_evlisteners.h"
#include "ui_playlist.h"
#include "ui_skinned_button.h"
#include "ui_skinned_menurow.h"
#include "ui_skinned_window.h"

void view_show_player (bool_t show)
{
    if (show)
        gtk_window_present ((GtkWindow *) mainwin);
    else
        gtk_widget_hide (mainwin);

    view_apply_show_playlist ();
    view_apply_show_equalizer ();

    start_stop_visual (FALSE);
}

void view_set_show_playlist (bool_t show)
{
    aud_set_bool ("skins", "playlist_visible", show);
    hook_call ("skins set playlist_visible", NULL);

    view_apply_show_playlist ();
}

void view_apply_show_playlist (void)
{
    bool_t show = aud_get_bool ("skins", "playlist_visible");

    if (show && gtk_widget_get_visible (mainwin))
        gtk_window_present ((GtkWindow *) playlistwin);
    else
        gtk_widget_hide (playlistwin);

    button_set_active (mainwin_pl, show);
}

void view_set_show_equalizer (bool_t show)
{
    aud_set_bool ("skins", "equalizer_visible", show);
    hook_call ("skins set equalizer_visible", NULL);

    view_apply_show_equalizer ();
}

void view_apply_show_equalizer (void)
{
    bool_t show = aud_get_bool ("skins", "equalizer_visible");

    if (show && gtk_widget_get_visible (mainwin))
        gtk_window_present ((GtkWindow *) equalizerwin);
    else
        gtk_widget_hide (equalizerwin);

    button_set_active (mainwin_eq, show);
}

void view_set_player_shaded (bool_t shaded)
{
    aud_set_bool ("skins", "player_shaded", shaded);
    hook_call ("skins set player_shaded", NULL);

    view_apply_player_shaded ();
}

void view_apply_player_shaded (void)
{
    bool_t shaded = aud_get_bool ("skins", "player_shaded");

    window_set_shaded (mainwin, shaded);

    int width = shaded ? MAINWIN_SHADED_WIDTH : active_skin->properties.mainwin_width;
    int height = shaded ? MAINWIN_SHADED_HEIGHT : active_skin->properties.mainwin_height;
    window_set_size (mainwin, width, height);

    mainwin_set_shape ();
}

void view_set_playlist_shaded (bool_t shaded)
{
    aud_set_bool ("skins", "playlist_shaded", shaded);
    hook_call ("skins set playlist_shaded", NULL);

    view_apply_playlist_shaded ();
}

void view_apply_playlist_shaded (void)
{
    bool_t shaded = aud_get_bool ("skins", "playlist_shaded");

    window_set_shaded (playlistwin, shaded);

    int height = shaded ? MAINWIN_SHADED_HEIGHT : config.playlist_height;
    window_set_size (playlistwin, config.playlist_width, height);

    playlistwin_update ();
}

void view_set_equalizer_shaded (bool_t shaded)
{
    aud_set_bool ("skins", "equalizer_shaded", shaded);
    hook_call ("skins set equalizer_shaded", NULL);

    view_apply_equalizer_shaded ();
}

void view_apply_equalizer_shaded (void)
{
    bool_t shaded = aud_get_bool ("skins", "equalizer_shaded");

    window_set_shaded (equalizerwin, shaded);
    window_set_size (equalizerwin, 275, shaded ? 14 : 116);

    equalizerwin_set_shape ();
}

void view_set_on_top (bool_t on_top)
{
    aud_set_bool ("skins", "always_on_top", on_top);
    hook_call ("skins set always_on_top", NULL);

    view_apply_on_top ();
}

void view_apply_on_top (void)
{
    bool_t on_top = aud_get_bool ("skins", "always_on_top");

    gtk_window_set_keep_above ((GtkWindow *) mainwin, on_top);
    gtk_window_set_keep_above ((GtkWindow *) equalizerwin, on_top);
    gtk_window_set_keep_above ((GtkWindow *) playlistwin, on_top);

    ui_skinned_menurow_update (mainwin_menurow);
}

void view_set_sticky (bool_t sticky)
{
    aud_set_bool ("skins", "sticky", sticky);
    hook_call ("skins set sticky", NULL);

    view_apply_sticky ();
}

void view_apply_sticky (void)
{
    bool_t sticky = aud_get_bool ("skins", "sticky");

    if (sticky)
    {
        gtk_window_stick ((GtkWindow *) mainwin);
        gtk_window_stick ((GtkWindow *) equalizerwin);
        gtk_window_stick ((GtkWindow *) playlistwin);
    }
    else
    {
        gtk_window_unstick ((GtkWindow *) mainwin);
        gtk_window_unstick ((GtkWindow *) equalizerwin);
        gtk_window_unstick ((GtkWindow *) playlistwin);
    }
}

void view_set_show_remaining (bool_t remaining)
{
    aud_set_bool ("skins", "show_remaining_time", remaining);
    hook_call ("skins set show_remaining_time", NULL);

    view_apply_show_remaining ();
}

void view_apply_show_remaining (void)
{
    mainwin_update_song_info ();
}
