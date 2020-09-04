/*
 *  hotkey_api_common.cc
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

#include "api_hotkey.h"
#include <X11/X.h>
#include <gdk/gdkevents.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libaudcore/i18n.h>
#ifdef _WIN32
#include <libaudcore/runtime.h>
#include <windows.h>
#endif

template<typename T_GDK_EVENT>
int Hotkey::calculate_mod(T_GDK_EVENT * event)
{
    int mod = 0;
    if (event->state & GDK_CONTROL_MASK)
        mod |= HK_CONTROL_MASK;

    if (event->state & GDK_MOD1_MASK)
        mod |= HK_MOD1_ALT_MASK;

    if (event->state & GDK_SHIFT_MASK)
        mod |= HK_SHIFT_MASK;

    if (event->state & GDK_MOD5_MASK)
        mod |= HK_MOD5_MASK;

    if (event->state & GDK_MOD4_MASK)
        mod |= HK_MOD4_MASK;
    return mod;
}

template int Hotkey::calculate_mod<GdkEventScroll>(GdkEventScroll * event);
template int Hotkey::calculate_mod<GdkEventButton>(GdkEventButton * event);
// template int Hotkey::calculate_mod<GdkEventKey>(GdkEventKey* event);

std::pair<int, int> Hotkey::get_is_mod(GdkEventKey * event)
{
    int mod = 0;
    int is_mod = 0;

    if ((event->state & GDK_CONTROL_MASK) |
        (!is_mod && (is_mod = (event->keyval == GDK_Control_L ||
                               event->keyval == GDK_Control_R))))
        mod |= HK_CONTROL_MASK;

    if ((event->state & GDK_MOD1_MASK) |
        (!is_mod &&
         (is_mod = (event->keyval == GDK_Alt_L || event->keyval == GDK_Alt_R))))
        mod |= HK_MOD1_ALT_MASK;

    if ((event->state & GDK_SHIFT_MASK) |
        (!is_mod && (is_mod = (event->keyval == GDK_Shift_L ||
                               event->keyval == GDK_Shift_R))))
        mod |= HK_SHIFT_MASK;

    if ((event->state & GDK_MOD5_MASK) |
        (!is_mod && (is_mod = (event->keyval == GDK_ISO_Level3_Shift))))
        mod |= HK_MOD5_MASK;

    if ((event->state & GDK_MOD4_MASK)

        | (!is_mod && (is_mod = (event->keyval == GDK_Super_L ||
                                 event->keyval == GDK_Super_R))))
        mod |= HK_MOD4_MASK;

    return std::make_pair(mod, is_mod);
}

void Hotkey::set_keytext(GtkWidget * entry, int key, int mask, int type)
{
    char * text = nullptr;

    if (key == 0 && mask == 0)
    {
        text = g_strdup(_("(none)"));
    }
    else
    {
        char * keytext = nullptr;
        if (type == TYPE_KEY)
        {
            Hotkey::key_to_string(key, &keytext);
        }
        if (type == TYPE_MOUSE)
        {
            keytext = g_strdup_printf("Button%d", key);
        }
        text = Hotkey::create_human_readable_keytext(keytext, key, mask);
        g_free(keytext);
    }

    gtk_entry_set_text(GTK_ENTRY(entry), text);
    gtk_editable_set_position(GTK_EDITABLE(entry), -1);
    if (text)
        g_free(text);
}
