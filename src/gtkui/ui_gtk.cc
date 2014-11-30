/*
 * ui_gtk.c
 * Copyright 2009-2012 William Pitcock, Tomasz Moń, Michał Lipski, and John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <libaudcore/runtime.h>
#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>
#include <libaudcore/runtime.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>

#include "gtkui.h"
#include "layout.h"
#include "ui_playlist_notebook.h"
#include "ui_playlist_widget.h"
#include "ui_infoarea.h"
#include "ui_statusbar.h"
#include "playlist_util.h"

static const char * const gtkui_defaults[] = {
 "infoarea_show_vis", "TRUE",
 "infoarea_visible", "TRUE",
 "menu_visible", "TRUE",
 "playlist_tabs_visible", "TRUE",
 "statusbar_visible", "TRUE",
 "entry_count_visible", "FALSE",
 "close_button_visible", "TRUE",

 "autoscroll", "TRUE",
 "playlist_columns", "title artist album queued length",
 "playlist_headers", "TRUE",
 "show_remaining_time", "FALSE",
 "step_size", "5",

 "player_x", "-1000",
 "player_y", "-1000",
 "player_width", "760",
 "player_height", "460",

 nullptr
};

class GtkUI : public IfacePlugin
{
public:
    static constexpr PluginInfo info = {
        N_("GTK Interface"),
        PACKAGE,
        nullptr,
        & gtkui_prefs
    };

    constexpr GtkUI () : IfacePlugin (info) {}

    bool init ();
    void cleanup ();
    void show (bool show);

    void run ()
        { gtk_main (); }
    void quit ()
        { gtk_main_quit (); }

    void show_about_window ()
        { audgui_show_about_window (); }
    void hide_about_window ()
        { audgui_hide_about_window (); }
    void show_filebrowser (bool open)
        { audgui_run_filebrowser (open); }
    void hide_filebrowser ()
        { audgui_hide_filebrowser (); }
    void show_jump_to_song ()
        { audgui_jump_to_track (); }
    void hide_jump_to_song ()
        { audgui_jump_to_track_hide (); }
    void show_prefs_window ()
        { audgui_show_prefs_window (); }
    void hide_prefs_window ()
        { audgui_hide_prefs_window (); }
    void plugin_menu_add (AudMenuID id, void func (), const char * name, const char * icon)
        { audgui_plugin_menu_add (id, func, name, icon); }
    void plugin_menu_remove (AudMenuID id, void func ())
        { audgui_plugin_menu_remove (id, func); }
};

EXPORT GtkUI aud_plugin_instance;

static PluginHandle * search_tool;

static GtkWidget * volume;
static gboolean volume_slider_is_moving = FALSE;
static unsigned update_volume_timeout_source = 0;
static unsigned long volume_change_handler_id;

static GtkAccelGroup * accel;

static GtkWidget * window, * vbox_outer, * menu_box, * menu, * toolbar, * vbox,
 * infoarea, * statusbar;
static GtkToolItem * menu_button, * search_button, * button_play, * button_stop,
 * button_shuffle, * button_repeat;
static GtkWidget * slider, * label_time;
static GtkWidget * menu_main, * menu_rclick, * menu_tab;

static gboolean slider_is_moving = FALSE;
static int slider_seek_time = -1;
static unsigned delayed_title_change_source = 0;
static unsigned update_song_timeout_source = 0;

static void save_window_size (void)
{
    int x, y, w, h;
    gtk_window_get_position ((GtkWindow *) window, & x, & y);
    gtk_window_get_size ((GtkWindow *) window, & w, & h);

    aud_set_int ("gtkui", "player_x", x);
    aud_set_int ("gtkui", "player_y", y);
    aud_set_int ("gtkui", "player_width", w);
    aud_set_int ("gtkui", "player_height", h);
}

static void restore_window_size (void)
{
    int x = aud_get_int ("gtkui", "player_x");
    int y = aud_get_int ("gtkui", "player_y");
    int w = aud_get_int ("gtkui", "player_width");
    int h = aud_get_int ("gtkui", "player_height");

    gtk_window_set_default_size ((GtkWindow *) window, w, h);

    if (x > -1000 && y > -1000)
        gtk_window_move ((GtkWindow *) window, x, y);
}

static gboolean window_delete()
{
    gboolean handle = FALSE;

    hook_call("window close", &handle);

    if (handle)
        return TRUE;

    aud_quit ();
    return TRUE;
}

static void button_open_pressed (void)
{
    audgui_run_filebrowser (TRUE);
}

static void button_add_pressed (void)
{
    audgui_run_filebrowser (FALSE);
}

void set_ab_repeat_a (void)
{
    if (! aud_drct_get_playing ())
        return;

    int a, b;
    aud_drct_get_ab_repeat (a, b);
    a = aud_drct_get_time ();
    aud_drct_set_ab_repeat (a, b);
}

void set_ab_repeat_b (void)
{
    if (! aud_drct_get_playing ())
        return;

    int a, b;
    aud_drct_get_ab_repeat (a, b);
    b = aud_drct_get_time ();
    aud_drct_set_ab_repeat (a, b);
}

void clear_ab_repeat (void)
{
    aud_drct_set_ab_repeat (-1, -1);
}

static gboolean title_change_cb (void)
{
    if (delayed_title_change_source)
    {
        g_source_remove (delayed_title_change_source);
        delayed_title_change_source = 0;
    }

    if (aud_drct_get_playing ())
    {
        if (aud_drct_get_ready ())
        {
            String title = aud_drct_get_title ();
            gtk_window_set_title ((GtkWindow *) window,
             str_printf (_("%s - Audacious"), (const char *) title));
        }
        else
            gtk_window_set_title ((GtkWindow *) window, _("Buffering ..."));
    }
    else
        gtk_window_set_title ((GtkWindow *) window, _("Audacious"));

    return FALSE;
}

void GtkUI::show (bool show)
{
    if (show)
    {
        if (! gtk_widget_get_visible (window))
            restore_window_size ();

        gtk_window_present ((GtkWindow *) window);
    }
    else
    {
        if (gtk_widget_get_visible (window))
            save_window_size ();

        gtk_widget_hide (window);
    }

    show_hide_infoarea_vis ();
}

static void append_str (char * buf, int bufsize, const char * str)
{
    snprintf (buf + strlen (buf), bufsize - strlen (buf), "%s", str);
}

static void set_time_label (int time, int len)
{
    char s[128] = "<b>";

    if (len && aud_get_bool ("gtkui", "show_remaining_time"))
        append_str (s, sizeof s, str_format_time (len - time));
    else
        append_str (s, sizeof s, str_format_time (time));

    if (len)
    {
        append_str (s, sizeof s, " / ");
        append_str (s, sizeof s, str_format_time (len));

        int a, b;
        aud_drct_get_ab_repeat (a, b);

        if (a >= 0)
        {
            append_str (s, sizeof s, " A=");
            append_str (s, sizeof s, str_format_time (a));
        }

        if (b >= 0)
        {
            append_str (s, sizeof s, " B=");
            append_str (s, sizeof s, str_format_time (b));
        }
    }

    append_str (s, sizeof s, "</b>");

    /* only update label if necessary */
    if (strcmp (gtk_label_get_label ((GtkLabel *) label_time), s))
        gtk_label_set_markup ((GtkLabel *) label_time, s);
}

