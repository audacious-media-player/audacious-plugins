/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007 Tomasz Moń
 * Copyright (c) 2008 William Pitcock
 * Copyright (c) 2009-2011 John Lindgren
 *
 * Based on:
 * BMP - Cross-platform multimedia player
 * Copyright (C) 2003-2004  BMP development team.
 *
 * XMMS:
 * Copyright (C) 1998-2003  XMMS development team.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 * The Audacious team does not consider modular code linking to
 * Audacious or using our public API to be a derived work.
 */

#include <gdk/gdkkeysyms.h>

#include "draw-compat.h"
#include "menus.h"
#include "skins_cfg.h"
#include "ui_playlist.h"
#include "ui_skin.h"
#include "ui_skinned_playlist.h"
#include "ui_skinned_playlist_slider.h"

#include <audacious/drct.h>
#include <libaudcore/runtime.h>
#include <audacious/playlist.h>
#include <libaudgui/libaudgui.h>

enum {DRAG_SELECT = 1, DRAG_MOVE};

typedef struct {
    GtkWidget * slider;
    PangoFontDescription * font;
    gint width, height, row_height, offset, rows, first, scroll, scroll_source,
     hover, drag;
    gint popup_pos, popup_source;
    gboolean popup_shown;
} PlaylistData;

static gboolean playlist_button_press (GtkWidget * list, GdkEventButton * event);
static gboolean playlist_button_release (GtkWidget * list, GdkEventButton *
 event);
static gboolean playlist_motion (GtkWidget * list, GdkEventMotion * event);
static gboolean playlist_leave (GtkWidget * list, GdkEventCrossing * event);

static void popup_trigger (GtkWidget * list, PlaylistData * data, gint pos);
static void popup_hide (GtkWidget * list, PlaylistData * data);

static void calc_layout (PlaylistData * data)
{
    data->rows = data->height / data->row_height;

    if (data->rows && active_title)
    {
        data->offset = data->row_height;
        data->rows --;
    }
    else
        data->offset = 0;

    if (data->first + data->rows > active_length)
        data->first = active_length - data->rows;
    if (data->first < 0)
        data->first = 0;
}

static gint calc_position (PlaylistData * data, gint y)
{
    if (y < data->offset)
        return -1;

    gint position = data->first + (y - data->offset) / data->row_height;

    if (position >= data->first + data->rows || position >= active_length)
        return active_length;

    return position;
}

static gint adjust_position (PlaylistData * data, gboolean relative, gint
 position)
{
    if (active_length == 0)
        return -1;

    if (relative)
    {
        gint focus = aud_playlist_get_focus (active_playlist);
        if (focus == -1)
            return 0;

        position += focus;
    }

    if (position < 0)
        return 0;
    if (position >= active_length)
        return active_length - 1;

    return position;
}

static void cancel_all (GtkWidget * list, PlaylistData * data)
{
    data->drag = FALSE;

    if (data->scroll)
    {
        data->scroll = 0;
        g_source_remove (data->scroll_source);
    }

    if (data->hover != -1)
    {
        data->hover = -1;
        gtk_widget_queue_draw (list);
    }

    popup_hide (list, data);
}

