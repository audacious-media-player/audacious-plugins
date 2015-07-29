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

#include <QCloseEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QWidget>

class Widget : public QWidget
{
public:
    void queue_draw () { update (); }

protected:
    void add_input (int width, int height, bool track_motion, bool drawable);
    void add_drawable (int width, int height);

    void set_scale (int scale) { m_scale = scale; }
    void resize (int w, int h) { QWidget::resize (w * m_scale, h * m_scale); }

#ifdef Q_OS_MAC
    /* repaint() causes graphical glitches on OS X
     * http://redmine.audacious-media-player.org/issues/558 */
    void draw_now () { update (); }
#else
    void draw_now () { repaint (); }
#endif

    virtual void draw (QPainter & cr) {}
    virtual bool keypress (QKeyEvent * event) { return false; }
    virtual bool button_press (QMouseEvent * event) { return false; }
    virtual bool button_release (QMouseEvent * event) { return false; }
    virtual bool scroll (QWheelEvent * event) { return false; }
    virtual bool motion (QMouseEvent * event) { return false; }
    virtual bool leave () { return false; }
    virtual bool close () { return false; }

private:
    void paintEvent (QPaintEvent *);

    void keyPressEvent (QKeyEvent * event)
        { event->setAccepted (keypress (event)); }
    void mousePressEvent (QMouseEvent * event)
        { event->setAccepted (button_press (event)); }
    void mouseReleaseEvent (QMouseEvent * event)
        { event->setAccepted (button_release (event)); }
    void wheelEvent (QWheelEvent * event)
        { event->setAccepted (scroll (event)); }
    void mouseMoveEvent (QMouseEvent * event)
        { event->setAccepted (motion (event)); }
    void leaveEvent (QEvent * event)
        { event->setAccepted (leave ()); }
    void closeEvent (QCloseEvent * event)
        { event->setAccepted (close ()); }

    bool m_drawable = false;
    int m_scale = 1;
};

#endif // SKINS_WIDGET_H
