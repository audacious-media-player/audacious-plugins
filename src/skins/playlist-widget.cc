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

#include "menus.h"
#include "skins_cfg.h"
#include "skin.h"
#include "playlist-widget.h"
#include "playlist-slider.h"

#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/playlist.h>
#include <libaudgui/libaudgui.h>

enum {
    DRAG_SELECT = 1,
    DRAG_MOVE
};

void PlaylistWidget::update_title ()
{
    if (Playlist::n_playlists () > 1)
    {
        String title = m_playlist.get_title ();
        m_title_text = String (str_printf (_("%s (%d of %d)"),
         (const char *) title, 1 + m_playlist.index (), Playlist::n_playlists ()));
    }
    else
        m_title_text = String ();
}

void PlaylistWidget::calc_layout ()
{
    m_rows = m_height / m_row_height;

    if (m_rows && m_title_text)
    {
        m_offset = m_row_height;
        m_rows --;
    }
    else
        m_offset = 0;

    if (m_first + m_rows > m_length)
        m_first = m_length - m_rows;
    if (m_first < 0)
        m_first = 0;
}

int PlaylistWidget::calc_position (int y) const
{
    if (y < m_offset)
        return -1;

    int position = m_first + (y - m_offset) / m_row_height;
    if (position >= m_first + m_rows || position >= m_length)
        return m_length;

    return position;
}

int PlaylistWidget::adjust_position (bool relative, int position) const
{
    if (m_length == 0)
        return -1;

    if (relative)
    {
        int focus = m_playlist.get_focus ();
        if (focus == -1)
            return 0;

        position += focus;
    }

    if (position < 0)
        return 0;
    if (position >= m_length)
        return m_length - 1;

    return position;
}

void PlaylistWidget::cancel_all ()
{
    m_drag = false;

    if (m_scroll)
    {
        m_scroll = 0;
        scroll_timer.stop ();
    }

    if (m_hover != -1)
    {
        m_hover = -1;
        queue_draw ();
    }

    popup_hide ();
}

