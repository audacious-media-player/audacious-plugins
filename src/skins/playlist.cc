/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2011  Audacious development team.
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

#include <inttypes.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/runtime.h>
#include <libaudcore/tuple.h>
#include <libaudgui/libaudgui.h>

#include "actions-mainwin.h"
#include "dnd.h"
#include "drag-handle.h"
#include "menus.h"
#include "plugin.h"
#include "skins_cfg.h"
#include "main.h"
#include "playlist.h"
#include "button.h"
#include "playlist-widget.h"
#include "playlist-slider.h"
#include "textbox.h"
#include "window.h"
#include "view.h"

#define PLAYLISTWIN_MIN_WIDTH           MAINWIN_WIDTH
#define PLAYLISTWIN_MIN_HEIGHT          MAINWIN_HEIGHT
#define PLAYLISTWIN_WIDTH_SNAP          25
#define PLAYLISTWIN_HEIGHT_SNAP         29
#define PLAYLISTWIN_SHADED_HEIGHT       MAINWIN_SHADED_HEIGHT

#define APPEND(b, ...) snprintf (b + strlen (b), sizeof b - strlen (b), __VA_ARGS__)

class PlWindow : public Window
{
public:
    PlWindow (bool shaded) :
    Window (WINDOW_PLAYLIST, & config.playlist_x, & config.playlist_y,
     config.playlist_width, shaded ? PLAYLISTWIN_SHADED_HEIGHT :
     config.playlist_height, shaded) {}

private:
    void draw (cairo_t * cr);
    bool button_press (GdkEventButton * event);
    bool scroll (GdkEventScroll * event);
};

Window * playlistwin;
PlaylistWidget * playlistwin_list;
TextBox * playlistwin_sinfo;

static Button * playlistwin_shade, * playlistwin_close;
static Button * playlistwin_shaded_shade, * playlistwin_shaded_close;

static PlaylistSlider * playlistwin_slider;
static TextBox * playlistwin_time_min, * playlistwin_time_sec;
static TextBox * playlistwin_info;
static Button * playlistwin_srew, * playlistwin_splay;
static Button * playlistwin_spause, * playlistwin_sstop;
static Button * playlistwin_sfwd, * playlistwin_seject;
static Button * playlistwin_sscroll_up, *playlistwin_sscroll_down;
static DragHandle * resize_handle, * sresize_handle;
static Button * button_add, * button_sub, * button_sel, * button_misc, * button_list;

static int resize_base_width, resize_base_height;
static int drop_position;
static bool song_changed;

static void update_info ()
{
    auto playlist = Playlist::active_playlist ();
    StringBuf s1 = str_format_time (playlist.selected_length_ms ());
    StringBuf s2 = str_format_time (playlist.total_length_ms ());
    playlistwin_info->set_text (str_concat ({s1, "/", s2}));
}

static void update_rollup_text ()
{
    auto playlist = Playlist::active_playlist ();
    int entry = playlist.get_position ();
    Tuple tuple = playlist.entry_tuple (entry, Playlist::NoWait);
    char scratch[512];

    scratch[0] = 0;

    if (entry > -1)
    {
        String title = tuple.get_str (Tuple::FormattedTitle);
        int length = tuple.get_int (Tuple::Length);

        if (aud_get_bool (nullptr, "show_numbers_in_pl"))
            APPEND (scratch, "%d. ", 1 + entry);

        APPEND (scratch, "%s", (const char *) title);

        if (length >= 0)
        {
            StringBuf buf = str_format_time (length);
            APPEND (scratch, " (%s)", (const char *) buf);
        }
    }

    playlistwin_sinfo->set_text (scratch);
}

static void playlistwin_shade_toggle ()
{
    view_set_playlist_shaded (! aud_get_bool ("skins", "playlist_shaded"));
}

static void playlistwin_scroll (int dir)
{
    int rows, first;

    playlistwin_list->row_info (& rows, & first);
    playlistwin_list->scroll_to (first + dir * rows / 3);
}

static void playlistwin_scroll_up_pushed ()
{
    playlistwin_scroll (-1);
}