static void set_slider (int time)
{
    gtk_range_set_value ((GtkRange *) slider, time);
}

static gboolean time_counter_cb (void)
{
    if (slider_is_moving)
        return TRUE;

    slider_seek_time = -1;  // delayed reset to avoid seeking twice

    int time = aud_drct_get_time ();
    int length = aud_drct_get_length ();

    if (length > 0)
        set_slider (time);

    set_time_label (time, length);

    return TRUE;
}

static void do_seek (int time)
{
    int length = aud_drct_get_length ();
    time = aud::clamp (time, 0, length);

    set_slider (time);
    set_time_label (time, length);
    aud_drct_seek (time);

    // Trick: Unschedule and then schedule the update function.  This gives the
    // player 1/4 second to perform the seek before we update the display again,
    // in an attempt to reduce flickering.
    if (update_song_timeout_source)
    {
        g_source_remove (update_song_timeout_source);
        update_song_timeout_source = g_timeout_add (250, (GSourceFunc) time_counter_cb, nullptr);
    }
}

static gboolean ui_slider_change_value_cb (GtkRange * range,
 GtkScrollType scroll, double value)
{
    int length = aud_drct_get_length ();
    int time = aud::clamp ((int) value, 0, length);

    set_time_label (time, length);

    if (slider_is_moving)
        slider_seek_time = time;
    else if (time != slider_seek_time)  // avoid seeking twice
        do_seek (time);

    return FALSE;
}