void PlaylistWidget::draw (cairo_t * cr)
{
    int active_entry = m_playlist.get_position ();
    int left = 3, right = 3;
    PangoLayout * layout;
    int width;

    /* background */

    set_cairo_color (cr, skin.colors[SKIN_PLEDIT_NORMALBG]);
    cairo_paint (cr);

    /* playlist title */

    if (m_offset)
    {
        layout = gtk_widget_create_pango_layout (gtk_dr (), m_title_text);
        pango_layout_set_font_description (layout, m_font.get ());
        pango_layout_set_width (layout, PANGO_SCALE * (m_width - left - right));
        pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);
        pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_MIDDLE);

        cairo_move_to (cr, left, 0);
        set_cairo_color (cr, skin.colors[SKIN_PLEDIT_NORMAL]);
        pango_cairo_show_layout (cr, layout);
        g_object_unref (layout);
    }

    /* selection highlight */

    for (int i = m_first; i < m_first + m_rows && i < m_length; i ++)
    {
        if (! m_playlist.entry_selected (i))
            continue;

        cairo_rectangle (cr, 0, m_offset + m_row_height * (i - m_first), m_width, m_row_height);
        set_cairo_color (cr, skin.colors[SKIN_PLEDIT_SELECTEDBG]);
        cairo_fill (cr);
    }

    /* entry numbers */

    if (aud_get_bool (nullptr, "show_numbers_in_pl"))
    {
        width = 0;

        for (int i = m_first; i < m_first + m_rows && i < m_length; i ++)
        {
            char buf[16];
            snprintf (buf, sizeof buf, "%d.", 1 + i);

            layout = gtk_widget_create_pango_layout (gtk_dr (), buf);
            pango_layout_set_font_description (layout, m_font.get ());

            PangoRectangle rect;
            pango_layout_get_pixel_extents (layout, nullptr, & rect);
            width = aud::max (width, rect.width);

            cairo_move_to (cr, left, m_offset + m_row_height * (i - m_first));
            set_cairo_color (cr, skin.colors[(i == active_entry) ?
             SKIN_PLEDIT_CURRENT : SKIN_PLEDIT_NORMAL]);
            pango_cairo_show_layout (cr, layout);
            g_object_unref (layout);
        }

        left += width + 4;
    }

    /* entry lengths */

    width = 0;

    for (int i = m_first; i < m_first + m_rows && i < m_length; i ++)
    {
        Tuple tuple = m_playlist.entry_tuple (i, Playlist::NoWait);
        int len = tuple.get_int (Tuple::Length);
        if (len < 0)
            continue;

        layout = gtk_widget_create_pango_layout (gtk_dr (), str_format_time (len));
        pango_layout_set_font_description (layout, m_font.get ());

        PangoRectangle rect;
        pango_layout_get_pixel_extents (layout, nullptr, & rect);
        width = aud::max (width, rect.width);

        cairo_move_to (cr, m_width - right - rect.width, m_offset + m_row_height * (i - m_first));
        set_cairo_color (cr, skin.colors[(i == active_entry) ?
         SKIN_PLEDIT_CURRENT : SKIN_PLEDIT_NORMAL]);
        pango_cairo_show_layout (cr, layout);
        g_object_unref (layout);
    }

    right += width + 6;

    /* queue positions */

    if (m_playlist.n_queued ())
    {
        width = 0;

        for (int i = m_first; i < m_first + m_rows && i < m_length; i ++)
        {
            int pos = m_playlist.queue_find_entry (i);
            if (pos < 0)
                continue;

            char buf[16];
            snprintf (buf, sizeof buf, "(#%d)", 1 + pos);

            layout = gtk_widget_create_pango_layout (gtk_dr (), buf);
            pango_layout_set_font_description (layout, m_font.get ());

            PangoRectangle rect;
            pango_layout_get_pixel_extents (layout, nullptr, & rect);
            width = aud::max (width, rect.width);

            cairo_move_to (cr, m_width - right - rect.width, m_offset +
             m_row_height * (i - m_first));
            set_cairo_color (cr, skin.colors[(i == active_entry) ?
             SKIN_PLEDIT_CURRENT : SKIN_PLEDIT_NORMAL]);
            pango_cairo_show_layout (cr, layout);
            g_object_unref (layout);
        }

        right += width + 6;
    }

    /* titles */

    for (int i = m_first; i < m_first + m_rows && i < m_length; i ++)
    {
        Tuple tuple = m_playlist.entry_tuple (i, Playlist::NoWait);
        String title = tuple.get_str (Tuple::FormattedTitle);

        layout = gtk_widget_create_pango_layout (gtk_dr (), title);
        pango_layout_set_font_description (layout, m_font.get ());
        pango_layout_set_width (layout, PANGO_SCALE * (m_width - left - right));
        pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);

        cairo_move_to (cr, left, m_offset + m_row_height * (i - m_first));
        set_cairo_color (cr, skin.colors[(i == active_entry) ?
         SKIN_PLEDIT_CURRENT : SKIN_PLEDIT_NORMAL]);
        pango_cairo_show_layout (cr, layout);
        g_object_unref (layout);
    }

    /* focus rectangle */

    int focus = m_playlist.get_focus ();

    /* don't show rectangle if this is the only selected entry */
    if (focus >= m_first && focus <= m_first + m_rows - 1 &&
        (! m_playlist.entry_selected (focus) || m_playlist.n_selected () > 1))
    {
        cairo_new_path (cr);
        cairo_set_line_width (cr, 1);
        cairo_rectangle (cr, 0.5, m_offset + m_row_height * (focus - m_first) +
         0.5, m_width - 1, m_row_height - 1);
        set_cairo_color (cr, skin.colors[SKIN_PLEDIT_NORMAL]);
        cairo_stroke (cr);
    }

    /* hover line */

    if (m_hover >= m_first && m_hover <= m_first + m_rows)
    {
        cairo_new_path (cr);
        cairo_set_line_width (cr, 2);
        cairo_move_to (cr, 0, m_offset + m_row_height * (m_hover - m_first));
        cairo_rel_line_to (cr, m_width, 0);
        set_cairo_color (cr, skin.colors[SKIN_PLEDIT_NORMAL]);
        cairo_stroke (cr);
    }
}