static void playlistwin_scroll_down_pushed ()
{
    playlistwin_scroll (1);
}

/* note: height is ignored if the window is shaded */
static void playlistwin_resize (int w, int h)
{
    int tx, ty;

    tx = (w - PLAYLISTWIN_MIN_WIDTH) / PLAYLISTWIN_WIDTH_SNAP;
    tx = (tx * PLAYLISTWIN_WIDTH_SNAP) + PLAYLISTWIN_MIN_WIDTH;
    if (tx < PLAYLISTWIN_MIN_WIDTH)
        tx = PLAYLISTWIN_MIN_WIDTH;

    if (! aud_get_bool ("skins", "playlist_shaded"))
    {
        ty = (h - PLAYLISTWIN_MIN_HEIGHT) / PLAYLISTWIN_HEIGHT_SNAP;
        ty = (ty * PLAYLISTWIN_HEIGHT_SNAP) + PLAYLISTWIN_MIN_HEIGHT;
        if (ty < PLAYLISTWIN_MIN_HEIGHT)
            ty = PLAYLISTWIN_MIN_HEIGHT;
    }
    else
        ty = config.playlist_height;

    if (tx == config.playlist_width && ty == config.playlist_height)
        return;

    config.playlist_width = w = tx;
    config.playlist_height = h = ty;

    playlistwin_list->resize (w - 31, h - 58);
    playlistwin->move_widget (false, playlistwin_slider, w - 15, 20);
    playlistwin_slider->resize (h - 58);

    playlistwin->move_widget (true, playlistwin_shaded_shade, w - 21, 3);
    playlistwin->move_widget (true, playlistwin_shaded_close, w - 11, 3);
    playlistwin->move_widget (false, playlistwin_shade, w - 21, 3);
    playlistwin->move_widget (false, playlistwin_close, w - 11, 3);

    playlistwin->move_widget (false, playlistwin_time_min, w - 82, h - 15);
    playlistwin->move_widget (false, playlistwin_time_sec, w - 64, h - 15);
    playlistwin->move_widget (false, playlistwin_info, w - 143, h - 28);

    playlistwin->move_widget (false, playlistwin_srew, w - 144, h - 16);
    playlistwin->move_widget (false, playlistwin_splay, w - 138, h - 16);
    playlistwin->move_widget (false, playlistwin_spause, w - 128, h - 16);
    playlistwin->move_widget (false, playlistwin_sstop, w - 118, h - 16);
    playlistwin->move_widget (false, playlistwin_sfwd, w - 109, h - 16);
    playlistwin->move_widget (false, playlistwin_seject, w - 100, h - 16);
    playlistwin->move_widget (false, playlistwin_sscroll_up, w - 14, h - 35);
    playlistwin->move_widget (false, playlistwin_sscroll_down, w - 14, h - 30);

    playlistwin->move_widget (false, resize_handle, w - 20, h - 20);
    playlistwin->move_widget (true, sresize_handle, w - 31, 0);

    playlistwin_sinfo->set_width (w - 35);

    playlistwin->move_widget (false, button_add, 12, h - 29);
    playlistwin->move_widget (false, button_sub, 40, h - 29);
    playlistwin->move_widget (false, button_sel, 68, h - 29);
    playlistwin->move_widget (false, button_misc, 100, h - 29);
    playlistwin->move_widget (false, button_list, w - 46, h - 29);
}

bool PlWindow::scroll (GdkEventScroll * event)
{
    switch (event->direction)
    {
    case GDK_SCROLL_DOWN:
        playlistwin_scroll (1);
        break;
    case GDK_SCROLL_UP:
        playlistwin_scroll (-1);
        break;
    default:
        break;
    }

    return true;
}

bool PlWindow::button_press (GdkEventButton * event)
{
    if (event->button == 1 && event->type == GDK_2BUTTON_PRESS &&
     event->window == gtk_widget_get_window (gtk ()) && event->y < 14)
    {
        playlistwin_shade_toggle ();
        return true;
    }

    if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
    {
        menu_popup (UI_MENU_PLAYLIST, event->x_root, event->y_root, false, false, 3, event->time);
        return true;
    }

    return Window::button_press (event);
}