DRAW_FUNC_BEGIN (playlist_draw)
    PlaylistData * data = g_object_get_data ((GObject *) wid, "playlistdata");
    g_return_val_if_fail (data, FALSE);

    gint active_entry = aud_playlist_get_position (active_playlist);
    gint left = 3, right = 3;
    PangoLayout * layout;
    gint width;

    /* background */

    set_cairo_color (cr, active_skin->colors[SKIN_PLEDIT_NORMALBG]);
    cairo_paint (cr);

    /* playlist title */

    if (data->offset)
    {
        layout = gtk_widget_create_pango_layout (wid, active_title);
        pango_layout_set_font_description (layout, data->font);
        pango_layout_set_width (layout, PANGO_SCALE * (data->width - left -
         right));
        pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
        pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_MIDDLE);

        cairo_move_to (cr, left, 0);
        set_cairo_color (cr, active_skin->colors[SKIN_PLEDIT_NORMAL]);
        pango_cairo_show_layout (cr, layout);
        g_object_unref (layout);
    }

    /* selection highlight */

    for (gint i = data->first; i < data->first + data->rows && i <
     active_length; i ++)
    {
        if (! aud_playlist_entry_get_selected (active_playlist, i))
            continue;

        cairo_rectangle (cr, 0, data->offset + data->row_height * (i -
         data->first), data->width, data->row_height);
        set_cairo_color (cr, active_skin->colors[SKIN_PLEDIT_SELECTEDBG]);
        cairo_fill (cr);
    }

    /* entry numbers */

    if (aud_get_bool (NULL, "show_numbers_in_pl"))
    {
        width = 0;

        for (gint i = data->first; i < data->first + data->rows && i <
         active_length; i ++)
        {
            gchar buf[16];
            snprintf (buf, sizeof buf, "%d.", 1 + i);

            layout = gtk_widget_create_pango_layout (wid, buf);
            pango_layout_set_font_description (layout, data->font);

            PangoRectangle rect;
            pango_layout_get_pixel_extents (layout, NULL, & rect);
            width = MAX (width, rect.width);

            cairo_move_to (cr, left, data->offset + data->row_height * (i -
             data->first));
            set_cairo_color (cr, active_skin->colors[(i == active_entry) ?
             SKIN_PLEDIT_CURRENT : SKIN_PLEDIT_NORMAL]);
            pango_cairo_show_layout (cr, layout);
            g_object_unref (layout);
        }

        left += width + 4;
    }

    /* entry lengths */

    width = 0;

    for (gint i = data->first; i < data->first + data->rows && i <
     active_length; i ++)
    {
        int len = aud_playlist_entry_get_length (active_playlist, i, TRUE);
        if (len <= 0)
            continue;

        char buf[16];
        audgui_format_time (buf, sizeof buf, len);

        layout = gtk_widget_create_pango_layout (wid, buf);
        pango_layout_set_font_description (layout, data->font);

        PangoRectangle rect;
        pango_layout_get_pixel_extents (layout, NULL, & rect);
        width = MAX (width, rect.width);

        cairo_move_to (cr, data->width - right - rect.width, data->offset +
         data->row_height * (i - data->first));
        set_cairo_color (cr, active_skin->colors[(i == active_entry) ?
         SKIN_PLEDIT_CURRENT : SKIN_PLEDIT_NORMAL]);
        pango_cairo_show_layout (cr, layout);
        g_object_unref (layout);
    }

    right += width + 6;

    /* queue positions */

    if (aud_playlist_queue_count (active_playlist))
    {
        width = 0;

        for (gint i = data->first; i < data->first + data->rows && i <
         active_length; i ++)
        {
            gint pos = aud_playlist_queue_find_entry (active_playlist, i);
            if (pos < 0)
                continue;

            gchar buf[16];
            snprintf (buf, sizeof buf, "(#%d)", 1 + pos);

            layout = gtk_widget_create_pango_layout (wid, buf);
            pango_layout_set_font_description (layout, data->font);

            PangoRectangle rect;
            pango_layout_get_pixel_extents (layout, NULL, & rect);
            width = MAX (width, rect.width);

            cairo_move_to (cr, data->width - right - rect.width, data->offset +
             data->row_height * (i - data->first));
            set_cairo_color (cr, active_skin->colors[(i == active_entry) ?
             SKIN_PLEDIT_CURRENT : SKIN_PLEDIT_NORMAL]);
            pango_cairo_show_layout (cr, layout);
            g_object_unref (layout);
        }

        right += width + 6;
    }

    /* titles */

    for (gint i = data->first; i < data->first + data->rows && i <
     active_length; i ++)
    {
        gchar * title = aud_playlist_entry_get_title (active_playlist, i, TRUE);

        layout = gtk_widget_create_pango_layout (wid, title);
        pango_layout_set_font_description (layout, data->font);
        pango_layout_set_width (layout, PANGO_SCALE * (data->width - left -
         right));
        pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);

        str_unref (title);

        cairo_move_to (cr, left, data->offset + data->row_height * (i -
         data->first));
        set_cairo_color (cr, active_skin->colors[(i == active_entry) ?
         SKIN_PLEDIT_CURRENT : SKIN_PLEDIT_NORMAL]);
        pango_cairo_show_layout (cr, layout);
        g_object_unref (layout);
    }

    /* focus rectangle */

    gint focus = aud_playlist_get_focus (active_playlist);
    if (focus >= data->first && focus <= data->first + data->rows - 1)
    {
        cairo_new_path (cr);
        cairo_set_line_width (cr, 1);
        cairo_rectangle (cr, 0.5, data->offset + data->row_height * (focus -
         data->first) + 0.5, data->width - 1, data->row_height - 1);
        set_cairo_color (cr, active_skin->colors[SKIN_PLEDIT_NORMAL]);
        cairo_stroke (cr);
    }

    /* hover line */

    if (data->hover >= data->first && data->hover <= data->first + data->rows)
    {
        cairo_new_path (cr);
        cairo_set_line_width (cr, 2);
        cairo_move_to (cr, 0, data->offset + data->row_height * (data->hover -
         data->first));
        cairo_rel_line_to (cr, data->width, 0);
        set_cairo_color (cr, active_skin->colors[SKIN_PLEDIT_NORMAL]);
        cairo_stroke (cr);
    }
