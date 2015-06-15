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
        gtk_window_present ((GtkWindow *) mainwin->gtk ());
        show_plugin_windows ();
    }
    else
    {
        gtk_widget_hide (mainwin->gtk ());
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

    if (show && gtk_widget_get_visible (mainwin->gtk ()))
        gtk_window_present ((GtkWindow *) playlistwin->gtk ());
    else
        gtk_widget_hide (playlistwin->gtk ());

    mainwin_pl->set_active (show);
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

    if (show && gtk_widget_get_visible (mainwin->gtk ()))
        gtk_window_present ((GtkWindow *) equalizerwin->gtk ());
    else
        gtk_widget_hide (equalizerwin->gtk ());

    mainwin_eq->set_active (show);
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

    mainwin->set_shaded (shaded);

    int width = shaded ? MAINWIN_SHADED_WIDTH : skin.hints.mainwin_width;
    int height = shaded ? MAINWIN_SHADED_HEIGHT : skin.hints.mainwin_height;
    mainwin->resize (width, height);

    if (config.autoscroll)
        mainwin_info->set_scroll (! shaded);
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

    playlistwin->set_shaded (shaded);

    int height = shaded ? MAINWIN_SHADED_HEIGHT : config.playlist_height;
    playlistwin->resize (config.playlist_width, height);

    if (config.autoscroll)
        playlistwin_sinfo->set_scroll (shaded);
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

    equalizerwin->set_shaded (shaded);
    equalizerwin->resize (275, shaded ? 14 : 116);
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

    gtk_window_set_keep_above ((GtkWindow *) mainwin->gtk (), on_top);
    gtk_window_set_keep_above ((GtkWindow *) equalizerwin->gtk (), on_top);
    gtk_window_set_keep_above ((GtkWindow *) playlistwin->gtk (), on_top);

    mainwin_menurow->update ();
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
        gtk_window_stick ((GtkWindow *) mainwin->gtk ());
        gtk_window_stick ((GtkWindow *) equalizerwin->gtk ());
        gtk_window_stick ((GtkWindow *) playlistwin->gtk ());
    }
    else
    {
        gtk_window_unstick ((GtkWindow *) mainwin->gtk ());
        gtk_window_unstick ((GtkWindow *) equalizerwin->gtk ());
        gtk_window_unstick ((GtkWindow *) playlistwin->gtk ());
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

static cairo_region_t * scale_mask (const Index<GdkRectangle> & mask, int scale)
{
    cairo_region_t * region = nullptr;

    for (auto & rect : mask)
    {
        cairo_rectangle_int_t scaled = {
            rect.x * scale,
            rect.y * scale,
            rect.width * scale,
            rect.height * scale
        };

        if (region)
            cairo_region_union_rectangle (region, & scaled);
        else
            region = cairo_region_create_rectangle (& scaled);
    }

    return region;
}

void view_apply_skin ()
{
    mainwin->set_shapes
     (scale_mask (skin.masks[SKIN_MASK_MAIN], config.scale),
      scale_mask (skin.masks[SKIN_MASK_MAIN_SHADE], config.scale));
    equalizerwin->set_shapes
     (scale_mask (skin.masks[SKIN_MASK_EQ], config.scale),
      scale_mask (skin.masks[SKIN_MASK_EQ_SHADE], config.scale));

    // hide the equalizer graph if we have a short eqmain.bmp
    int h = cairo_image_surface_get_height (skin.pixmaps[SKIN_EQMAIN].get ());
    gtk_widget_set_visible (equalizerwin_graph->gtk (), h >= 315);

    mainwin_refresh_hints ();
    TextBox::update_all ();
    mainwin_vis->set_colors ();

    gtk_widget_queue_draw (mainwin->gtk ());
    gtk_widget_queue_draw (equalizerwin->gtk ());
    gtk_widget_queue_draw (playlistwin->gtk ());
}