void playlistwin_hide_timer ()
{
    playlistwin_time_min->set_text (nullptr);
    playlistwin_time_sec->set_text (nullptr);
}

void playlistwin_set_time (const char * minutes, const char * seconds)
{
    playlistwin_time_min->set_text (minutes);
    playlistwin_time_sec->set_text (seconds);
}

static void drag_motion (GtkWidget * widget, GdkDragContext * context, int x,
 int y, unsigned time, void * unused)
{
    if (! aud_get_bool ("skins", "playlist_shaded"))
        playlistwin_list->hover (x - 12, y - 20);
}

static void drag_leave (GtkWidget * widget, GdkDragContext * context, unsigned time,
 void * unused)
{
    if (! aud_get_bool ("skins", "playlist_shaded"))
        playlistwin_list->hover_end ();
}

static void drag_drop (GtkWidget * widget, GdkDragContext * context, int x,
 int y, unsigned time, void * unused)
{
    if (aud_get_bool ("skins", "playlist_shaded"))
        drop_position = -1;
    else
    {
        playlistwin_list->hover (x - 12, y - 20);
        drop_position = playlistwin_list->hover_end ();
    }
}

static void drag_data_received (GtkWidget * widget, GdkDragContext * context,
 int x, int y, GtkSelectionData * data, unsigned info, unsigned time, void * unused)
{
    audgui_urilist_insert (Playlist::active_playlist (), drop_position,
     (const char *) gtk_selection_data_get_data (data));
    drop_position = -1;
}

static void playlistwin_hide ()
{
    view_set_show_playlist (false);
}

static void resize_press ()
{
    resize_base_width = config.playlist_width;
    resize_base_height = config.playlist_height;
}

static void resize_drag (int x_offset, int y_offset)
{
    bool shaded = aud_get_bool ("skins", "playlist_shaded");

    /* compromise between rounding and truncating; this has no real
     * justification at all other than it "looks about right". */
    playlistwin_resize (resize_base_width + x_offset + PLAYLISTWIN_WIDTH_SNAP /
     3, resize_base_height + y_offset + PLAYLISTWIN_HEIGHT_SNAP / 3);
    playlistwin->resize (config.playlist_width, shaded ?
     PLAYLISTWIN_SHADED_HEIGHT : config.playlist_height);
}

static void button_add_cb (Button * button, GdkEventButton * event)
{
    int xpos, ypos;
    playlistwin->getPosition (& xpos, & ypos);
    menu_popup (UI_MENU_PLAYLIST_ADD, xpos + 12 * config.scale,
     ypos + (config.playlist_height - 8) * config.scale, false, true,
     event->button, event->time);
}

static void button_sub_cb (Button * button, GdkEventButton * event)
{
    int xpos, ypos;
    playlistwin->getPosition (& xpos, & ypos);
    menu_popup (UI_MENU_PLAYLIST_REMOVE, xpos + 40 * config.scale,
     ypos + (config.playlist_height - 8) * config.scale, false, true,
     event->button, event->time);
}

static void button_sel_cb (Button * button, GdkEventButton * event)
{
    int xpos, ypos;
    playlistwin->getPosition (& xpos, & ypos);
    menu_popup (UI_MENU_PLAYLIST_SELECT, xpos + 68 * config.scale,
     ypos + (config.playlist_height - 8) * config.scale, false, true,
     event->button, event->time);
}

static void button_misc_cb (Button * button, GdkEventButton * event)
{
    int xpos, ypos;
    playlistwin->getPosition (& xpos, & ypos);
    menu_popup (UI_MENU_PLAYLIST_SORT, xpos + 100 * config.scale,
     ypos + (config.playlist_height - 8) * config.scale, false, true,
     event->button, event->time);
}