PlaylistWidget::PlaylistWidget (int width, int height, const char * font) :
    m_width (width * config.scale),
    m_height (height * config.scale)
{
    add_input (m_width, m_height, true, true);
    set_font (font);  /* calls refresh() */
}

void PlaylistWidget::resize (int width, int height)
{
    m_width = width * config.scale;
    m_height = height * config.scale;

    Widget::resize (m_width, m_height);
    refresh ();
}

void PlaylistWidget::set_font (const char * font)
{
    m_font.capture (pango_font_description_from_string (font));

    PangoLayout * layout = gtk_widget_create_pango_layout (gtk_dr (), "A");
    pango_layout_set_font_description (layout, m_font.get ());

    PangoRectangle rect;
    pango_layout_get_pixel_extents (layout, nullptr, & rect);

    /* make sure row_height is non-zero; we divide by it */
    m_row_height = aud::max (rect.height, 1);

    g_object_unref (layout);
    refresh ();
}

void PlaylistWidget::refresh ()
{
    auto prev_playlist = m_playlist;
    m_playlist = Playlist::active_playlist ();
    m_length = m_playlist.n_entries ();

    update_title ();
    calc_layout ();

    if (m_playlist != prev_playlist)
    {
        cancel_all ();
        m_first = 0;
        ensure_visible (m_playlist.get_focus ());
    }

    queue_draw ();

    if (m_slider)
        m_slider->refresh ();
}

void PlaylistWidget::ensure_visible (int position)
{
    if (position < m_first || position >= m_first + m_rows)
        m_first = position - m_rows / 2;

    calc_layout ();
}

void PlaylistWidget::select_single (bool relative, int position)
{
    position = adjust_position (relative, position);

    if (position == -1)
        return;

    m_playlist.select_all (false);
    m_playlist.select_entry (position, true);
    m_playlist.set_focus (position);
    ensure_visible (position);
}

void PlaylistWidget::select_extend (bool relative, int position)
{
    position = adjust_position (relative, position);

    if (position == -1)
        return;

    int count = adjust_position (true, 0);
    int sign = (position > count) ? 1 : -1;

    for (; count != position; count += sign)
        m_playlist.select_entry (count, ! m_playlist.entry_selected (count + sign));

    m_playlist.select_entry (position, true);
    m_playlist.set_focus (position);
    ensure_visible (position);
}

void PlaylistWidget::select_slide (bool relative, int position)
{
    position = adjust_position (relative, position);

    if (position == -1)
        return;

    m_playlist.set_focus (position);
    ensure_visible (position);
}

void PlaylistWidget::select_toggle (bool relative, int position)
{
    position = adjust_position (relative, position);

    if (position == -1)
        return;

    m_playlist.select_entry (position, ! m_playlist.entry_selected (position));
    m_playlist.set_focus (position);
    ensure_visible (position);
}

void PlaylistWidget::select_move (bool relative, int position)
{
    int focus = m_playlist.get_focus ();
    position = adjust_position (relative, position);

    if (focus == -1 || position == -1 || position == focus)
        return;

    focus += m_playlist.shift_entries (focus, position - focus);
    ensure_visible (focus);
}

void PlaylistWidget::delete_selected ()
{
    m_playlist.remove_selected ();

    m_length = m_playlist.n_entries ();
    int focus = m_playlist.get_focus ();

    if (focus != -1)
    {
        m_playlist.select_entry (focus, true);
        ensure_visible (focus);
    }
}