DRAW_FUNC_END

static void playlist_destroy (GtkWidget * list)
{
    PlaylistData * data = g_object_get_data ((GObject *) list, "playlistdata");
    g_return_if_fail (data);

    cancel_all (list, data);

    pango_font_description_free (data->font);
    g_free (data);
}

GtkWidget * ui_skinned_playlist_new (gint width, gint height, const gchar * font)
{
    GtkWidget * list = gtk_drawing_area_new ();
    gtk_widget_set_size_request (list, width, height);
    gtk_widget_add_events (list, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
     | GDK_LEAVE_NOTIFY_MASK | GDK_POINTER_MOTION_MASK);

    DRAW_CONNECT (list, playlist_draw);
    g_signal_connect (list, "button-press-event", (GCallback)
     playlist_button_press, NULL);
    g_signal_connect (list, "button-release-event", (GCallback)
     playlist_button_release, NULL);
    g_signal_connect (list, "leave-notify-event", (GCallback) playlist_leave,
     NULL);
    g_signal_connect (list, "motion-notify-event", (GCallback) playlist_motion,
     NULL);
    g_signal_connect (list, "destroy", (GCallback) playlist_destroy, NULL);

    PlaylistData * data = g_malloc0 (sizeof (PlaylistData));
    data->width = width;
    data->height = height;
    data->hover = -1;
    data->popup_pos = -1;
    g_object_set_data ((GObject *) list, "playlistdata", data);

    ui_skinned_playlist_set_font (list, font);

    return list;
}

void ui_skinned_playlist_set_slider (GtkWidget * list, GtkWidget * slider)
{
    PlaylistData * data = g_object_get_data ((GObject *) list, "playlistdata");
    g_return_if_fail (data);

    data->slider = slider;
}

void ui_skinned_playlist_resize (GtkWidget * list, gint width, gint height)
{
    PlaylistData * data = g_object_get_data ((GObject *) list, "playlistdata");
    g_return_if_fail (data);

    gtk_widget_set_size_request (list, width, height);

    data->width = width;
    data->height = height;

    calc_layout (data);
    gtk_widget_queue_draw (list);

    if (data->slider)
        ui_skinned_playlist_slider_update (data->slider);
}

void ui_skinned_playlist_set_font (GtkWidget * list, const gchar * font)
{
    PlaylistData * data = g_object_get_data ((GObject *) list, "playlistdata");
    g_return_if_fail (data);

    pango_font_description_free (data->font);
    data->font = pango_font_description_from_string (font);

    PangoLayout * layout = gtk_widget_create_pango_layout (list, "A");
    pango_layout_set_font_description (layout, data->font);

    PangoRectangle rect;
    pango_layout_get_pixel_extents (layout, NULL, & rect);

    /* make sure row_height is non-zero; we divide by it */
    data->row_height = MAX (rect.height, 1);

    g_object_unref (layout);

    calc_layout (data);
    gtk_widget_queue_draw (list);

    if (data->slider)
        ui_skinned_playlist_slider_update (data->slider);
}

void ui_skinned_playlist_update (GtkWidget * list)
{
    PlaylistData * data = g_object_get_data ((GObject *) list, "playlistdata");
    g_return_if_fail (data);

    calc_layout (data);

    gtk_widget_queue_draw (list);

    if (data->slider != NULL)
        ui_skinned_playlist_slider_update (data->slider);
}

static void scroll_to (PlaylistData * data, gint position)
{
    if (position < data->first || position >= data->first + data->rows)
        data->first = position - data->rows / 2;

    calc_layout (data);
}