static gboolean ui_slider_button_press_cb(GtkWidget * widget, GdkEventButton * event, void * user_data)
{
    gboolean primary_warps = FALSE;
    g_object_get (gtk_widget_get_settings (widget),
     "gtk-primary-button-warps-slider", & primary_warps, nullptr);

    if (event->button == 1 && ! primary_warps)
        event->button = 2;

    slider_is_moving = TRUE;
    return FALSE;
}

static gboolean ui_slider_button_release_cb(GtkWidget * widget, GdkEventButton * event, void * user_data)
{
    gboolean primary_warps = FALSE;
    g_object_get (gtk_widget_get_settings (widget),
     "gtk-primary-button-warps-slider", & primary_warps, nullptr);

    if (event->button == 1 && ! primary_warps)
        event->button = 2;

    if (slider_seek_time != -1)
        do_seek (slider_seek_time);

    slider_is_moving = FALSE;
    return FALSE;
}

static gboolean ui_volume_value_changed_cb(GtkButton * button, double volume, void * user_data)
{
    aud_drct_set_volume_main (volume);
    return TRUE;
}

static void ui_volume_pressed_cb(GtkButton *button, void * user_data)
{
    volume_slider_is_moving = TRUE;
}

static void ui_volume_released_cb(GtkButton *button, void * user_data)
{
    volume_slider_is_moving = FALSE;
}

static gboolean ui_volume_slider_update(void * data)
{
    if (volume_slider_is_moving || data == nullptr)
        return TRUE;

    int volume = aud_drct_get_volume_main ();

    if (volume == (int) gtk_scale_button_get_value(GTK_SCALE_BUTTON(data)))
        return TRUE;

    g_signal_handler_block(data, volume_change_handler_id);
    gtk_scale_button_set_value(GTK_SCALE_BUTTON(data), volume);
    g_signal_handler_unblock(data, volume_change_handler_id);

    return TRUE;
}

static void set_slider_length (int length)
{
    if (length > 0)
    {
        gtk_range_set_range ((GtkRange *) slider, 0, length);
        gtk_widget_show (slider);
    }
    else
        gtk_widget_hide (slider);
}

void update_step_size (void)
{
    double step_size = aud_get_double ("gtkui", "step_size");
    gtk_range_set_increments ((GtkRange *) slider, step_size * 1000, step_size * 1000);
}

static void pause_cb (void)
{
    gtk_tool_button_set_icon_name ((GtkToolButton *) button_play,
     aud_drct_get_paused () ? "media-playback-start" : "media-playback-pause");
}

static void ui_playback_begin (void)
{
    pause_cb ();
    gtk_widget_set_sensitive ((GtkWidget *) button_stop, TRUE);

    if (delayed_title_change_source)
        g_source_remove (delayed_title_change_source);

    /* If "title change" is not called by 1/4 second after starting playback,
     * show "Buffering ..." as the window title. */
    delayed_title_change_source = g_timeout_add (250, (GSourceFunc)
     title_change_cb, nullptr);
}

static void ui_playback_ready (void)
{
    title_change_cb ();
    set_slider_length (aud_drct_get_length ());
    time_counter_cb ();

    /* update time counter 4 times a second */
    if (! update_song_timeout_source)
        update_song_timeout_source = g_timeout_add (250, (GSourceFunc)
         time_counter_cb, nullptr);

    gtk_widget_show (label_time);
}

static void ui_playback_stop (void)
{
    if (update_song_timeout_source)
    {
        g_source_remove(update_song_timeout_source);
        update_song_timeout_source = 0;
    }

    if (delayed_title_change_source)
        g_source_remove (delayed_title_change_source);

    /* Don't update the window title immediately; we may be about to start
     * another song. */
    delayed_title_change_source = g_idle_add ((GSourceFunc) title_change_cb,
     nullptr);

    gtk_tool_button_set_icon_name ((GtkToolButton *) button_play, "media-playback-start");
    gtk_widget_set_sensitive ((GtkWidget *) button_stop, FALSE);
    gtk_widget_hide (slider);
    gtk_widget_hide (label_time);
}

static GtkToolItem * toolbar_button_add (GtkWidget * toolbar,
 void (* callback) (void), const char * icon)
{
    GtkToolItem * item = gtk_tool_button_new (nullptr, nullptr);
    gtk_tool_button_set_icon_name ((GtkToolButton *) item, icon);
    gtk_toolbar_insert ((GtkToolbar *) toolbar, item, -1);
    g_signal_connect (item, "clicked", callback, nullptr);
    return item;
}

