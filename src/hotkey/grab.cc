/*
 *  This file is part of audacious-hotkey plugin for audacious
 *
 *  Copyright (c) 2007 - 2008  Sascha Hlusiak <contact@saschahlusiak.de>
 *  Name: grab.c
 *  Description: grab.c
 *
 *  Part of this code is from itouch-ctrl plugin.
 *  Authors of itouch-ctrl are listed below:
 *
 *  Copyright (c) 2006 - 2007 Vladimir Paskov <vlado.paskov@gmail.com>
 *
 *  Part of this code are from xmms-itouch plugin.
 *  Authors of xmms-itouch are listed below:
 *
 *  Copyright (C) 2000-2002 Ville Syrjälä <syrjala@sci.fi>
 *                         Bryn Davies <curious@ihug.com.au>
 *                         Jonathan A. Davis <davis@jdhouse.org>
 *                         Jeremy Tan <nsx@nsx.homeip.net>
 *
 *  audacious-hotkey is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  audacious-hotkey is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with audacious-hotkey; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "grab.h"
#include "plugin.h"


static int grabbed = 0;
static unsigned int numlock_mask = 0;
static unsigned int scrolllock_mask = 0;
static unsigned int capslock_mask = 0;


/* Taken from xbindkeys */
static void get_offending_modifiers (Display * dpy)
{
    int i;
    XModifierKeymap *modmap;
    KeyCode nlock, slock;

    static int mask_table[8] = {
        ShiftMask, LockMask, ControlMask, Mod1Mask,
        Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask
    };

    nlock = XKeysymToKeycode (dpy, XK_Num_Lock);
    slock = XKeysymToKeycode (dpy, XK_Scroll_Lock);

    /*
    * Find out the masks for the NumLock and ScrollLock modifiers,
    * so that we can bind the grabs for when they are enabled too.
    */
    modmap = XGetModifierMapping (dpy);

    if (modmap != nullptr && modmap->max_keypermod > 0)
    {
        for (i = 0; i < 8 * modmap->max_keypermod; i++)
        {
            if (modmap->modifiermap[i] == nlock && nlock != 0)
                numlock_mask = mask_table[i / modmap->max_keypermod];
            else if (modmap->modifiermap[i] == slock && slock != 0)
                scrolllock_mask = mask_table[i / modmap->max_keypermod];
        }
    }

    capslock_mask = LockMask;

    if (modmap)
        XFreeModifiermap (modmap);
}


static int x11_error_handler (Display *dpy, XErrorEvent *error)
{
    return 0;
}

