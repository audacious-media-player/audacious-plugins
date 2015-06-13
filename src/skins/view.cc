/*
 * view.c
 * Copyright 2014-2015 John Lindgren
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

#include <libaudcore/runtime.h>
#include <libaudcore/hook.h>

#include "plugin.h"
#include "plugin-window.h"
#include "skins_cfg.h"
#include "ui_equalizer.h"
#include "ui_main.h"
#include "ui_main_evlisteners.h"
#include "ui_playlist.h"
#include "ui_skinned_button.h"
#include "ui_skinned_equalizer_graph.h"
#include "ui_skinned_textbox.h"
#include "ui_skinned_menurow.h"
#include "ui_skinned_window.h"
#include "ui_vis.h"

void view_show_player (bool show)
{
    if (show)
    {
        gtk_window_present ((GtkWindow *) mainwin);
        show_plugin_windows ();
    }
    else
    {
        gtk_widget_hide (mainwin);
        hide_plugin_windows ();
    }

    view_apply_show_playlist ();
    view_apply_show_equalizer ();

    start_stop_visual (FALSE);
}

void view_set_show_playlist (bool show)
{
    aud_set_bool ("skins", "playlist_visible", show);
    hook_call ("skins set playlist_visible", nullptr);

    view_apply_show_playlist ();
}

void view_apply_show_playlist ()
{
    bool show = aud_get_bool ("skins", "playlist_visible");

    if (show && gtk_widget_get_visible (mainwin))
        gtk_window_present ((GtkWindow *) playlistwin);
    else
        gtk_widget_hide (playlistwin);

    button_set_active (mainwin_pl, show);
}

void view_set_show_equalizer (bool show)
{
    aud_set_bool ("skins", "equalizer_visible", show);
    hook_call ("skins set equalizer_visible", nullptr);

    view_apply_show_equalizer ();
}

void view_apply_show_equalizer ()
{
    bool show = aud_get_bool ("skins", "equalizer_visible");

    if (show && gtk_widget_get_visible (mainwin))
        gtk_window_present ((GtkWindow *) equalizerwin);
    else
        gtk_widget_hide (equalizerwin);

    button_set_active (mainwin_eq, show);
}

void view_set_player_shaded (bool shaded)
{
    aud_set_bool ("skins", "player_shaded", shaded);
    hook_call ("skins set player_shaded", nullptr);

    view_apply_player_shaded ();
}

void view_apply_player_shaded ()
{
    bool shaded = aud_get_bool ("skins", "player_shaded");

    window_set_shaded (mainwin, shaded);

    int width = shaded ? MAINWIN_SHADED_WIDTH : skin.hints.mainwin_width;
    int height = shaded ? MAINWIN_SHADED_HEIGHT : skin.hints.mainwin_height;
    window_set_size (mainwin, width, height);

    if (config.autoscroll)
        textbox_set_scroll (mainwin_info, ! shaded);
}

void view_set_playlist_shaded (bool shaded)
{
    aud_set_bool ("skins", "playlist_shaded", shaded);
    hook_call ("skins set playlist_shaded", nullptr);

    view_apply_playlist_shaded ();
}

void view_apply_playlist_shaded ()
{
    bool shaded = aud_get_bool ("skins", "playlist_shaded");

    window_set_shaded (playlistwin, shaded);

    int height = shaded ? MAINWIN_SHADED_HEIGHT : config.playlist_height;
    window_set_size (playlistwin, config.playlist_width, height);

    if (config.autoscroll)
        textbox_set_scroll (playlistwin_sinfo, shaded);
}

void view_set_equalizer_shaded (bool shaded)
{
    aud_set_bool ("skins", "equalizer_shaded", shaded);
    hook_call ("skins set equalizer_shaded", nullptr);

    view_apply_equalizer_shaded ();
}

void view_apply_equalizer_shaded ()
{
    bool shaded = aud_get_bool ("skins", "equalizer_shaded");

    window_set_shaded (equalizerwin, shaded);
    window_set_size (equalizerwin, 275, shaded ? 14 : 116);
}

void view_set_double_size (bool double_size)
{
    aud_set_bool ("skins", "double_size", double_size);
    hook_call ("skins set double_size", nullptr);

    view_apply_double_size ();
}

void view_apply_double_size ()
{
    skins_restart ();
}

void view_set_on_top (bool on_top)
{
    aud_set_bool ("skins", "always_on_top", on_top);
    hook_call ("skins set always_on_top", nullptr);

    view_apply_on_top ();
}

void view_apply_on_top ()
{
    bool on_top = aud_get_bool ("skins", "always_on_top");

    gtk_window_set_keep_above ((GtkWindow *) mainwin, on_top);
    gtk_window_set_keep_above ((GtkWindow *) equalizerwin, on_top);
    gtk_window_set_keep_above ((GtkWindow *) playlistwin, on_top);

    ui_skinned_menurow_update (mainwin_menurow);
}

void view_set_sticky (bool sticky)
{
    aud_set_bool ("skins", "sticky", sticky);
    hook_call ("skins set sticky", nullptr);

    view_apply_sticky ();
}

void view_apply_sticky ()
{
    bool sticky = aud_get_bool ("skins", "sticky");

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

void view_set_show_remaining (bool remaining)
{
    aud_set_bool ("skins", "show_remaining_time", remaining);
    hook_call ("skins set show_remaining_time", nullptr);

    view_apply_show_remaining ();
}

void view_apply_show_remaining ()
{
    mainwin_update_song_info ();
}

static GdkRegion * scale_mask (const Index<GdkRectangle> & mask, int scale)
{
    GdkRegion * region = nullptr;

    for (auto & rect : mask)
    {
        GdkRectangle scaled = {
            rect.x * scale,
            rect.y * scale,
            rect.width * scale,
            rect.height * scale
        };

        if (region)
            gdk_region_union_with_rect (region, & scaled);
        else
            region = gdk_region_rectangle (& scaled);
    }

    return region;
}

void view_apply_skin ()
{
    window_set_shapes (mainwin,
     scale_mask (skin.masks[SKIN_MASK_MAIN], config.scale),
     scale_mask (skin.masks[SKIN_MASK_MAIN_SHADE], config.scale));
    window_set_shapes (equalizerwin,
     scale_mask (skin.masks[SKIN_MASK_EQ], config.scale),
     scale_mask (skin.masks[SKIN_MASK_EQ_SHADE], config.scale));

    // hide the equalizer graph if we have a short eqmain.bmp
    int h = cairo_image_surface_get_height (skin.pixmaps[SKIN_EQMAIN].get ());
    gtk_widget_set_visible (equalizerwin_graph->gtk (), h >= 315);

    mainwin_refresh_hints ();
    textbox_update_all ();
    mainwin_vis->set_colors ();

    gtk_widget_queue_draw (mainwin);
    gtk_widget_queue_draw (equalizerwin);
    gtk_widget_queue_draw (playlistwin);
}