static void select_single (PlaylistData * data, gboolean relative, gint position)
{
    position = adjust_position (data, relative, position);

    if (position == -1)
        return;

    aud_playlist_select_all (active_playlist, FALSE);
    aud_playlist_entry_set_selected (active_playlist, position, TRUE);
    aud_playlist_set_focus (active_playlist, position);
    scroll_to (data, position);
}

static void select_extend (PlaylistData * data, gboolean relative, gint position)
{
    position = adjust_position (data, relative, position);

    if (position == -1)
        return;

    gint count = adjust_position (data, TRUE, 0);
    gint sign = (position > count) ? 1 : -1;

    for (; count != position; count += sign)
        aud_playlist_entry_set_selected (active_playlist, count,
         ! aud_playlist_entry_get_selected (active_playlist, count + sign));

    aud_playlist_entry_set_selected (active_playlist, position, TRUE);
    aud_playlist_set_focus (active_playlist, position);
    scroll_to (data, position);
}

static void select_slide (PlaylistData * data, gboolean relative, gint position)
{
    position = adjust_position (data, relative, position);

    if (position == -1)
        return;

    aud_playlist_set_focus (active_playlist, position);
    scroll_to (data, position);
}

static void select_toggle (PlaylistData * data, gboolean relative, gint position)
{
    position = adjust_position (data, relative, position);

    if (position == -1)
        return;

    aud_playlist_entry_set_selected (active_playlist, position,
     ! aud_playlist_entry_get_selected (active_playlist, position));
    aud_playlist_set_focus (active_playlist, position);
    scroll_to (data, position);
}

static void select_move (PlaylistData * data, gboolean relative, gint position)
{
    gint focus = aud_playlist_get_focus (active_playlist);
    position = adjust_position (data, relative, position);

    if (focus == -1 || position == -1 || position == focus)
        return;

    focus += aud_playlist_shift (active_playlist, focus, position - focus);
    scroll_to (data, focus);
}

static void delete_selected (PlaylistData * data)
{
    aud_playlist_delete_selected (active_playlist);

    active_length = aud_playlist_entry_count (active_playlist);
    gint focus = aud_playlist_get_focus (active_playlist);

    if (focus != -1)
    {
        aud_playlist_entry_set_selected (active_playlist, focus, TRUE);
        scroll_to (data, focus);
    }
}

gboolean ui_skinned_playlist_key (GtkWidget * list, GdkEventKey * event)
{
    PlaylistData * data = g_object_get_data ((GObject *) list, "playlistdata");
    g_return_val_if_fail (data, FALSE);

    cancel_all (list, data);

    switch (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK))
    {
      case 0:
        switch (event->keyval)
        {
          case GDK_KEY_Up:
            select_single (data, TRUE, -1);
            break;
          case GDK_KEY_Down:
            select_single (data, TRUE, 1);
            break;
          case GDK_KEY_Page_Up:
            select_single (data, TRUE, - data->rows);
            break;
          case GDK_KEY_Page_Down:
            select_single (data, TRUE, data->rows);
            break;
          case GDK_KEY_Home:
            select_single (data, FALSE, 0);
            break;
          case GDK_KEY_End:
            select_single (data, FALSE, active_length - 1);
            break;
          case GDK_KEY_Return:
            select_single (data, TRUE, 0);
            aud_playlist_set_position (active_playlist,
             aud_playlist_get_focus (active_playlist));
            aud_drct_play_playlist (active_playlist);
            break;
          case GDK_KEY_Escape:
            select_single (data, FALSE, aud_playlist_get_position
             (active_playlist));
            break;
          case GDK_KEY_Delete:
            delete_selected (data);
            break;
          default:
            return FALSE;
        }
        break;
      case GDK_SHIFT_MASK:
        switch (event->keyval)
        {
          case GDK_KEY_Up:
            select_extend (data, TRUE, -1);
            break;
          case GDK_KEY_Down:
            select_extend (data, TRUE, 1);
            break;
          case GDK_KEY_Page_Up:
            select_extend (data, TRUE, -data->rows);
            break;
          case GDK_KEY_Page_Down:
            select_extend (data, TRUE, data->rows);
            break;
          case GDK_KEY_Home:
            select_extend (data, FALSE, 0);
            break;
          case GDK_KEY_End:
            select_extend (data, FALSE, active_length - 1);
            break;
          default:
            return FALSE;
        }
        break;
      case GDK_CONTROL_MASK:
        switch (event->keyval)
        {
          case GDK_KEY_space:
            select_toggle (data, TRUE, 0);
            break;
          case GDK_KEY_Up:
            select_slide (data, TRUE, -1);
            break;
          case GDK_KEY_Down:
            select_slide (data, TRUE, 1);
            break;
          case GDK_KEY_Page_Up:
            select_slide (data, TRUE, -data->rows);
            break;
          case GDK_KEY_Page_Down:
            select_slide (data, TRUE, data->rows);
            break;
          case GDK_KEY_Home:
            select_slide (data, FALSE, 0);
            break;
          case GDK_KEY_End:
            select_slide (data, FALSE, active_length - 1);
            break;
          default:
            return FALSE;
        }
        break;
      case GDK_MOD1_MASK:
        switch (event->keyval)
        {
          case GDK_KEY_Up:
            select_move (data, TRUE, -1);
            break;
          case GDK_KEY_Down:
            select_move (data, TRUE, 1);
            break;
          case GDK_KEY_Page_Up:
            select_move (data, TRUE, -data->rows);
            break;
          case GDK_KEY_Page_Down:
            select_move (data, TRUE, data->rows);
            break;
          case GDK_KEY_Home:
            select_move (data, FALSE, 0);
            break;
          case GDK_KEY_End:
            select_move (data, FALSE, active_length - 1);
            break;
          default:
            return FALSE;
        }
        break;
      default:
        return FALSE;
    }

    playlistwin_update ();
    return TRUE;
}