/* grab required keys */
static void grab_key(const HotkeyConfiguration *hotkey, Display *xdisplay, Window x_root_window)
{
    unsigned int modifier = hotkey->mask & ~(numlock_mask | capslock_mask | scrolllock_mask);

    if (hotkey->key == 0) return;

    if (hotkey->type == TYPE_KEY)
    {
        XGrabKey (xdisplay, hotkey->key, modifier, x_root_window,
            False, GrabModeAsync, GrabModeAsync);

        if (modifier == AnyModifier)
            return;

        if (numlock_mask)
            XGrabKey (xdisplay, hotkey->key, modifier | numlock_mask,
                x_root_window,
                False, GrabModeAsync, GrabModeAsync);

        if (capslock_mask)
            XGrabKey (xdisplay, hotkey->key, modifier | capslock_mask,
                x_root_window,
                False, GrabModeAsync, GrabModeAsync);

        if (scrolllock_mask)
            XGrabKey (xdisplay, hotkey->key, modifier | scrolllock_mask,
                x_root_window,
                False, GrabModeAsync, GrabModeAsync);

        if (numlock_mask && capslock_mask)
            XGrabKey (xdisplay, hotkey->key, modifier | numlock_mask | capslock_mask,
                x_root_window,
                False, GrabModeAsync, GrabModeAsync);

        if (numlock_mask && scrolllock_mask)
            XGrabKey (xdisplay, hotkey->key, modifier | numlock_mask | scrolllock_mask,
                x_root_window,
                False, GrabModeAsync, GrabModeAsync);

        if (capslock_mask && scrolllock_mask)
            XGrabKey (xdisplay, hotkey->key, modifier | capslock_mask | scrolllock_mask,
                x_root_window,
                False, GrabModeAsync, GrabModeAsync);

        if (numlock_mask && capslock_mask && scrolllock_mask)
            XGrabKey (xdisplay, hotkey->key,
                modifier | numlock_mask | capslock_mask | scrolllock_mask,
                x_root_window, False, GrabModeAsync,
                GrabModeAsync);
    }
    if (hotkey->type == TYPE_MOUSE)
    {
        XGrabButton (xdisplay, hotkey->key, modifier, x_root_window,
            False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);

        if (modifier == AnyModifier)
            return;

        if (numlock_mask)
            XGrabButton (xdisplay, hotkey->key, modifier | numlock_mask,
                x_root_window,
                False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);

        if (capslock_mask)
            XGrabButton (xdisplay, hotkey->key, modifier | capslock_mask,
                x_root_window,
                False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);

        if (scrolllock_mask)
            XGrabButton (xdisplay, hotkey->key, modifier | scrolllock_mask,
                x_root_window,
                False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);

        if (numlock_mask && capslock_mask)
            XGrabButton (xdisplay, hotkey->key, modifier | numlock_mask | capslock_mask,
                x_root_window,
                False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);

        if (numlock_mask && scrolllock_mask)
            XGrabButton (xdisplay, hotkey->key, modifier | numlock_mask | scrolllock_mask,
                x_root_window,
                False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);

        if (capslock_mask && scrolllock_mask)
            XGrabButton (xdisplay, hotkey->key, modifier | capslock_mask | scrolllock_mask,
                x_root_window,
                False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);

        if (numlock_mask && capslock_mask && scrolllock_mask)
            XGrabButton (xdisplay, hotkey->key,
                modifier | numlock_mask | capslock_mask | scrolllock_mask,
                x_root_window, False, ButtonPressMask, GrabModeAsync,
                GrabModeAsync, None, None);
    }
}

void grab_keys ( )
{
    Display* xdisplay;
    int screen;
    PluginConfig* plugin_cfg = get_config();
    HotkeyConfiguration *hotkey;

    XErrorHandler old_handler = 0;
     xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());

    if (grabbed) return;


    XSync(xdisplay, False);
    old_handler = XSetErrorHandler (x11_error_handler);

    get_offending_modifiers(xdisplay);
    hotkey = &(plugin_cfg->first);
    while (hotkey)
    {
        for (screen=0;screen<ScreenCount(xdisplay);screen++)
        {
            grab_key(hotkey, xdisplay, RootWindow(xdisplay, screen));
        }
        hotkey = hotkey->next;
    }

    XSync(xdisplay, False);
    XSetErrorHandler (old_handler);

    grabbed = 1;
}