static void button_list_cb (Button * button, GdkEventButton * event)
{
    int xpos, ypos;
    playlistwin->getPosition (& xpos, & ypos);
    menu_popup (UI_MENU_PLAYLIST,
     xpos + (config.playlist_width - 12) * config.scale,
     ypos + (config.playlist_height - 8) * config.scale, true, true,
     event->button, event->time);
}

static void playlistwin_create_widgets ()
{
    int w = config.playlist_width, h = config.playlist_height;

    bool shaded = aud_get_bool ("skins", "playlist_shaded");
    playlistwin_sinfo = new TextBox (w - 35, nullptr, shaded && config.autoscroll);
    playlistwin->put_widget (true, playlistwin_sinfo, 4, 4);

    playlistwin_shaded_shade = new Button (9, 9, 128, 45, 150, 42, SKIN_PLEDIT, SKIN_PLEDIT);
    playlistwin->put_widget (true, playlistwin_shaded_shade, w - 21, 3);
    playlistwin_shaded_shade->on_release ((ButtonCB) playlistwin_shade_toggle);

    playlistwin_shaded_close = new Button (9, 9, 138, 45, 52, 42, SKIN_PLEDIT, SKIN_PLEDIT);
    playlistwin->put_widget (true, playlistwin_shaded_close, w - 11, 3);
    playlistwin_shaded_close->on_release ((ButtonCB) playlistwin_hide);

    playlistwin_shade = new Button (9, 9, 157, 3, 62, 42, SKIN_PLEDIT, SKIN_PLEDIT);
    playlistwin->put_widget (false, playlistwin_shade, w - 21, 3);
    playlistwin_shade->on_release ((ButtonCB) playlistwin_shade_toggle);

    playlistwin_close = new Button (9, 9, 167, 3, 52, 42, SKIN_PLEDIT, SKIN_PLEDIT);
    playlistwin->put_widget (false, playlistwin_close, w - 11, 3);
    playlistwin_close->on_release ((ButtonCB) playlistwin_hide);

    String font = aud_get_str ("skins", "playlist_font");
    playlistwin_list = new PlaylistWidget (w - 31, h - 58, font);
    playlistwin->put_widget (false, playlistwin_list, 12, 20);

    /* playlist list box slider */
    playlistwin_slider = new PlaylistSlider (playlistwin_list, h - 58);
    playlistwin->put_widget (false, playlistwin_slider, w - 15, 20);
    playlistwin_list->set_slider (playlistwin_slider);

    playlistwin_time_min = new TextBox (15, nullptr, false);
    playlistwin->put_widget (false, playlistwin_time_min, w - 82, h - 15);
    playlistwin_time_min->on_press (change_timer_mode_cb);

    playlistwin_time_sec = new TextBox (10, nullptr, false);
    playlistwin->put_widget (false, playlistwin_time_sec, w - 64, h - 15);
    playlistwin_time_sec->on_press (change_timer_mode_cb);

    playlistwin_info = new TextBox (90, nullptr, false);
    playlistwin->put_widget (false, playlistwin_info, w - 143, h - 28);

    /* mini play control buttons at right bottom corner */

    playlistwin_srew = new Button (8, 7);
    playlistwin->put_widget (false, playlistwin_srew, w - 144, h - 16);
    playlistwin_srew->on_release ((ButtonCB) aud_drct_pl_prev);

    playlistwin_splay = new Button (10, 7);
    playlistwin->put_widget (false, playlistwin_splay, w - 138, h - 16);
    playlistwin_splay->on_release ((ButtonCB) aud_drct_play);

    playlistwin_spause = new Button (10, 7);
    playlistwin->put_widget (false, playlistwin_spause, w - 128, h - 16);
    playlistwin_spause->on_release ((ButtonCB) aud_drct_pause);

    playlistwin_sstop = new Button (9, 7);
    playlistwin->put_widget (false, playlistwin_sstop, w - 118, h - 16);
    playlistwin_sstop->on_release ((ButtonCB) aud_drct_stop);

    playlistwin_sfwd = new Button (8, 7);
    playlistwin->put_widget (false, playlistwin_sfwd, w - 109, h - 16);
    playlistwin_sfwd->on_release ((ButtonCB) aud_drct_pl_next);

    playlistwin_seject = new Button (9, 7);
    playlistwin->put_widget (false, playlistwin_seject, w - 100, h - 16);
    playlistwin_seject->on_release ((ButtonCB) action_play_file);

    playlistwin_sscroll_up = new Button (8, 5);
    playlistwin->put_widget (false, playlistwin_sscroll_up, w - 14, h - 35);
    playlistwin_sscroll_up->on_release ((ButtonCB) playlistwin_scroll_up_pushed);

    playlistwin_sscroll_down = new Button (8, 5);
    playlistwin->put_widget (false, playlistwin_sscroll_down, w - 14, h - 30);
    playlistwin_sscroll_down->on_release ((ButtonCB) playlistwin_scroll_down_pushed);

    /* resize handles */

    resize_handle = new DragHandle (20, 20, resize_press, resize_drag);
    playlistwin->put_widget (false, resize_handle, w - 20, h - 20);

    sresize_handle = new DragHandle (9, PLAYLISTWIN_SHADED_HEIGHT, resize_press, resize_drag);
    playlistwin->put_widget (true, sresize_handle, w - 31, 0);

    /* lower button row */

    button_add = new Button (25, 18);
    playlistwin->put_widget (false, button_add, 12, h - 29);
    button_add->on_press (button_add_cb);

    button_sub = new Button (25, 18);
    playlistwin->put_widget (false, button_sub, 40, h - 29);
    button_sub->on_press (button_sub_cb);

    button_sel = new Button (25, 18);
    playlistwin->put_widget (false, button_sel, 68, h - 29);
    button_sel->on_press (button_sel_cb);

    button_misc = new Button (25, 18);
    playlistwin->put_widget (false, button_misc, 100, h - 29);
    button_misc->on_press (button_misc_cb);

    button_list = new Button (23, 18);
    playlistwin->put_widget (false, button_list, w - 46, h - 29);
    button_list->on_press (button_list_cb);
}

