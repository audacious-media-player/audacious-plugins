/*
 * ui_skinned_window.h
 * Copyright 2011 John Lindgren
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

#ifndef SKINS_UI_SKINNED_WINDOW_H
#define SKINS_UI_SKINNED_WINDOW_H

#include "widget.h"

#include <libaudcore/objects.h>

enum {
    WINDOW_MAIN,
    WINDOW_EQ,
    WINDOW_PLAYLIST,
    N_WINDOWS
};

class Window : public Widget
{
public:
    Window (int id, int * x, int * y, int w, int h, bool shaded);
    ~Window ();

    void resize (int w, int h);
    void set_shapes (GdkRegion * shape, GdkRegion * sshape);
    bool is_shaded () { return m_is_shaded; }
    void set_shaded (bool shaded);
    void put_widget (bool shaded, Widget * widget, int x, int y);
    void move_widget (bool shaded, Widget * widget, int x, int y);

    void setWindowTitle (const char * title)
        { gtk_window_set_title ((GtkWindow *) gtk (), title); }
    void getPosition (int * x, int * y)
        { gtk_window_get_position ((GtkWindow *) gtk (), x, y); }
    void move (int x, int y)
        { gtk_window_move ((GtkWindow *) gtk (), x, y); }

protected:
    void realize ();
    bool keypress (GdkEventKey * event);  // in main.cc
    bool button_press (GdkEventButton * event);
    bool button_release (GdkEventButton * event);
    bool motion (GdkEventMotion * event);
    bool close ();

private:
    void apply_shape ();

    const int m_id;
    bool m_is_shaded = false;
    bool m_is_moving = false;
    GtkWidget * m_normal = nullptr, * m_shaded = nullptr;
    SmartPtr<GdkRegion, gdk_region_destroy> m_shape, m_sshape;
};

void dock_add_window (int id, Window * window, int * x, int * y, int w, int h);
void dock_remove_window (int id);
void dock_set_size (int id, int w, int h);
void dock_move_start (int id, int x, int y);
void dock_move (int x, int y);
void dock_change_scale (int old_scale, int new_scale);

#endif