/* grab required keys */
static void ungrab_key(HotkeyConfiguration *hotkey, Display* xdisplay, Window x_root_window)
{
    unsigned int modifier = hotkey->mask & ~(numlock_mask | capslock_mask | scrolllock_mask);

    if (hotkey->key == 0) return;

    if (hotkey->type == TYPE_KEY)
    {
        XUngrabKey (xdisplay, hotkey->key, modifier, x_root_window);

        if (modifier == AnyModifier)
            return;

        if (numlock_mask)
            XUngrabKey (xdisplay, hotkey->key, modifier | numlock_mask, x_root_window);

        if (capslock_mask)
            XUngrabKey (xdisplay, hotkey->key, modifier | capslock_mask, x_root_window);

        if (scrolllock_mask)
            XUngrabKey (xdisplay, hotkey->key, modifier | scrolllock_mask, x_root_window);

        if (numlock_mask && capslock_mask)
            XUngrabKey (xdisplay, hotkey->key, modifier | numlock_mask | capslock_mask, x_root_window);

        if (numlock_mask && scrolllock_mask)
            XUngrabKey (xdisplay, hotkey->key, modifier | numlock_mask | scrolllock_mask, x_root_window);

        if (capslock_mask && scrolllock_mask)
            XUngrabKey (xdisplay, hotkey->key, modifier | capslock_mask | scrolllock_mask, x_root_window);

        if (numlock_mask && capslock_mask && scrolllock_mask)
            XUngrabKey (xdisplay, hotkey->key, modifier | numlock_mask | capslock_mask | scrolllock_mask, x_root_window);
    }
    if (hotkey->type == TYPE_MOUSE)
    {
        XUngrabButton (xdisplay, hotkey->key, modifier, x_root_window);

        if (modifier == AnyModifier)
            return;

        if (numlock_mask)
            XUngrabButton (xdisplay, hotkey->key, modifier | numlock_mask, x_root_window);

        if (capslock_mask)
            XUngrabButton (xdisplay, hotkey->key, modifier | capslock_mask, x_root_window);

        if (scrolllock_mask)
            XUngrabButton (xdisplay, hotkey->key, modifier | scrolllock_mask, x_root_window);

        if (numlock_mask && capslock_mask)
            XUngrabButton (xdisplay, hotkey->key, modifier | numlock_mask | capslock_mask, x_root_window);

        if (numlock_mask && scrolllock_mask)
            XUngrabButton (xdisplay, hotkey->key, modifier | numlock_mask | scrolllock_mask, x_root_window);

        if (capslock_mask && scrolllock_mask)
            XUngrabButton (xdisplay, hotkey->key, modifier | capslock_mask | scrolllock_mask, x_root_window);

        if (numlock_mask && capslock_mask && scrolllock_mask)
            XUngrabButton (xdisplay, hotkey->key, modifier | numlock_mask | capslock_mask | scrolllock_mask, x_root_window);
    }
}

void ungrab_keys ( )
{
    Display* xdisplay;
    int screen;
    PluginConfig* plugin_cfg = get_config();
    HotkeyConfiguration *hotkey;

    XErrorHandler old_handler = 0;
     xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());

    if (!grabbed) return;
    if (!xdisplay) return;

    XSync(xdisplay, False);
    old_handler = XSetErrorHandler (x11_error_handler);

    get_offending_modifiers(xdisplay);

    hotkey = &(plugin_cfg->first);
    while (hotkey)
    {
        for (screen=0;screen<ScreenCount(xdisplay);screen++)
        {
            ungrab_key(hotkey, xdisplay, RootWindow(xdisplay, screen));
        }
        hotkey = hotkey->next;
    }

    XSync(xdisplay, False);
    XSetErrorHandler (old_handler);

    grabbed = 0;
}


static GdkFilterReturn
gdk_filter(GdkXEvent *xevent,
       GdkEvent *event,
       void * data)
{
    HotkeyConfiguration *hotkey;
    hotkey = &(get_config()->first);
    switch (((XEvent*)xevent)->type)
    {
    case KeyPress:
        {
            XKeyEvent *keyevent = (XKeyEvent*)xevent;
            while (hotkey)
            {
                if ((hotkey->key == keyevent->keycode) &&
                    (hotkey->mask == (keyevent->state & ~(scrolllock_mask | numlock_mask | capslock_mask))) &&
                    (hotkey->type == TYPE_KEY))
                {
                    if (handle_keyevent(hotkey->event))
                        return GDK_FILTER_REMOVE;
                    break;
                }

                hotkey = hotkey->next;
            }
            break;
        }
    case ButtonPress:
        {
            XButtonEvent *buttonevent = (XButtonEvent*)xevent;
            while (hotkey)
            {
                if ((hotkey->key == buttonevent->button) &&
                    (hotkey->mask == (buttonevent->state & ~(scrolllock_mask | numlock_mask | capslock_mask))) &&
                    (hotkey->type == TYPE_MOUSE))
                {
                    if (handle_keyevent(hotkey->event))
                        return GDK_FILTER_REMOVE;
                    break;
                }

                hotkey = hotkey->next;
            }

            break;
        }
    }

    return GDK_FILTER_CONTINUE;
}

gboolean setup_filter()
{
    gdk_window_add_filter (gdk_screen_get_root_window
     (gdk_screen_get_default ()), gdk_filter, nullptr);

    return true;
}

void release_filter()
{
    gdk_window_remove_filter (gdk_screen_get_root_window
     (gdk_screen_get_default ()), gdk_filter, nullptr);
}