bool PlaylistWidget::handle_keypress (GdkEventKey * event)
{
    cancel_all ();

    switch (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK))
    {
      case 0:
        switch (event->keyval)
        {
          case GDK_KEY_Up:
            select_single (true, -1);
            break;
          case GDK_KEY_Down:
            select_single (true, 1);
            break;
          case GDK_KEY_Page_Up:
            select_single (true, -m_rows);
            break;
          case GDK_KEY_Page_Down:
            select_single (true, m_rows);
            break;
          case GDK_KEY_Home:
            select_single (false, 0);
            break;
          case GDK_KEY_End:
            select_single (false, m_length - 1);
            break;
          case GDK_KEY_Return:
            select_single (true, 0);
            m_playlist.set_position (m_playlist.get_focus ());
            m_playlist.start_playback ();
            break;
          case GDK_KEY_Escape:
            select_single (false, m_playlist.get_position ());
            break;
          case GDK_KEY_Delete:
            delete_selected ();
            break;
          default:
            return false;
        }
        break;
      case GDK_SHIFT_MASK:
        switch (event->keyval)
        {
          case GDK_KEY_Up:
            select_extend (true, -1);
            break;
          case GDK_KEY_Down:
            select_extend (true, 1);
            break;
          case GDK_KEY_Page_Up:
            select_extend (true, -m_rows);
            break;
          case GDK_KEY_Page_Down:
            select_extend (true, m_rows);
            break;
          case GDK_KEY_Home:
            select_extend (false, 0);
            break;
          case GDK_KEY_End:
            select_extend (false, m_length - 1);
            break;
          default:
            return false;
        }
        break;
      case GDK_CONTROL_MASK:
        switch (event->keyval)
        {
          case GDK_KEY_space:
            select_toggle (true, 0);
            break;
          case GDK_KEY_Up:
            select_slide (true, -1);
            break;
          case GDK_KEY_Down:
            select_slide (true, 1);
            break;
          case GDK_KEY_Page_Up:
            select_slide (true, -m_rows);
            break;
          case GDK_KEY_Page_Down:
            select_slide (true, m_rows);
            break;
          case GDK_KEY_Home:
            select_slide (false, 0);
            break;
          case GDK_KEY_End:
            select_slide (false, m_length - 1);
            break;
          default:
            return false;
        }
        break;
      case GDK_MOD1_MASK:
        switch (event->keyval)
        {
          case GDK_KEY_Up:
            select_move (true, -1);
            break;
          case GDK_KEY_Down:
            select_move (true, 1);
            break;
          case GDK_KEY_Page_Up:
            select_move (true, -m_rows);
            break;
          case GDK_KEY_Page_Down:
            select_move (true, m_rows);
            break;
          case GDK_KEY_Home:
            select_move (false, 0);
            break;
          case GDK_KEY_End:
            select_move (false, m_length - 1);
            break;
          default:
            return false;
        }
        break;
      default:
        return false;
    }

    refresh ();
    return true;
}

void PlaylistWidget::row_info (int * rows, int * first)
{
    * rows = m_rows;
    * first = m_first;
}

void PlaylistWidget::scroll_to (int row)
{
    cancel_all ();
    m_first = row;
    refresh ();
}

void PlaylistWidget::set_focused (int row)
{
    cancel_all ();
    m_playlist.set_focus (row);
    ensure_visible (row);
    refresh ();
}

void PlaylistWidget::hover (int x, int y)
{
    int row;

    if (y < m_offset)
        row = m_first;
    else if (y > m_offset + m_row_height * m_rows)
        row = m_first + m_rows;
    else
        row = m_first + (y - m_offset + m_row_height / 2) / m_row_height;

    if (row > m_length)
        row = m_length;

    if (row != m_hover)
    {
        m_hover = row;
        queue_draw ();
    }
}

