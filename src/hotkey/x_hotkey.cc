/*
 *  x_hotkey.cc
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

#include <X11/X.h>
#include <X11/XKBlib.h>
#include <gdk/gdkx.h>
#include <gtk/gtkentry.h>

#include "api_hotkey.h"

void Hotkey::add_hotkey(HotkeyConfiguration ** pphotkey, OS_KeySym keysym,
                        int mask, int type, EVENT event)
{
    KeyCode keycode;
    HotkeyConfiguration * photkey;
    if (keysym == 0)
        return;
    if (pphotkey == nullptr)
        return;
    photkey = *pphotkey;
    if (photkey == nullptr)
        return;
    keycode = XKeysymToKeycode(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
                               keysym);
    if (keycode == 0)
        return;
    if (photkey->key)
    {
        photkey->next = g_new(HotkeyConfiguration, 1);
        photkey = photkey->next;
        *pphotkey = photkey;
        photkey->next = nullptr;
    }
    photkey->key = (int)keycode;
    photkey->mask = mask;
    photkey->event = event;
    photkey->type = type;
}

void Hotkey::key_to_string(int key, char ** out_keytext)
{
    KeySym keysym;
    keysym = XkbKeycodeToKeysym(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
                                key, 0, 0);
    if (keysym == 0 || keysym == NoSymbol)
    {
        *out_keytext = g_strdup_printf("#%d", key);
    }
    else
    {
        *out_keytext = g_strdup(XKeysymToString(keysym));
    }
}

char* Hotkey::create_human_readable_keytext(const char* const keytext, int key, int mask)
{
    static const constexpr unsigned int modifiers[] = {
        HK_CONTROL_MASK, HK_SHIFT_MASK, HK_MOD1_ALT_MASK, HK_MOD2_MASK,
        HK_MOD3_MASK,    HK_MOD4_MASK,  HK_MOD5_MASK};
    static const constexpr char * const modifier_string[] = {
        "Control", "Shift", "Alt", "Mod2", "Mod3", "Super", "Mod5"};
    const char * strings[9];
    int i, j;
    for (i = 0, j = 0; j < 7; j++)
    {
        if (mask & modifiers[j])
            strings[i++] = modifier_string[j];
    }
    if (key != 0)
        strings[i++] = keytext;
    strings[i] = nullptr;

    return g_strjoinv(" + ", (char **)strings);
}