

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
#ifdef _WIN32
    AUDDBG("lHotkeyFlow:Win call: get_is_mod.%s",
           gdk_keyval_name(event->keyval));
#endif
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
        static const char * modifier_string[] = {
            "Control", "Shift", "Alt", "Mod2", "Mod3", "Super", "Mod5"};
        static const unsigned int modifiers[] = {
            HK_CONTROL_MASK, HK_SHIFT_MASK, HK_MOD1_ALT_MASK, HK_MOD2_MASK,
            HK_MOD3_MASK,    HK_MOD4_MASK,  HK_MOD5_MASK};
        const char * strings[9];
        char * keytext = nullptr;
        int i, j;
        if (type == TYPE_KEY)
        {
            Hotkey::key_to_string(key, &keytext);
        }
        if (type == TYPE_MOUSE)
        {
            keytext = g_strdup_printf("Button%d", key);
        }

        for (i = 0, j = 0; j < 7; j++)
        {
            if (mask & modifiers[j])
                strings[i++] = modifier_string[j];
        }
        if (key != 0)
            strings[i++] = keytext;
        strings[i] = nullptr;

        text = g_strjoinv(" + ", (char **)strings);
        g_free(keytext);
    }

    gtk_entry_set_text(GTK_ENTRY(entry), text);
    gtk_editable_set_position(GTK_EDITABLE(entry), -1);
    if (text)
        g_free(text);
}