static GtkToolItem * toggle_button_new (const char * icon,
 void (* toggled) (GtkToggleToolButton * button))
{
    GtkToolItem * item = gtk_toggle_tool_button_new ();
    gtk_tool_button_set_icon_name ((GtkToolButton *) item, icon);
    g_signal_connect (item, "toggled", (GCallback) toggled, nullptr);
    return item;
}

static GtkWidget *markup_label_new(const char * str)
{
    GtkWidget *label = gtk_label_new(str);
    g_object_set(G_OBJECT(label), "use-markup", TRUE, nullptr);
    return label;
}

static void window_mapped_cb (GtkWidget * widget, void * unused)
{
    gtk_widget_grab_focus (playlist_get_treeview (aud_playlist_get_active ()));
}

static gboolean window_keypress_cb (GtkWidget * widget, GdkEventKey * event, void * unused)
{
    switch (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK))
    {
      case 0:
      {
        GtkWidget * focused = gtk_window_get_focus ((GtkWindow *) window);

        /* escape key returns focus to playlist */
        if (event->keyval == GDK_KEY_Escape)
        {
            if (! focused || ! gtk_widget_is_ancestor (focused, (GtkWidget *) UI_PLAYLIST_NOTEBOOK))
                gtk_widget_grab_focus (playlist_get_treeview (aud_playlist_get_active ()));

            return FALSE;
        }

        /* single-key shortcuts; must not interfere with text entry */
        if (focused && GTK_IS_ENTRY (focused))
            return FALSE;

        switch (event->keyval)
        {
        case 'z':
            aud_drct_pl_prev ();
            return TRUE;
        case 'x':
            aud_drct_play ();
            return TRUE;
        case 'c':
        case ' ':
            aud_drct_pause ();
            return TRUE;
        case 'v':
            aud_drct_stop ();
            return TRUE;
        case 'b':
            aud_drct_pl_next ();
            return TRUE;
        case GDK_KEY_Left:
            if (aud_drct_get_playing ())
                do_seek (aud_drct_get_time () - aud_get_double ("gtkui", "step_size") * 1000);
            return TRUE;
        case GDK_KEY_Right:
            if (aud_drct_get_playing ())
                do_seek (aud_drct_get_time () + aud_get_double ("gtkui", "step_size") * 1000);
            return TRUE;
        }

        return FALSE;
      }

      case GDK_CONTROL_MASK:
        switch (event->keyval)
        {
          case GDK_KEY_ISO_Left_Tab:
          case GDK_KEY_Tab:
            aud_playlist_set_active ((aud_playlist_get_active () + 1) %
             aud_playlist_count ());
            break;

          default:
            return FALSE;
        }
        break;
      case (GDK_CONTROL_MASK | GDK_SHIFT_MASK):
        switch (event->keyval)
        {
          case GDK_KEY_ISO_Left_Tab:
          case GDK_KEY_Tab:
            aud_playlist_set_active (aud_playlist_get_active () ?
             aud_playlist_get_active () - 1 : aud_playlist_count () - 1);
            break;
          default:
            return FALSE;
        }
        break;
      case GDK_MOD1_MASK:
        switch (event->keyval)
        {
          case GDK_KEY_Left:
            if (aud_drct_get_playing ())
                do_seek (aud_drct_get_time () - aud_get_double ("gtkui", "step_size") * 1000);
            break;
          case GDK_KEY_Right:
            if (aud_drct_get_playing ())
                do_seek (aud_drct_get_time () + aud_get_double ("gtkui", "step_size") * 1000);
            break;
          default:
            return FALSE;
        }
      default:
        return FALSE;
    }

    return TRUE;
}

static gboolean playlist_keypress_cb (GtkWidget * widget, GdkEventKey * event, void * unused)
{
    switch (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK))
    {
    case 0:
        switch (event->keyval)
        {
        case GDK_KEY_Escape:
            ui_playlist_notebook_position (aud::to_ptr (aud_playlist_get_active ()), nullptr);
            return TRUE;
        case GDK_KEY_Delete:
            playlist_delete_selected ();
            return TRUE;
        case GDK_KEY_Menu:
            popup_menu_rclick (0, event->time);
            return TRUE;
        }

        break;
    case GDK_CONTROL_MASK:
        switch (event->keyval)
        {
        case 'x':
            playlist_cut ();
            return TRUE;
        case 'c':
            playlist_copy ();
            return TRUE;
        case 'v':
            playlist_paste ();
            return TRUE;
        case 'a':
            aud_playlist_select_all (aud_playlist_get_active (), TRUE);
            return TRUE;
        }

        break;
    }

    return FALSE;
}