void ui_skinned_playlist_row_info (GtkWidget * list, gint * rows, gint * first)
{
    PlaylistData * data = g_object_get_data ((GObject *) list, "playlistdata");
    g_return_if_fail (data);

    * rows = data->rows;
    * first = data->first;
}

void ui_skinned_playlist_scroll_to (GtkWidget * list, gint row)
{
    PlaylistData * data = g_object_get_data ((GObject *) list, "playlistdata");
    g_return_if_fail (data);

    cancel_all (list, data);
    data->first = row;
    calc_layout (data);

    gtk_widget_queue_draw (list);

    if (data->slider)
        ui_skinned_playlist_slider_update (data->slider);
}

void ui_skinned_playlist_set_focused (GtkWidget * list, gint row)
{
    PlaylistData * data = g_object_get_data ((GObject *) list, "playlistdata");
    g_return_if_fail (data);

    cancel_all (list, data);
    aud_playlist_set_focus (active_playlist, row);
    scroll_to (data, row);

    gtk_widget_queue_draw (list);
}

void ui_skinned_playlist_hover (GtkWidget * list, gint x, gint y)
{
    PlaylistData * data = g_object_get_data ((GObject *) list, "playlistdata");
    g_return_if_fail (data);

    gint new;

    if (y < data->offset)
        new = data->first;
    else if (y > data->offset + data->row_height * data->rows)
        new = data->first + data->rows;
    else
        new = data->first + (y - data->offset + data->row_height / 2) /
         data->row_height;

    if (new > active_length)
        new = active_length;

    if (new != data->hover)
    {
        data->hover = new;
        gtk_widget_queue_draw (list);
    }
}

gint ui_skinned_playlist_hover_end (GtkWidget * list)
{
    PlaylistData * data = g_object_get_data ((GObject *) list, "playlistdata");
    g_return_val_if_fail (data, -1);

    gint temp = data->hover;
    data->hover = -1;

    gtk_widget_queue_draw (list);
    return temp;
}

static gboolean playlist_button_press (GtkWidget * list, GdkEventButton * event)
{
    PlaylistData * data = g_object_get_data ((GObject *) list, "playlistdata");
    g_return_val_if_fail (data, FALSE);

    gint position = calc_position (data, event->y);
    gint state = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK |
     GDK_MOD1_MASK);

    cancel_all (list, data);

    switch (event->type)
    {
      case GDK_BUTTON_PRESS:
        switch (event->button)
        {
          case 1:
            if (position == -1 || position == active_length)
                return TRUE;

            switch (state)
            {
              case 0:
                if (aud_playlist_entry_get_selected (active_playlist, position))
                    select_slide (data, FALSE, position);
                else
                    select_single (data, FALSE, position);

                data->drag = DRAG_MOVE;
                break;
              case GDK_SHIFT_MASK:
                select_extend (data, FALSE, position);
                data->drag = DRAG_SELECT;
                break;
              case GDK_CONTROL_MASK:
                select_toggle (data, FALSE, position);
                data->drag = DRAG_SELECT;
                break;
              default:
                return TRUE;
            }

            break;
          case 3:
            if (state)
                return TRUE;

            if (position != -1 && position != active_length)
            {
                if (aud_playlist_entry_get_selected (active_playlist, position))
                    select_slide (data, FALSE, position);
                else
                    select_single (data, FALSE, position);
            }

            menu_popup ((position == -1) ? UI_MENU_PLAYLIST :
             UI_MENU_PLAYLIST_CONTEXT, event->x_root, event->y_root, FALSE,
             FALSE, 3, event->time);
            break;
          default:
            return FALSE;
        }

        break;
      case GDK_2BUTTON_PRESS:
        if (event->button != 1 || state || position == active_length)
            return TRUE;

        if (position != -1)
            aud_playlist_set_position (active_playlist, position);

        aud_drct_play_playlist (active_playlist);
        break;
      default:
        return TRUE;
    }

    playlistwin_update ();
    return TRUE;
}