int PlaylistWidget::hover_end ()
{
    int temp = m_hover;
    m_hover = -1;

    queue_draw ();
    return temp;
}

bool PlaylistWidget::button_press (GdkEventButton * event)
{
    int position = calc_position (event->y);
    int state = event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK |
     GDK_MOD1_MASK);

    cancel_all ();

    switch (event->type)
    {
      case GDK_BUTTON_PRESS:
        switch (event->button)
        {
          case 1:
            if (position == -1 || position == m_length)
                return true;

            switch (state)
            {
              case 0:
                if (m_playlist.entry_selected (position))
                    select_slide (false, position);
                else
                    select_single (false, position);

                m_drag = DRAG_MOVE;
                break;
              case GDK_SHIFT_MASK:
                select_extend (false, position);
                m_drag = DRAG_SELECT;
                break;
              case GDK_CONTROL_MASK:
                select_toggle (false, position);
                m_drag = DRAG_SELECT;
                break;
              default:
                return true;
            }

            break;
          case 3:
            if (state)
                return true;

            if (position != -1 && position != m_length)
            {
                if (m_playlist.entry_selected (position))
                    select_slide (false, position);
                else
                    select_single (false, position);
            }

            menu_popup ((position == -1) ? UI_MENU_PLAYLIST :
             UI_MENU_PLAYLIST_CONTEXT, event->x_root, event->y_root, false,
             false, 3, event->time);
            break;
          default:
            return false;
        }

        break;
      case GDK_2BUTTON_PRESS:
        if (event->button != 1 || state || position == m_length)
            return true;

        if (position != -1)
            m_playlist.set_position (position);

        m_playlist.start_playback ();
        break;
      default:
        return true;
    }

    refresh ();
    return true;
}

bool PlaylistWidget::button_release (GdkEventButton * event)
{
    cancel_all ();
    return true;
}

void PlaylistWidget::scroll_timeout ()
{
    int position = adjust_position (true, m_scroll);
    if (position == -1)
        return;

    switch (m_drag)
    {
      case DRAG_SELECT:
        select_extend (false, position);
        break;
      case DRAG_MOVE:
        select_move (false, position);
        break;
    }

    refresh ();
}

bool PlaylistWidget::motion (GdkEventMotion * event)
{
    int position = calc_position (event->y);

    if (m_drag)
    {
        if (position == -1 || position == m_length)
        {
            if (! m_scroll)
                scroll_timer.start ();

            m_scroll = (position == -1 ? -1 : 1);
        }
        else
        {
            if (m_scroll)
            {
                m_scroll = 0;
                scroll_timer.stop ();
            }

            switch (m_drag)
            {
              case DRAG_SELECT:
                select_extend (false, position);
                break;
              case DRAG_MOVE:
                select_move (false, position);
                break;
            }

            refresh ();
        }
    }
    else
    {
        if (position == -1 || position == m_length)
            cancel_all ();
        else if (aud_get_bool (nullptr, "show_filepopup_for_tuple") && m_popup_pos != position)
        {
            cancel_all ();
            popup_trigger (position);
        }
    }

    return true;
}

bool PlaylistWidget::leave ()
{
    if (! m_drag)
        cancel_all ();

    return true;
}

void PlaylistWidget::popup_trigger (int pos)
{
    audgui_infopopup_hide ();

    m_popup_pos = pos;
    m_popup_timer.queue (aud_get_int (nullptr, "filepopup_delay") * 100,
     aud::obj_member<PlaylistWidget, & PlaylistWidget::popup_show>, this);
}

void PlaylistWidget::popup_show ()
{
    audgui_infopopup_show (m_playlist, m_popup_pos);
}

void PlaylistWidget::popup_hide ()
{
    audgui_infopopup_hide ();

    m_popup_pos = -1;
    m_popup_timer.stop ();
}