static void update_toggles (void * data, void * user)
{
    gtk_toggle_tool_button_set_active ((GtkToggleToolButton *) button_repeat,
     aud_get_bool (nullptr, "repeat"));
    gtk_toggle_tool_button_set_active ((GtkToggleToolButton *) button_shuffle,
     aud_get_bool (nullptr, "shuffle"));
}

static void toggle_repeat (GtkToggleToolButton * button)
{
    aud_set_bool (nullptr, "repeat", gtk_toggle_tool_button_get_active (button));
}

static void toggle_shuffle (GtkToggleToolButton * button)
{
    aud_set_bool (nullptr, "shuffle", gtk_toggle_tool_button_get_active (button));
}

static void toggle_search_tool (GtkToggleToolButton * button)
{
    aud_plugin_enable (search_tool, gtk_toggle_tool_button_get_active (button));
}

static bool search_tool_toggled (PluginHandle * plugin, void *)
{
    gtk_toggle_tool_button_set_active ((GtkToggleToolButton *) search_button,
     aud_plugin_get_enabled (plugin));
    return true;
}

static void config_save (void)
{
    if (gtk_widget_get_visible (window))
        save_window_size ();

    layout_save ();
    pw_col_save ();
}

static void ui_hooks_associate(void)
{
    hook_associate ("title change", (HookFunction) title_change_cb, nullptr);
    hook_associate ("playback begin", (HookFunction) ui_playback_begin, nullptr);
    hook_associate ("playback ready", (HookFunction) ui_playback_ready, nullptr);
    hook_associate ("playback pause", (HookFunction) pause_cb, nullptr);
    hook_associate ("playback unpause", (HookFunction) pause_cb, nullptr);
    hook_associate ("playback stop", (HookFunction) ui_playback_stop, nullptr);
    hook_associate ("playlist update", ui_playlist_notebook_update, nullptr);
    hook_associate ("playlist activate", ui_playlist_notebook_activate, nullptr);
    hook_associate ("playlist set playing", ui_playlist_notebook_set_playing, nullptr);
    hook_associate ("playlist position", ui_playlist_notebook_position, nullptr);
    hook_associate ("set shuffle", update_toggles, nullptr);
    hook_associate ("set repeat", update_toggles, nullptr);
    hook_associate ("config save", (HookFunction) config_save, nullptr);
}

static void ui_hooks_disassociate(void)
{
    hook_dissociate ("title change", (HookFunction) title_change_cb);
    hook_dissociate ("playback begin", (HookFunction) ui_playback_begin);
    hook_dissociate ("playback ready", (HookFunction) ui_playback_ready);
    hook_dissociate ("playback pause", (HookFunction) pause_cb);
    hook_dissociate ("playback unpause", (HookFunction) pause_cb);
    hook_dissociate ("playback stop", (HookFunction) ui_playback_stop);
    hook_dissociate("playlist update", ui_playlist_notebook_update);
    hook_dissociate ("playlist activate", ui_playlist_notebook_activate);
    hook_dissociate ("playlist set playing", ui_playlist_notebook_set_playing);
    hook_dissociate ("playlist position", ui_playlist_notebook_position);
    hook_dissociate ("set shuffle", update_toggles);
    hook_dissociate ("set repeat", update_toggles);
    hook_dissociate ("config save", (HookFunction) config_save);
}

static void add_dock_plugin (PluginHandle * plugin, void * unused)
{
    GtkWidget * widget = (GtkWidget *) aud_plugin_get_gtk_widget (plugin);
    if (widget)
        layout_add (plugin, widget);
}

static void remove_dock_plugin (PluginHandle * plugin, void * unused)
{
    layout_remove (plugin);
}

static void add_dock_plugins (void)
{
    for (PluginHandle * plugin : aud_plugin_list (PluginType::General))
    {
        if (aud_plugin_get_enabled (plugin))
            add_dock_plugin (plugin, nullptr);
    }

    for (PluginHandle * plugin : aud_plugin_list (PluginType::Vis))
    {
        if (aud_plugin_get_enabled (plugin))
            add_dock_plugin (plugin, nullptr);
    }

    hook_associate ("dock plugin enabled", (HookFunction) add_dock_plugin, nullptr);
    hook_associate ("dock plugin disabled", (HookFunction) remove_dock_plugin, nullptr);
}

