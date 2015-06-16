/*
 * widget.h
 * Copyright 2015 John Lindgren
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

#ifndef SKINS_WIDGET_H
#define SKINS_WIDGET_H

#include <QPainter>
#include <QWidget>

class Widget : public QWidget
{
public:
    void queue_draw () { update (); }

protected:
    void add_input (int width, int height, bool track_motion, bool drawable);
    void add_drawable (int width, int height);

    void draw_now () { repaint (); }

    virtual void draw (QPainter & cr) {}
#if 0
    virtual void realize () {}
    virtual bool button_press (GdkEventButton * event) { return false; }
    virtual bool button_release (GdkEventButton * event) { return false; }
    virtual bool scroll (GdkEventScroll * event) { return false; }
    virtual bool motion (GdkEventMotion * event) { return false; }
    virtual bool leave (GdkEventCrossing * event) { return false; }
#endif

private:
    void paintEvent (QPaintEvent *);
};

#endif // SKINS_WIDGET_H
