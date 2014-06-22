/*
 *  This file is part of audacious-hotkey plugin for audacious
 *
 *  Copyright (c) 2007 - 2008  Sascha Hlusiak <contact@saschahlusiak.de>
 *  Name: gui.c
 *  Description: gui.c
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
#include <gdk/gdkkeysyms-compat.h>

#include <libaudcore/plugin.h>
#include <libaudcore/i18n.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include <X11/XKBlib.h>

#include "plugin.h"
#include "gui.h"
#include "grab.h"

typedef struct _KeyControls {
    GtkWidget *keytext;
    GtkWidget *grid;
    GtkWidget *button;
    GtkWidget *combobox;

    HotkeyConfiguration hotkey;
    struct _KeyControls *next, *prev, *first;
} KeyControls;


static void clear_keyboard (GtkWidget *widget, gpointer data);
static void add_callback (GtkWidget *widget, gpointer data);
static void cancel_callback (GtkWidget *widget, gpointer data);
static void destroy_callback (GtkWidget *widget, gpointer data);
static void ok_callback (GtkWidget *widget, gpointer data);


static const char * event_desc[EVENT_MAX] = {
    [EVENT_PREV_TRACK] = N_("Previous track"),
    [EVENT_PLAY] = N_("Play"),
    [EVENT_PAUSE] = N_("Pause/Resume"),
    [EVENT_STOP] = N_("Stop"),
    [EVENT_NEXT_TRACK] = N_("Next track"),
    [EVENT_FORWARD] = N_("Forward 5 seconds"),
    [EVENT_BACKWARD] = N_("Rewind 5 seconds"),
    [EVENT_MUTE] = N_("Mute"),
    [EVENT_VOL_UP] = N_("Volume up"),
    [EVENT_VOL_DOWN] = N_("Volume down"),
    [EVENT_JUMP_TO_FILE] = N_("Jump to file"),
    [EVENT_TOGGLE_WIN] = N_("Toggle player window(s)"),
    [EVENT_SHOW_AOSD] = N_("Show On-Screen-Display"),
    [EVENT_TOGGLE_REPEAT] = N_("Toggle repeat"),
    [EVENT_TOGGLE_SHUFFLE] = N_("Toggle shuffle"),
    [EVENT_TOGGLE_STOP] = N_("Toggle stop after current"),
    [EVENT_RAISE] = N_("Raise player window(s)")
};


static void set_keytext (GtkWidget *entry, gint key, gint mask, gint type)
{
    gchar *text = NULL;

    if (key == 0 && mask == 0)
    {
        text = g_strdup(_("(none)"));
    } else {
        static const char *modifier_string[] = { "Control", "Shift", "Alt", "Mod2", "Mod3", "Super", "Mod5" };
        static const unsigned int modifiers[] = { ControlMask, ShiftMask, Mod1Mask, Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask };
        const char *strings[9];
        gchar *keytext = NULL;
        int i, j;
        if (type == TYPE_KEY)
        {
            KeySym keysym;
            keysym = XkbKeycodeToKeysym(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), key, 0, 0);
            if (keysym == 0 || keysym == NoSymbol)
            {
                keytext = g_strdup_printf("#%d", key);
            } else {
                keytext = g_strdup(XKeysymToString(keysym));
            }
        }
        if (type == TYPE_MOUSE)
        {
            keytext = g_strdup_printf("Button%d", key);
        }

        for (i = 0, j=0; j<7; j++)
        {
            if (mask & modifiers[j])
                 strings[i++] = modifier_string[j];
        }
        if (key != 0) strings[i++] = keytext;
        strings[i] = NULL;

        text = g_strjoinv(" + ", (char **)strings);
        g_free(keytext);
    }

    gtk_entry_set_text(GTK_ENTRY(entry), text);
    gtk_editable_set_position(GTK_EDITABLE(entry), -1);
    if (text) g_free(text);
}

static gboolean
on_entry_key_press_event(GtkWidget * widget,
                         GdkEventKey * event,
                         gpointer user_data)
{
    KeyControls *controls = (KeyControls*) user_data;
    int is_mod;
    int mod;

    if (event->keyval == GDK_Tab) return FALSE;
    if (event->keyval == GDK_Escape && ((event->state & ~GDK_LOCK_MASK) == 0)) return FALSE;
    if (event->keyval == GDK_Return && ((event->state & ~GDK_LOCK_MASK) == 0)) return FALSE;
    if (event->keyval == GDK_ISO_Left_Tab)
    {
        set_keytext(controls->keytext, controls->hotkey.key, controls->hotkey.mask, controls->hotkey.type);
        return FALSE;
    }
    if (event->keyval == GDK_Up && ((event->state & ~GDK_LOCK_MASK) == 0)) return FALSE;
    if (event->keyval == GDK_Down && ((event->state & ~GDK_LOCK_MASK) == 0)) return FALSE;

    mod = 0;
    is_mod = 0;

    if ((event->state & GDK_CONTROL_MASK) | (!is_mod && (is_mod = (event->keyval == GDK_Control_L || event->keyval == GDK_Control_R))))
            mod |= ControlMask;

    if ((event->state & GDK_MOD1_MASK) | (!is_mod && (is_mod = (event->keyval == GDK_Alt_L || event->keyval == GDK_Alt_R))))
            mod |= Mod1Mask;

    if ((event->state & GDK_SHIFT_MASK) | (!is_mod && (is_mod = (event->keyval == GDK_Shift_L || event->keyval == GDK_Shift_R))))
            mod |= ShiftMask;

    if ((event->state & GDK_MOD5_MASK) | (!is_mod && (is_mod = (event->keyval == GDK_ISO_Level3_Shift))))
            mod |= Mod5Mask;

    if ((event->state & GDK_MOD4_MASK) | (!is_mod && (is_mod = (event->keyval == GDK_Super_L || event->keyval == GDK_Super_R))))
            mod |= Mod4Mask;

    if (!is_mod) {
        controls->hotkey.key = event->hardware_keycode;
        controls->hotkey.mask = mod;
        controls->hotkey.type = TYPE_KEY;
        if (controls->next == NULL)
            add_callback (NULL, (gpointer) controls);
        else gtk_widget_grab_focus(GTK_WIDGET(controls->next->keytext));
    }

    set_keytext(controls->keytext, is_mod ? 0 : event->hardware_keycode, mod, TYPE_KEY);
    return TRUE;
}

static gboolean
on_entry_key_release_event(GtkWidget * widget,
                           GdkEventKey * event,
                           gpointer user_data)
{
    KeyControls *controls = (KeyControls*) user_data;
    if (!gtk_widget_is_focus(widget)) return FALSE;
    set_keytext(controls->keytext, controls->hotkey.key, controls->hotkey.mask, controls->hotkey.type);

    return TRUE;
}

static gboolean
on_entry_button_press_event(GtkWidget * widget,
                            GdkEventButton * event,
                            gpointer user_data)
{
    KeyControls *controls = (KeyControls*) user_data;
    int mod;

    if (!gtk_widget_is_focus(widget)) return FALSE;

    mod = 0;
    if (event->state & GDK_CONTROL_MASK)
            mod |= ControlMask;

    if (event->state & GDK_MOD1_MASK)
            mod |= Mod1Mask;

    if (event->state & GDK_SHIFT_MASK)
            mod |= ShiftMask;

    if (event->state & GDK_MOD5_MASK)
            mod |= Mod5Mask;

    if (event->state & GDK_MOD4_MASK)
            mod |= Mod4Mask;

    if ((event->button <= 3) && (mod == 0))
    {
        GtkWidget* dialog;
        int response;
        dialog = gtk_message_dialog_new (GTK_WINDOW(gtk_widget_get_toplevel(widget)),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_WARNING,
            GTK_BUTTONS_YES_NO,
            _("It is not recommended to bind the primary mouse buttons without modificators.\n\n"
              "Do you want to continue?"));
        gtk_window_set_title(GTK_WINDOW(dialog), _("Binding mouse buttons"));
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy (dialog);
        if (response != GTK_RESPONSE_YES) return TRUE;
    }

    controls->hotkey.key = event->button;
    controls->hotkey.mask = mod;
        controls->hotkey.type = TYPE_MOUSE;
    set_keytext(controls->keytext, controls->hotkey.key, controls->hotkey.mask, controls->hotkey.type);
    if (controls->next == NULL)
        add_callback (NULL, (gpointer) controls);

    return TRUE;
}

static gboolean
on_entry_scroll_event(GtkWidget * widget,
                            GdkEventScroll * event,
                            gpointer user_data)
{
    KeyControls *controls = (KeyControls*) user_data;
    int mod;

    if (!gtk_widget_is_focus(widget)) return FALSE;

    mod = 0;
    if (event->state & GDK_CONTROL_MASK)
            mod |= ControlMask;

    if (event->state & GDK_MOD1_MASK)
            mod |= Mod1Mask;

    if (event->state & GDK_SHIFT_MASK)
            mod |= ShiftMask;

    if (event->state & GDK_MOD5_MASK)
            mod |= Mod5Mask;

    if (event->state & GDK_MOD4_MASK)
            mod |= Mod4Mask;

    if (event->direction == GDK_SCROLL_UP)
        controls->hotkey.key = 4;
    else if (event->direction == GDK_SCROLL_DOWN)
        controls->hotkey.key = 5;
    else if (event->direction == GDK_SCROLL_LEFT)
        controls->hotkey.key = 6;
    else if (event->direction == GDK_SCROLL_RIGHT)
        controls->hotkey.key = 7;
    else return FALSE;

    controls->hotkey.mask = mod;
        controls->hotkey.type = TYPE_MOUSE;
    set_keytext(controls->keytext, controls->hotkey.key, controls->hotkey.mask, controls->hotkey.type);
    if (controls->next == NULL)
        add_callback (NULL, (gpointer) controls);
    return TRUE;
}

KeyControls* add_event_controls(KeyControls* list,
                GtkWidget *grid,
                int row,
                HotkeyConfiguration *hotkey)
{
    KeyControls *controls;
    int i;

    controls = (KeyControls*) g_malloc(sizeof(KeyControls));
    controls->next = NULL;
    controls->prev = list;
    controls->first = list->first;
    controls->grid = grid;
    list->next = controls;

    if (hotkey)
    {
        controls->hotkey.key = hotkey->key;
        controls->hotkey.mask = hotkey->mask;
        controls->hotkey.type = hotkey->type;
        controls->hotkey.event = hotkey->event;
        if (controls->hotkey.key == 0)
            controls->hotkey.mask = 0;
    } else {
        controls->hotkey.key = 0;
        controls->hotkey.mask = 0;
        controls->hotkey.type = TYPE_KEY;
        controls->hotkey.event = (EVENT) 0;
    }

    controls->combobox = gtk_combo_box_text_new();
    gtk_widget_set_hexpand(controls->combobox, TRUE);
    for (i=0;i<EVENT_MAX;i++)
    {
        gtk_combo_box_text_append_text((GtkComboBoxText *) controls->combobox, _(event_desc[i]));
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(controls->combobox), controls->hotkey.event);
    gtk_grid_attach (GTK_GRID (grid), controls->combobox, 0, row, 1, 1);


    controls->keytext = gtk_entry_new ();
    gtk_widget_set_hexpand (controls->keytext, TRUE);
    gtk_grid_attach (GTK_GRID (grid), controls->keytext, 1, row, 1, 1);
    gtk_editable_set_editable(GTK_EDITABLE(controls->keytext), FALSE);


    set_keytext(controls->keytext, controls->hotkey.key, controls->hotkey.mask, controls->hotkey.type);
    g_signal_connect((gpointer)controls->keytext, "key_press_event",
                         G_CALLBACK(on_entry_key_press_event), controls);
    g_signal_connect((gpointer)controls->keytext, "key_release_event",
                         G_CALLBACK(on_entry_key_release_event), controls);
    g_signal_connect((gpointer)controls->keytext, "button_press_event",
                         G_CALLBACK(on_entry_button_press_event), controls);
    g_signal_connect((gpointer)controls->keytext, "scroll_event",
                         G_CALLBACK(on_entry_scroll_event), controls);


    controls->button = gtk_button_new();
    gtk_button_set_image (GTK_BUTTON (controls->button),
     gtk_image_new_from_icon_name ("edit-delete", GTK_ICON_SIZE_BUTTON));
    gtk_grid_attach (GTK_GRID (grid), controls->button, 2, row, 1, 1);
    g_signal_connect (G_OBJECT (controls->button), "clicked",
            G_CALLBACK (clear_keyboard), controls);

    gtk_widget_grab_focus(GTK_WIDGET(controls->keytext));
    return controls;
}

void show_configure ()
{
    KeyControls *first_controls, *current_controls;
    GtkWidget *window;
    GtkWidget *main_vbox, *hbox;
    GtkWidget *alignment;
    GtkWidget *frame;
    GtkWidget *label;
    GtkWidget *image;
    GtkWidget *grid;
    GtkWidget *button_box, *button;
    PluginConfig* plugin_cfg;
    HotkeyConfiguration *hotkey, temphotkey;
    int i;

    load_config ( );

    plugin_cfg = get_config();

    ungrab_keys();

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), _("Global Hotkey Plugin Configuration"));
    gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
    gtk_container_set_border_width (GTK_CONTAINER (window), 5);

    main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_add (GTK_CONTAINER (window), main_vbox);

    alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
    gtk_box_pack_start (GTK_BOX (main_vbox), alignment, FALSE, TRUE, 0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 4, 0, 0, 0);
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_container_add (GTK_CONTAINER (alignment), hbox);
    image = gtk_image_new_from_icon_name ("dialog-information", GTK_ICON_SIZE_DIALOG);
    gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, TRUE, 0);
    label = gtk_label_new (_("Press a key combination inside a text field.\nYou can also bind mouse buttons."));
    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label), _("Hotkeys:"));
    frame = gtk_frame_new (NULL);
    gtk_frame_set_label_widget (GTK_FRAME (frame), label);
    gtk_box_pack_start (GTK_BOX (main_vbox), frame, TRUE, TRUE, 0);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
    alignment = gtk_alignment_new (0, 0, 1, 0);
    gtk_container_add (GTK_CONTAINER (frame), alignment);
    gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 3, 3, 3, 3);

    grid = gtk_grid_new ();
    gtk_grid_set_column_spacing (GTK_GRID (grid), 2);
    gtk_container_add (GTK_CONTAINER (alignment), grid);

    label = gtk_label_new (NULL);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
    gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
    gtk_label_set_markup (GTK_LABEL (label),
            _("<b>Action:</b>"));
    gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

    label = gtk_label_new (NULL);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
    gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
    gtk_label_set_markup (GTK_LABEL (label),
            _("<b>Key Binding:</b>"));
    gtk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);


    hotkey = &(plugin_cfg->first);
    i = 1;
    first_controls = (KeyControls*) g_malloc(sizeof(KeyControls));
    first_controls->next = NULL;
    first_controls->prev = NULL;
    first_controls->grid = grid;
    first_controls->button = NULL;
    first_controls->combobox = NULL;
    first_controls->keytext = NULL;
    first_controls->first = first_controls;
    first_controls->hotkey.key = 0;
    first_controls->hotkey.mask = 0;
    first_controls->hotkey.event = (EVENT) 0;
    first_controls->hotkey.type = TYPE_KEY;
    current_controls = first_controls;
    if (hotkey -> key != 0)
    {
        while (hotkey)
        {
            current_controls = add_event_controls(current_controls, grid, i, hotkey);
            hotkey = hotkey->next;
            i++;
        }
    }
    temphotkey.key = 0;
    temphotkey.mask = 0;
    temphotkey.type = TYPE_KEY;
    if (current_controls != first_controls)
        temphotkey.event = (EVENT) (current_controls->hotkey.event + 1);
    else
        temphotkey.event = (EVENT) 0;
    if (temphotkey.event >= EVENT_MAX)
        temphotkey.event = (EVENT) 0;
    add_event_controls(current_controls, grid, i, &temphotkey);



    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, TRUE, 0);

    button_box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start (GTK_BOX (hbox), button_box, FALSE, TRUE, 0);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (button_box), GTK_BUTTONBOX_START);
    gtk_box_set_spacing (GTK_BOX (button_box), 4);

    button = audgui_button_new (_("_Add"), "list-add", NULL, NULL);
    gtk_container_add (GTK_CONTAINER (button_box), button);
    g_signal_connect (G_OBJECT (button), "clicked",
            G_CALLBACK (add_callback), first_controls);

    button_box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start (GTK_BOX (hbox), button_box, TRUE, TRUE, 0);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (button_box), GTK_BUTTONBOX_END);
    gtk_box_set_spacing (GTK_BOX (button_box), 4);

    button = audgui_button_new (_("_Cancel"), "process-stop", NULL, NULL);
    gtk_container_add (GTK_CONTAINER (button_box), button);
    g_signal_connect (G_OBJECT (button), "clicked",
            G_CALLBACK (cancel_callback), NULL);

    button = audgui_button_new (_("_Set"), "system-run", NULL, NULL);
    gtk_container_add (GTK_CONTAINER (button_box), button);
    g_signal_connect (G_OBJECT (button), "clicked",
            G_CALLBACK (ok_callback), first_controls);

    g_signal_connect (G_OBJECT (window), "destroy",
        G_CALLBACK (destroy_callback), first_controls);

    gtk_widget_show_all (GTK_WIDGET (window));
}

static void clear_keyboard (GtkWidget *widget, gpointer data)
{
    KeyControls *controls= (KeyControls*)data;

    if ((controls->next == NULL) && (controls->prev->keytext == NULL))
    {
        controls->hotkey.key = 0;
        controls->hotkey.mask = 0;
        controls->hotkey.type = TYPE_KEY;
        set_keytext(controls->keytext, 0, 0, TYPE_KEY);
        gtk_combo_box_set_active( GTK_COMBO_BOX(controls->combobox), 0);
        return;
    }

    if (controls->prev)
    {
        KeyControls* c;
        GtkWidget* grid;
        int row;

        gtk_widget_destroy(GTK_WIDGET(controls->button));
        gtk_widget_destroy(GTK_WIDGET(controls->keytext));
        gtk_widget_destroy(GTK_WIDGET(controls->combobox));

        row=0;
        c = controls->first;
        while (c) {
            if (c == controls) break;
            row++;
            c = c->next;
        }
        c = controls->next;
        controls->prev->next = controls->next;
        if (controls->next)
            controls->next->prev = controls->prev;
        g_free(controls);
        if (c) grid = c->grid; else grid = NULL;
        while (c)
        {
            g_object_ref(c->combobox);
            g_object_ref(c->keytext);
            g_object_ref(c->button);

            gtk_container_remove( GTK_CONTAINER(c->grid) , c->combobox);
            gtk_container_remove( GTK_CONTAINER(c->grid) , c->keytext);
            gtk_container_remove( GTK_CONTAINER(c->grid) , c->button);

            gtk_grid_attach (GTK_GRID (c->grid), c->combobox, 0, row, 1, 1);
            gtk_grid_attach (GTK_GRID (c->grid), c->keytext, 1, row, 1, 1);
            gtk_grid_attach (GTK_GRID (c->grid), c->button, 2, row, 1, 1);

            g_object_unref(c->combobox);
            g_object_unref(c->keytext);
            g_object_unref(c->button);

            c = c->next;
            row++;
        }
        if (grid)
            gtk_widget_show_all (GTK_WIDGET (grid));

        return;
    }
}

void add_callback (GtkWidget *widget, gpointer data)
{
    KeyControls* controls = (KeyControls*)data;
    HotkeyConfiguration temphotkey;
    int count;
    if (controls == NULL) return;
    if ((controls->next == NULL)&&(controls->hotkey.event+1 == EVENT_MAX)) return;
    controls = controls->first;
    if (controls == NULL) return;
    count = 1;
    while (controls->next) {
        controls = controls->next;
        count = count + 1;
    }
    temphotkey.key = 0;
    temphotkey.mask = 0;
    temphotkey.type = TYPE_KEY;
    temphotkey.event = (EVENT) (controls->hotkey.event + 1);
    if (temphotkey.event >= EVENT_MAX)
        temphotkey.event = (EVENT) 0;
    add_event_controls(controls, controls->grid, count, &temphotkey);
    gtk_widget_show_all (GTK_WIDGET (controls->grid));
}

void destroy_callback (GtkWidget *widget, gpointer data)
{
    KeyControls* controls = (KeyControls*)data;
    if (is_loaded())
    {
        grab_keys ();
    }
    while (controls) {
        KeyControls *old;
        old = controls;
        controls = controls->next;
        g_free(old);
    }
}

void cancel_callback (GtkWidget *widget, gpointer data)
{
    gtk_widget_destroy (gtk_widget_get_toplevel (GTK_WIDGET (widget)));
}

void ok_callback (GtkWidget *widget, gpointer data)
{
    KeyControls *controls = (KeyControls*)data;
    PluginConfig* plugin_cfg = get_config();
    HotkeyConfiguration *hotkey;

    hotkey = &(plugin_cfg->first);
    hotkey = hotkey->next;
    while (hotkey)
    {
        HotkeyConfiguration * old;
        old = hotkey;
        hotkey = hotkey->next;
        g_free(old);
    }
    plugin_cfg->first.next = NULL;
    plugin_cfg->first.key = 0;
    plugin_cfg->first.event = (EVENT) 0;
    plugin_cfg->first.mask = 0;

    hotkey = &(plugin_cfg->first);
    while (controls)
    {
        if (controls->hotkey.key) {
            if (hotkey->key) {
                hotkey->next = g_new(HotkeyConfiguration, 1);
                hotkey = hotkey->next;
                hotkey->next = NULL;
            }
            hotkey->key = controls->hotkey.key;
            hotkey->mask = controls->hotkey.mask;
            hotkey->event = (EVENT) gtk_combo_box_get_active( GTK_COMBO_BOX(controls->combobox) );
            hotkey->type = controls->hotkey.type;
        }
        controls = controls->next;
    }

    save_config ( );

    gtk_widget_destroy (gtk_widget_get_toplevel (GTK_WIDGET (widget)));
}
