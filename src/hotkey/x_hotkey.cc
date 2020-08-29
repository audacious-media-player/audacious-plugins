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