static void remove_dock_plugins (void)
{
    for (PluginHandle * plugin : aud_plugin_list (PluginType::General))
    {
        if (aud_plugin_get_enabled (plugin))
            remove_dock_plugin (plugin, nullptr);
    }

    for (PluginHandle * plugin : aud_plugin_list (PluginType::Vis))
    {
        if (aud_plugin_get_enabled (plugin))
            remove_dock_plugin (plugin, nullptr);
    }

    hook_dissociate ("dock plugin enabled", (HookFunction) add_dock_plugin);
    hook_dissociate ("dock plugin disabled", (HookFunction) remove_dock_plugin);
}

bool GtkUI::init ()
{
    if (aud_get_mainloop_type () != MainloopType::GLib)
        return false;

    audgui_init ();

    search_tool = aud_plugin_lookup_basename ("search-tool");

    aud_config_set_defaults ("gtkui", gtkui_defaults);

    pw_col_init ();

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(window_delete), nullptr);

    accel = gtk_accel_group_new ();
    gtk_window_add_accel_group ((GtkWindow *) window, accel);

    vbox_outer = gtk_vbox_new (FALSE, 0);
    gtk_container_add ((GtkContainer *) window, vbox_outer);

    menu_box = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start ((GtkBox *) vbox_outer, menu_box, FALSE, FALSE, 0);

    toolbar = gtk_toolbar_new ();
    gtk_toolbar_set_style ((GtkToolbar *) toolbar, GTK_TOOLBAR_ICONS);
    gtk_box_pack_start ((GtkBox *) vbox_outer, toolbar, FALSE, FALSE, 0);

    /* search button */
    if (search_tool)
    {
        search_button = toggle_button_new ("edit-find", toggle_search_tool);
        gtk_toolbar_insert ((GtkToolbar *) toolbar, search_button, -1);
        gtk_toggle_tool_button_set_active ((GtkToggleToolButton *) search_button,
         aud_plugin_get_enabled (search_tool));
        aud_plugin_add_watch (search_tool, search_tool_toggled, nullptr);
    }

    /* playback buttons */
    toolbar_button_add (toolbar, button_open_pressed, "document-open");
    toolbar_button_add (toolbar, button_add_pressed, "list-add");
    button_play = toolbar_button_add (toolbar, aud_drct_play_pause, "media-playback-start");
    button_stop = toolbar_button_add (toolbar, aud_drct_stop, "media-playback-stop");
    toolbar_button_add (toolbar, aud_drct_pl_prev, "media-skip-backward");
    toolbar_button_add (toolbar, aud_drct_pl_next, "media-skip-forward");

    /* time slider and label */
    GtkToolItem * boxitem1 = gtk_tool_item_new ();
    gtk_tool_item_set_expand (boxitem1, TRUE);
    gtk_toolbar_insert ((GtkToolbar *) toolbar, boxitem1, -1);

    GtkWidget * box1 = gtk_hbox_new (FALSE, 0);
    gtk_container_add ((GtkContainer *) boxitem1, box1);

    slider = gtk_hscale_new (nullptr);
    gtk_scale_set_draw_value(GTK_SCALE(slider), FALSE);
    gtk_widget_set_size_request(slider, 120, -1);
    gtk_widget_set_can_focus(slider, FALSE);
    gtk_box_pack_start ((GtkBox *) box1, slider, TRUE, TRUE, 6);

    update_step_size ();

    label_time = markup_label_new(nullptr);
    gtk_box_pack_end ((GtkBox *) box1, label_time, FALSE, FALSE, 6);

    gtk_widget_set_no_show_all (slider, TRUE);
    gtk_widget_set_no_show_all (label_time, TRUE);

    /* repeat and shuffle buttons */
    button_repeat = toggle_button_new ("media-playlist-repeat", toggle_repeat);
    gtk_toolbar_insert ((GtkToolbar *) toolbar, button_repeat, -1);
    button_shuffle = toggle_button_new ("media-playlist-shuffle", toggle_shuffle);
    gtk_toolbar_insert ((GtkToolbar *) toolbar, button_shuffle, -1);

    /* volume button */
    GtkToolItem * boxitem2 = gtk_tool_item_new ();
    gtk_toolbar_insert ((GtkToolbar *) toolbar, boxitem2, -1);

    GtkWidget * box2 = gtk_hbox_new (FALSE, 0);
    gtk_container_add ((GtkContainer *) boxitem2, box2);

    volume = gtk_volume_button_new();
    g_object_set ((GObject *) volume, "size", GTK_ICON_SIZE_LARGE_TOOLBAR, nullptr);
    gtk_button_set_relief(GTK_BUTTON(volume), GTK_RELIEF_NONE);
    gtk_scale_button_set_adjustment(GTK_SCALE_BUTTON(volume), GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 100, 1, 5, 0)));
    gtk_widget_set_can_focus(volume, FALSE);

    gtk_scale_button_set_value ((GtkScaleButton *) volume, aud_drct_get_volume_main ());

    gtk_box_pack_start ((GtkBox *) box2, volume, FALSE, FALSE, 0);

    /* main UI layout */
    layout_load ();

    GtkWidget * layout = layout_new ();
    gtk_box_pack_start ((GtkBox *) vbox_outer, layout, TRUE, TRUE, 0);

    vbox = gtk_vbox_new (FALSE, 6);
    layout_add_center (vbox);

    ui_playlist_notebook_new ();
    gtk_box_pack_start ((GtkBox *) vbox, (GtkWidget *) UI_PLAYLIST_NOTEBOOK, TRUE, TRUE, 0);

    /* optional UI elements */
    show_hide_menu ();
    show_hide_infoarea ();
    show_hide_statusbar ();

    AUDDBG("hooks associate\n");
    ui_hooks_associate();

    AUDDBG("playlist associate\n");
    ui_playlist_notebook_populate();

    g_signal_connect(slider, "change-value", G_CALLBACK(ui_slider_change_value_cb), nullptr);
    g_signal_connect(slider, "button-press-event", G_CALLBACK(ui_slider_button_press_cb), nullptr);
    g_signal_connect(slider, "button-release-event", G_CALLBACK(ui_slider_button_release_cb), nullptr);

    volume_change_handler_id = g_signal_connect(volume, "value-changed", G_CALLBACK(ui_volume_value_changed_cb), nullptr);
    g_signal_connect(volume, "pressed", G_CALLBACK(ui_volume_pressed_cb), nullptr);
    g_signal_connect(volume, "released", G_CALLBACK(ui_volume_released_cb), nullptr);
    update_volume_timeout_source = g_timeout_add(250, (GSourceFunc) ui_volume_slider_update, volume);

    g_signal_connect (window, "map-event", (GCallback) window_mapped_cb, nullptr);
    g_signal_connect (window, "key-press-event", (GCallback) window_keypress_cb, nullptr);
    g_signal_connect (UI_PLAYLIST_NOTEBOOK, "key-press-event", (GCallback) playlist_keypress_cb, nullptr);

    if (aud_drct_get_playing ())
    {
        ui_playback_begin ();
        if (aud_drct_get_ready ())
            ui_playback_ready ();
    }
    else
        ui_playback_stop ();

    title_change_cb ();

    gtk_widget_show_all (vbox_outer);

    update_toggles (nullptr, nullptr);

    menu_rclick = make_menu_rclick (accel);
    menu_tab = make_menu_tab (accel);

    add_dock_plugins ();

    return true;
}

