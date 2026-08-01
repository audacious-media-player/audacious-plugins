/*
 * x11-utils.cc
 * Copyright 2026 Thomas Lange
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

#include "x11-utils.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QGuiApplication>
#else
#include <QX11Info>
#endif

#include <X11/Xlib.h>

static Display * get_x11_display ()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
    auto * x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application> ();
    if (x11)
        return x11->display ();
#elif QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (QX11Info::isPlatformX11 ())
        return QX11Info::display ();
#endif
    return nullptr;
}

static void x11_set_wm_state_flag (QWidget * widget, const char * flag, bool enable)
{
    if (! widget || ! widget->testAttribute (Qt::WA_WState_Created))
        return;

    Display * display = get_x11_display ();
    if (! display)
        return;

    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.window = static_cast<Window> (widget->winId ());
    event.xclient.message_type = XInternAtom (display, "_NET_WM_STATE", false);
    event.xclient.format = 32;
    event.xclient.data.l[0] = static_cast<long> (enable);
    event.xclient.data.l[1] = XInternAtom (display, flag, false);
    event.xclient.data.l[2] = 0;
    event.xclient.data.l[3] = 1;
    event.xclient.data.l[4] = 0;

    XSendEvent (display, DefaultRootWindow (display), false,
                SubstructureRedirectMask | SubstructureNotifyMask, & event);
    XFlush (display);
}

void x11_window_set_stay_on_top (QWidget * widget, bool enable)
{
    x11_set_wm_state_flag (widget, "_NET_WM_STATE_ABOVE", enable);
}

void x11_window_set_sticky (QWidget * widget, bool enable)
{
    x11_set_wm_state_flag (widget, "_NET_WM_STATE_STICKY", enable);
}