void PlWindow::draw (cairo_t * cr)
{
    if (is_shaded ())
        skin_draw_playlistwin_shaded (cr, config.playlist_width, true);
    else
        skin_draw_playlistwin_frame (cr, config.playlist_width,
         config.playlist_height, true);
}

static void playlistwin_create_window ()
{
    bool shaded = aud_get_bool ("skins", "playlist_shaded");

    playlistwin = new PlWindow (shaded);
    playlistwin->setWindowTitle (_("Audacious Playlist Editor"));

    GtkWidget * w = playlistwin->gtk ();
    drag_dest_set (w);
    drop_position = -1;

    g_signal_connect (w, "drag-motion", (GCallback) drag_motion, nullptr);
    g_signal_connect (w, "drag-leave", (GCallback) drag_leave, nullptr);
    g_signal_connect (w, "drag-drop", (GCallback) drag_drop, nullptr);
    g_signal_connect (w, "drag-data-received", (GCallback) drag_data_received, nullptr);
}

static void update_cb (void *, void *)
{
    playlistwin_list->refresh ();

    if (song_changed)
    {
        playlistwin_list->set_focused (Playlist::active_playlist ().get_position ());
        song_changed = false;
    }

    update_info ();
    update_rollup_text ();
}

static void follow_cb (void * data, void *)
{
    auto list = aud::from_ptr<Playlist> (data);
    list.select_all (false);

    int row = list.get_position ();
    if (row >= 0)
        list.select_entry (row, true);

    if (list == Playlist::active_playlist ())
        song_changed = true;
}

void playlistwin_create ()
{
    playlistwin_create_window ();
    playlistwin_create_widgets ();

    update_info ();
    update_rollup_text ();

    song_changed = false;

    hook_associate ("playlist position", follow_cb, nullptr);
    hook_associate ("playlist activate", update_cb, nullptr);
    hook_associate ("playlist update", update_cb, nullptr);
}

void playlistwin_unhook ()
{
    hook_dissociate ("playlist position", follow_cb);
    hook_dissociate ("playlist activate", update_cb);
    hook_dissociate ("playlist update", update_cb);
}