void GtkUI::cleanup ()
{
    remove_dock_plugins ();

    if (menu_main)
        gtk_widget_destroy (menu_main);

    gtk_widget_destroy (menu_rclick);
    gtk_widget_destroy (menu_tab);

    if (update_song_timeout_source)
    {
        g_source_remove(update_song_timeout_source);
        update_song_timeout_source = 0;
    }

    if (update_volume_timeout_source)
    {
        g_source_remove(update_volume_timeout_source);
        update_volume_timeout_source = 0;
    }

    if (delayed_title_change_source)
    {
        g_source_remove (delayed_title_change_source);
        delayed_title_change_source = 0;
    }

    ui_hooks_disassociate();

    if (search_tool)
        aud_plugin_remove_watch (search_tool, search_tool_toggled, nullptr);

    gtk_widget_destroy (window);
    layout_cleanup ();

    audgui_cleanup ();
}

static void menu_position_cb (GtkMenu * menu, int * x, int * y, int * push, void * button)
{
    GtkAllocation alloc;
    int xorig, yorig, xwin, ywin;

    gtk_widget_get_allocation ((GtkWidget *) button, & alloc);
    gdk_window_get_origin (gtk_widget_get_window (window), & xorig, & yorig);
    gtk_widget_translate_coordinates ((GtkWidget *) button, window, 0, 0, & xwin, & ywin);

    * x = xorig + xwin;
    * y = yorig + ywin + alloc.height;
    * push = TRUE;
}

