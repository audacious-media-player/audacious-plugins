/*
 *  api_hotkey.h
 *  Audacious
 *
 *  Copyright (C) 2005-2020  Audacious team
 *
 *  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2003  Peter Alm, Mikael Alm, Olle Hallnas,
 *                           Thomas Nilsson and 4Front Technologies
 *  Copyright (C) 1999-2003  Haavard Kvaalen
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

#ifndef _X_HOTKEY_H_INCLUDED
#define _X_HOTKEY_H_INCLUDED

#include <gtk/gtkstyle.h>
#include <utility>
#ifndef _WIN32
#include <X11/X.h>
#include <X11/XF86keysym.h>
#endif
#include "plugin.h"

class Hotkey
{

    //#ifdef _WIN32
    //#define OS_KeySym int
    //#else
    //#define OS_KeySym KeySym
    //#endif

#ifdef _WIN32
    typedef int OS_KeySym;
#else
    typedef KeySym OS_KeySym;
#endif

public:
    static void set_keytext(GtkWidget * entry, int key, int mask, int type);
    static std::pair<int, int> get_is_mod(GdkEventKey * event);
    template<typename T_GDK_EVENT>
    static int calculate_mod(T_GDK_EVENT * event);
    static void add_hotkey(HotkeyConfiguration ** pphotkey, OS_KeySym keysym,
                           int mask, int type, EVENT event);
    static void key_to_string(int key, char ** out_keytext);
    static char* create_human_readable_keytext(const char* const keytext, int key, int mask);
};

#ifndef _WIN32
#define OS_KEY_AudioPrev XF86XK_AudioPrev
#define OS_KEY_AudioPlay XF86XK_AudioPlay
#define OS_KEY_AudioPause XF86XK_AudioPause
#define OS_KEY_AudioStop XF86XK_AudioStop
#define OS_KEY_AudioNext XF86XK_AudioNext
#define OS_KEY_AudioMute XF86XK_AudioMute
#define OS_KEY_AudioRaiseVolume XF86XK_AudioRaiseVolume
#define OS_KEY_AudioLowerVolume XF86XK_AudioLowerVolume
#else

#define OS_KEY_AudioPrev 0
#define OS_KEY_AudioPlay 0
#define OS_KEY_AudioPause 0
#define OS_KEY_AudioStop 0
#define OS_KEY_AudioNext 0
#define OS_KEY_AudioMute 0
#define OS_KEY_AudioRaiseVolume 0
#define OS_KEY_AudioLowerVolume 0

#endif

#endif //_X_HOTKEY_H_INCLUDED