static gboolean playlist_button_release (GtkWidget * list, GdkEventButton *
 event)
{
    PlaylistData * data = g_object_get_data ((GObject *) list, "playlistdata");
    g_return_val_if_fail (data, FALSE);

    cancel_all (list, data);
    return TRUE;
}

static gboolean scroll_cb (PlaylistData * data)
{
    gint position = adjust_position (data, TRUE, data->scroll);

    if (position == -1)
        return TRUE;

    switch (data->drag)
    {
      case DRAG_SELECT:
        select_extend (data, FALSE, position);
        break;
      case DRAG_MOVE:
        select_move (data, FALSE, position);
        break;
    }

    playlistwin_update ();
    return TRUE;
}

static gboolean playlist_motion (GtkWidget * list, GdkEventMotion * event)
{
    PlaylistData * data = g_object_get_data ((GObject *) list, "playlistdata");
    g_return_val_if_fail (data, FALSE);

    gint position = calc_position (data, event->y);
    gint new_scroll;

    if (data->drag)
    {
        if (position == -1 || position == active_length)
        {
            new_scroll = (position == -1 ? -1 : 1);

            if (data->scroll != new_scroll)
            {
                if (data->scroll)
                    g_source_remove (data->scroll_source);

                data->scroll = new_scroll;
                data->scroll_source = g_timeout_add (100, (GSourceFunc)
                 scroll_cb, data);
            }
        }
        else
        {
            if (data->scroll)
            {
                data->scroll = 0;
                g_source_remove (data->scroll_source);
            }

            switch (data->drag)
            {
              case DRAG_SELECT:
                select_extend (data, FALSE, position);
                break;
              case DRAG_MOVE:
                select_move (data, FALSE, position);
                break;
            }

            playlistwin_update ();
        }
    }
    else
    {
        if (position == -1 || position == active_length)
            cancel_all (list, data);
        else if (aud_get_bool (NULL, "show_filepopup_for_tuple") && data->popup_pos != position)
        {
            cancel_all (list, data);
            popup_trigger (list, data, position);
        }
    }

    return TRUE;
}

static gboolean playlist_leave (GtkWidget * list, GdkEventCrossing * event)
{
    PlaylistData * data = g_object_get_data ((GObject *) list, "playlistdata");
    g_return_val_if_fail (data, FALSE);

    if (! data->drag)
        cancel_all (list, data);

    return TRUE;
}

static gboolean popup_show (GtkWidget * list)
{
    PlaylistData * data = g_object_get_data ((GObject *) list, "playlistdata");
    g_return_val_if_fail (data, FALSE);

    audgui_infopopup_show (active_playlist, data->popup_pos);
    data->popup_shown = TRUE;

    g_source_remove (data->popup_source);
    data->popup_source = 0;
    return FALSE;
}

static void popup_trigger (GtkWidget * list, PlaylistData * data, gint pos)
{
    popup_hide (list, data);

    data->popup_pos = pos;
    data->popup_source = g_timeout_add (aud_get_int (NULL, "filepopup_delay") *
     100, (GSourceFunc) popup_show, list);
}

static void popup_hide (GtkWidget * list, PlaylistData * data)
{
    if (data->popup_source)
    {
        g_source_remove (data->popup_source);
        data->popup_source = 0;
    }

    if (data->popup_shown)
    {
        audgui_infopopup_hide ();
        data->popup_shown = FALSE;
    }

    data->popup_pos = -1;
}