static void menu_button_cb (void)
{
    if (gtk_toggle_tool_button_get_active ((GtkToggleToolButton *) menu_button))
        gtk_menu_popup ((GtkMenu *) menu_main, nullptr, nullptr, menu_position_cb,
         menu_button, 0, gtk_get_current_event_time ());
    else
        gtk_widget_hide (menu_main);
}

static void menu_hide_cb (void)
{
    gtk_toggle_tool_button_set_active ((GtkToggleToolButton *) menu_button, FALSE);
}

void show_hide_menu (void)
{
    if (aud_get_bool ("gtkui", "menu_visible"))
    {
        /* remove menu button from toolbar and show menu bar */
        if (menu_main)
            gtk_widget_destroy (menu_main);
        if (menu_button)
            gtk_widget_destroy ((GtkWidget *) menu_button);

        if (! menu)
        {
            menu = make_menu_bar (accel);
            g_signal_connect (menu, "destroy", (GCallback) gtk_widget_destroyed,
             & menu);
            gtk_widget_show (menu);
            gtk_box_pack_start ((GtkBox *) menu_box, menu, TRUE, TRUE, 0);
        }
    }
    else
    {
        /* hide menu bar and add menu item to toolbar */
        if (menu)
            gtk_widget_destroy (menu);

        if (! menu_main)
        {
            menu_main = make_menu_main (accel);
            g_signal_connect (menu_main, "destroy", (GCallback)
             gtk_widget_destroyed, & menu_main);
            g_signal_connect (menu_main, "hide", (GCallback) menu_hide_cb, nullptr);
        }

        if (! menu_button)
        {
            menu_button = gtk_toggle_tool_button_new ();
            gtk_tool_button_set_icon_name ((GtkToolButton *) menu_button, "audacious");
            g_signal_connect (menu_button, "destroy", (GCallback)
             gtk_widget_destroyed, & menu_button);
            gtk_widget_show ((GtkWidget *) menu_button);
            gtk_toolbar_insert ((GtkToolbar *) toolbar, menu_button, 0);
            g_signal_connect (menu_button, "toggled", (GCallback) menu_button_cb, nullptr);
        }
    }
}

void show_hide_infoarea (void)
{
    gboolean show = aud_get_bool ("gtkui", "infoarea_visible");

    if (show && ! infoarea)
    {
        infoarea = ui_infoarea_new ();
        g_signal_connect (infoarea, "destroy", (GCallback) gtk_widget_destroyed, & infoarea);
        gtk_box_pack_end ((GtkBox *) vbox, infoarea, FALSE, FALSE, 0);
        gtk_widget_show_all (infoarea);

        show_hide_infoarea_vis ();
    }

    if (! show && infoarea)
    {
        gtk_widget_destroy (infoarea);
        infoarea = nullptr;
    }
}

void show_hide_infoarea_vis (void)
{
    /* only turn on visualization if interface is shown */
    ui_infoarea_show_vis (gtk_widget_get_visible (window) && aud_get_bool
     ("gtkui", "infoarea_show_vis"));
}

void show_hide_statusbar (void)
{
    gboolean show = aud_get_bool ("gtkui", "statusbar_visible");

    if (show && ! statusbar)
    {
        statusbar = ui_statusbar_new ();
        g_signal_connect (statusbar, "destroy", (GCallback) gtk_widget_destroyed, & statusbar);
        gtk_box_pack_end ((GtkBox *) vbox_outer, statusbar, FALSE, FALSE, 0);
        gtk_widget_show_all (statusbar);
    }

    if (! show && statusbar)
    {
        gtk_widget_destroy (statusbar);
        statusbar = nullptr;
    }
}

void popup_menu_rclick (unsigned button, uint32_t time)
{
    gtk_menu_popup ((GtkMenu *) menu_rclick, nullptr, nullptr, nullptr, nullptr, button,
     time);
}

void popup_menu_tab (unsigned button, uint32_t time, int playlist)
{
    menu_tab_playlist_id = aud_playlist_get_unique_id (playlist);
    gtk_menu_popup ((GtkMenu *) menu_tab, nullptr, nullptr, nullptr, nullptr, button, time);
}

void activate_search_tool (void)
{
    if (! search_tool)
        return;

    aud_plugin_enable (search_tool, true);
    layout_focus (search_tool);
}

void activate_playlist_manager (void)
{
    PluginHandle * manager = aud_plugin_lookup_basename ("playlist-manager");
    if (! manager)
        return;

    aud_plugin_enable (manager, true);
    layout_focus (manager);
}
