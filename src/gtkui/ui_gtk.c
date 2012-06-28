/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2011  Audacious development team
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

#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <audacious/debug.h>
#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/playlist.h>
#include <audacious/plugin.h>
#include <audacious/plugins.h>
#include <audacious/misc.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "config.h"
#include "gtkui.h"
#include "layout.h"
#include "ui_playlist_notebook.h"
#include "ui_playlist_widget.h"
#include "ui_infoarea.h"
#include "ui_statusbar.h"
#include "playlist_util.h"

static const gchar * const gtkui_defaults[] = {
 "infoarea_show_vis", "TRUE",
 "infoarea_visible", "TRUE",
 "menu_visible", "TRUE",
 "player_visible", "TRUE",
 "statusbar_visible", "TRUE",

 "autoscroll", "TRUE",
 "playlist_columns", "title artist album queued length",
 "playlist_headers", "TRUE",

 "player_x", "-1",
 "player_y", "-1",
 "player_width", "760",
 "player_height", "460",

 /* hidden settings */
 "always_on_top", "FALSE",
 "save_window_position", "TRUE",
 "show_song_titles", "TRUE",
 NULL};

static PluginHandle * search_tool;

static GtkWidget *volume;
static gboolean volume_slider_is_moving = FALSE;
static guint update_volume_timeout_source = 0;
static gulong volume_change_handler_id;

static GtkAccelGroup * accel;

static GtkWidget * window, * vbox_outer, * menu_box, * menu, * toolbar, * vbox,
 * infoarea, * statusbar;
static GtkToolItem * menu_button, * search_button, * button_play, * button_stop,
 * button_shuffle, * button_repeat;
static GtkWidget * slider, * label_time;
static GtkWidget * menu_main, * menu_rclick, * menu_tab;

static GtkWidget * error_win = NULL;

static gboolean slider_is_moving = FALSE;
static guint delayed_title_change_source = 0;
static guint update_song_timeout_source = 0;

static gboolean init (void);
static void cleanup (void);
static void ui_show (gboolean show);
static gboolean ui_is_shown (void);
static gboolean ui_is_focused (void);
static void ui_show_error (const gchar * text);

AUD_IFACE_PLUGIN
(
    .name = N_("GTK Interface"),
    .domain = PACKAGE,
    .init = init,
    .cleanup = cleanup,
    .show = ui_show,
    .is_shown = ui_is_shown,
    .is_focused = ui_is_focused,
    .show_error = ui_show_error,
    .show_filebrowser = audgui_run_filebrowser,
    .show_jump_to_track = audgui_jump_to_track,
    .run_gtk_plugin = (void (*) (void *, const gchar *)) layout_add,
    .stop_gtk_plugin = (void (*) (void *)) layout_remove,
)

static void save_window_size (void)
{
    gint x, y, w, h;
    gtk_window_get_position ((GtkWindow *) window, & x, & y);
    gtk_window_get_size ((GtkWindow *) window, & w, & h);

    aud_set_int ("gtkui", "player_x", x);
    aud_set_int ("gtkui", "player_y", y);
    aud_set_int ("gtkui", "player_width", w);
    aud_set_int ("gtkui", "player_height", h);
}

static gboolean window_delete()
{
    gboolean handle = FALSE;

    hook_call("window close", &handle);

    if (handle)
        return TRUE;

    aud_drct_quit ();
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

static void button_play_pressed (void)
{
    if (aud_drct_get_playing () && ! aud_drct_get_paused ())
        aud_drct_pause ();
    else
        aud_drct_play ();
}

static gboolean title_change_cb (void)
{
    if (delayed_title_change_source)
    {
        g_source_remove (delayed_title_change_source);
        delayed_title_change_source = 0;
    }

    if (aud_drct_get_playing () && aud_get_bool ("gtkui", "show_song_titles"))
    {
        if (aud_drct_get_ready ())
        {
            gchar * title = aud_drct_get_title ();
            gchar * title_s = g_strdup_printf (_("%s - Audacious"), title);
            gtk_window_set_title ((GtkWindow *) window, title_s);
            g_free (title_s);
            str_unref (title);
        }
        else
            gtk_window_set_title ((GtkWindow *) window, _("Buffering ..."));
    }
    else
        gtk_window_set_title ((GtkWindow *) window, _("Audacious"));

    return FALSE;
}

static void ui_show (gboolean show)
{
    aud_set_bool ("gtkui", "player_visible", show);

    if (show)
    {
        if (aud_get_bool ("gtkui", "save_window_position") && ! gtk_widget_get_visible (window))
        {
            gint x = aud_get_int ("gtkui", "player_x");
            gint y = aud_get_int ("gtkui", "player_y");
            gtk_window_move ((GtkWindow *) window, x, y);
        }

        gtk_window_present ((GtkWindow *) window);
    }
    else if (gtk_widget_get_visible (window))
    {
        if (aud_get_bool ("gtkui", "save_window_position"))
        {
            gint x, y;
            gtk_window_get_position ((GtkWindow *) window, & x, & y);
            aud_set_int ("gtkui", "player_x", x);
            aud_set_int ("gtkui", "player_y", y);
        }

        gtk_widget_hide (window);
    }
}

static gboolean ui_is_shown (void)
{
    return aud_get_bool ("gtkui", "player_visible");
}

static gboolean ui_is_focused (void)
{
/* gtk_window_is_active() is too unreliable, unfortunately. --jlindgren */
#if 0
    return gtk_window_is_active ((GtkWindow *) window);
#else
    return ui_is_shown ();
#endif
}

static void ui_show_error (const gchar * text)
{
    audgui_simple_message (& error_win, GTK_MESSAGE_ERROR, _("Error"), _(text));
}

static void set_time_label (gint time, gint len)
{
    gchar s[128];
    snprintf (s, sizeof s, "<b>");

    time /= 1000;

    if (time < 3600)
        snprintf (s + strlen (s), sizeof s - strlen (s), aud_get_bool (NULL,
         "leading_zero") ? "%02d:%02d" : "%d:%02d", time / 60, time % 60);
    else
        snprintf (s + strlen (s), sizeof s - strlen (s), "%d:%02d:%02d", time /
         3600, (time / 60) % 60, time % 60);

    if (len)
    {
        len /= 1000;

        if (len < 3600)
            snprintf (s + strlen (s), sizeof s - strlen (s), aud_get_bool (NULL,
             "leading_zero") ? " / %02d:%02d" : " / %d:%02d", len / 60, len % 60);
        else
            snprintf (s + strlen (s), sizeof s - strlen (s), " / %d:%02d:%02d",
             len / 3600, (len / 60) % 60, len % 60);
    }

    snprintf (s + strlen (s), sizeof s - strlen (s), "</b>");
    gtk_label_set_markup ((GtkLabel *) label_time, s);
}

static void set_slider (gint time)
{
    gtk_range_set_value ((GtkRange *) slider, time);
}

static gboolean time_counter_cb (void)
{
    if (slider_is_moving)
        return TRUE;

    gint time = aud_drct_get_time ();
    gint length = aud_drct_get_length ();

    if (length > 0)
        set_slider (time);

    set_time_label (time, length);

    return TRUE;
}

static void do_seek (gint time)
{
    set_slider (time);
    set_time_label (time, aud_drct_get_length ());
    aud_drct_seek (time);

    // Trick: Unschedule and then schedule the update function.  This gives the
    // player 1/4 second to perform the seek before we update the display again,
    // in an attempt to reduce flickering.
    if (update_song_timeout_source)
    {
        g_source_remove (update_song_timeout_source);
        update_song_timeout_source = g_timeout_add (250, (GSourceFunc) time_counter_cb, NULL);
    }
}

static gboolean ui_slider_change_value_cb(GtkRange * range, GtkScrollType scroll)
{
    gint value = gtk_range_get_value (range);
    set_time_label (value, aud_drct_get_length ());

    if (!slider_is_moving)
        do_seek (gtk_range_get_value (range));

    return FALSE;
}

static gboolean ui_slider_button_press_cb(GtkWidget * widget, GdkEventButton * event, gpointer user_data)
{
    slider_is_moving = TRUE;
    return FALSE;
}

static gboolean ui_slider_button_release_cb(GtkWidget * widget, GdkEventButton * event, gpointer user_data)
{
    slider_is_moving = FALSE;
    return FALSE;
}

static gboolean ui_volume_value_changed_cb(GtkButton * button, gdouble volume, gpointer user_data)
{
    aud_drct_set_volume((gint) volume, (gint) volume);

    return TRUE;
}

static void ui_volume_pressed_cb(GtkButton *button, gpointer user_data)
{
    volume_slider_is_moving = TRUE;
}

static void ui_volume_released_cb(GtkButton *button, gpointer user_data)
{
    volume_slider_is_moving = FALSE;
}

static gboolean ui_volume_slider_update(gpointer data)
{
    gint volume;

    if (volume_slider_is_moving || data == NULL)
        return TRUE;

    aud_drct_get_volume_main(&volume);

    if (volume == (gint) gtk_scale_button_get_value(GTK_SCALE_BUTTON(data)))
        return TRUE;

    g_signal_handler_block(data, volume_change_handler_id);
    gtk_scale_button_set_value(GTK_SCALE_BUTTON(data), volume);
    g_signal_handler_unblock(data, volume_change_handler_id);

    return TRUE;
}

static void set_slider_length (gint length)
{
    if (length > 0)
    {
        gtk_range_set_range ((GtkRange *) slider, 0, length);
        gtk_widget_show (slider);
    }
    else
        gtk_widget_hide (slider);
}

static void pause_cb (void)
{
    gtk_tool_button_set_stock_id ((GtkToolButton *) button_play,
     aud_drct_get_paused () ? GTK_STOCK_MEDIA_PLAY : GTK_STOCK_MEDIA_PAUSE);
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
     title_change_cb, NULL);
}

static void ui_playback_ready (void)
{
    title_change_cb ();
    set_slider_length (aud_drct_get_length ());
    time_counter_cb ();

    /* update time counter 4 times a second */
    if (! update_song_timeout_source)
        update_song_timeout_source = g_timeout_add (250, (GSourceFunc)
         time_counter_cb, NULL);

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
     NULL);

    gtk_tool_button_set_stock_id ((GtkToolButton *) button_play, GTK_STOCK_MEDIA_PLAY);
    gtk_widget_set_sensitive ((GtkWidget *) button_stop, FALSE);
    gtk_widget_hide (slider);
    gtk_widget_hide (label_time);
}

static GtkToolItem * toolbar_button_add (GtkWidget * toolbar,
 void (* callback) (void), const gchar * stock_id)
{
    GtkToolItem * item = gtk_tool_button_new_from_stock (stock_id);
    gtk_toolbar_insert ((GtkToolbar *) toolbar, item, -1);

    g_signal_connect (item, "clicked", callback, NULL);
    return item;
}

static GtkToolItem * toggle_button_new (const gchar * icon, const gchar * alt,
 void (* toggled) (GtkToggleToolButton * button))
{
    GtkToolItem * item = gtk_toggle_tool_button_new ();

    if (! alt)
        gtk_tool_button_set_stock_id ((GtkToolButton *) item, icon);
    else if (gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), icon))
        gtk_tool_button_set_icon_name ((GtkToolButton *) item, icon);
    else
        gtk_tool_button_set_label ((GtkToolButton *) item, alt);

    g_signal_connect (item, "toggled", (GCallback) toggled, NULL);
    return item;
}

static GtkWidget *markup_label_new(const gchar * str)
{
    GtkWidget *label = gtk_label_new(str);
    g_object_set(G_OBJECT(label), "use-markup", TRUE, NULL);
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
      case 0:;
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
                do_seek (aud_drct_get_time () - 5000);
            return TRUE;
        case GDK_KEY_Right:
            if (aud_drct_get_playing ())
                do_seek (aud_drct_get_time () + 5000);
            return TRUE;
        }

        return FALSE;

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
                do_seek (aud_drct_get_time () - 5000);
            break;
          case GDK_KEY_Right:
            if (aud_drct_get_playing ())
                do_seek (aud_drct_get_time () + 5000);
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
            ui_playlist_notebook_position (GINT_TO_POINTER (aud_playlist_get_active ()), NULL);
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
     aud_get_bool (NULL, "repeat"));
    gtk_toggle_tool_button_set_active ((GtkToggleToolButton *) button_shuffle,
     aud_get_bool (NULL, "shuffle"));
}

static void toggle_repeat (GtkToggleToolButton * button)
{
    aud_set_bool (NULL, "repeat", gtk_toggle_tool_button_get_active (button));
}

static void toggle_shuffle (GtkToggleToolButton * button)
{
    aud_set_bool (NULL, "shuffle", gtk_toggle_tool_button_get_active (button));
}

static void toggle_search_tool (GtkToggleToolButton * button)
{
    aud_plugin_enable (search_tool, gtk_toggle_tool_button_get_active (button));
}

static gboolean search_tool_toggled (PluginHandle * plugin, void * unused)
{
    gtk_toggle_tool_button_set_active ((GtkToggleToolButton *) search_button,
     aud_plugin_get_enabled (plugin));
    return TRUE;
}

static void config_save (void)
{
    save_window_size ();
    layout_save ();
    pw_col_save ();
}

static void ui_hooks_associate(void)
{
    hook_associate ("title change", (HookFunction) title_change_cb, NULL);
    hook_associate ("playback begin", (HookFunction) ui_playback_begin, NULL);
    hook_associate ("playback ready", (HookFunction) ui_playback_ready, NULL);
    hook_associate ("playback pause", (HookFunction) pause_cb, NULL);
    hook_associate ("playback unpause", (HookFunction) pause_cb, NULL);
    hook_associate ("playback stop", (HookFunction) ui_playback_stop, NULL);
    hook_associate("playlist update", ui_playlist_notebook_update, NULL);
    hook_associate ("playlist activate", ui_playlist_notebook_activate, NULL);
    hook_associate ("playlist set playing", ui_playlist_notebook_set_playing, NULL);
    hook_associate ("playlist position", ui_playlist_notebook_position, NULL);
    hook_associate ("set shuffle", update_toggles, NULL);
    hook_associate ("set repeat", update_toggles, NULL);
    hook_associate ("config save", (HookFunction) config_save, NULL);
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

static gboolean init (void)
{
    search_tool = aud_plugin_lookup_basename ("search-tool");

    aud_config_set_defaults ("gtkui", gtkui_defaults);

    audgui_set_default_icon();
    audgui_register_stock_icons();

    pw_col_init ();

    gint x = aud_get_int ("gtkui", "player_x");
    gint y = aud_get_int ("gtkui", "player_y");
    gint w = aud_get_int ("gtkui", "player_width");
    gint h = aud_get_int ("gtkui", "player_height");

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size ((GtkWindow *) window, w, h);
    gtk_window_set_keep_above ((GtkWindow *) window, aud_get_bool ("gtkui", "always_on_top"));
    gtk_window_set_has_resize_grip ((GtkWindow *) window, FALSE);

    if (aud_get_bool ("gtkui", "save_window_position") && (x != -1 || y != -1))
        gtk_window_move ((GtkWindow *) window, x, y);

    g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(window_delete), NULL);

    accel = gtk_accel_group_new ();
    gtk_window_add_accel_group ((GtkWindow *) window, accel);

    vbox_outer = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add ((GtkContainer *) window, vbox_outer);

    menu_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start ((GtkBox *) vbox_outer, menu_box, FALSE, FALSE, 0);

    toolbar = gtk_toolbar_new ();
    gtk_toolbar_set_style ((GtkToolbar *) toolbar, GTK_TOOLBAR_ICONS);
    GtkStyleContext * context = gtk_widget_get_style_context (toolbar);
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_PRIMARY_TOOLBAR);
    gtk_box_pack_start ((GtkBox *) vbox_outer, toolbar, FALSE, FALSE, 0);

    /* search button */
    if (search_tool)
    {
        search_button = toggle_button_new (GTK_STOCK_FIND, NULL, toggle_search_tool);
        gtk_toolbar_insert ((GtkToolbar *) toolbar, search_button, -1);
        gtk_toggle_tool_button_set_active ((GtkToggleToolButton *) search_button,
         aud_plugin_get_enabled (search_tool));
        aud_plugin_add_watch (search_tool, search_tool_toggled, NULL);
    }

    /* playback buttons */
    toolbar_button_add (toolbar, button_open_pressed, GTK_STOCK_OPEN);
    toolbar_button_add (toolbar, button_add_pressed, GTK_STOCK_ADD);
    button_play = toolbar_button_add (toolbar, button_play_pressed, GTK_STOCK_MEDIA_PLAY);
    button_stop = toolbar_button_add (toolbar, aud_drct_stop, GTK_STOCK_MEDIA_STOP);
    toolbar_button_add (toolbar, aud_drct_pl_prev, GTK_STOCK_MEDIA_PREVIOUS);
    toolbar_button_add (toolbar, aud_drct_pl_next, GTK_STOCK_MEDIA_NEXT);

    /* time slider and label */
    GtkToolItem * boxitem1 = gtk_tool_item_new ();
    gtk_tool_item_set_expand (boxitem1, TRUE);
    gtk_toolbar_insert ((GtkToolbar *) toolbar, boxitem1, -1);

    GtkWidget * box1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add ((GtkContainer *) boxitem1, box1);

    slider = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, NULL);
    gtk_scale_set_draw_value(GTK_SCALE(slider), FALSE);
    gtk_widget_set_size_request(slider, 120, -1);
    gtk_widget_set_valign (slider, GTK_ALIGN_CENTER);
    gtk_widget_set_can_focus(slider, FALSE);
    gtk_box_pack_start ((GtkBox *) box1, slider, TRUE, TRUE, 6);

    label_time = markup_label_new(NULL);
    gtk_box_pack_end ((GtkBox *) box1, label_time, FALSE, FALSE, 6);

    gtk_widget_set_no_show_all (slider, TRUE);
    gtk_widget_set_no_show_all (label_time, TRUE);

    /* repeat and shuffle buttons */
    button_repeat = toggle_button_new ("media-playlist-repeat", "RP", toggle_repeat);
    gtk_toolbar_insert ((GtkToolbar *) toolbar, button_repeat, -1);
    button_shuffle = toggle_button_new ("media-playlist-shuffle", "SF", toggle_shuffle);
    gtk_toolbar_insert ((GtkToolbar *) toolbar, button_shuffle, -1);

    /* volume button */
    GtkToolItem * boxitem2 = gtk_tool_item_new ();
    gtk_toolbar_insert ((GtkToolbar *) toolbar, boxitem2, -1);

    GtkWidget * box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add ((GtkContainer *) boxitem2, box2);

    volume = gtk_volume_button_new();
    gtk_button_set_relief(GTK_BUTTON(volume), GTK_RELIEF_NONE);
    gtk_scale_button_set_adjustment(GTK_SCALE_BUTTON(volume), GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 100, 1, 5, 0)));
    gtk_widget_set_can_focus(volume, FALSE);

    gint lvol = 0, rvol = 0;
    aud_drct_get_volume(&lvol, &rvol);
    gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume), (lvol + rvol) / 2);

    gtk_box_pack_start ((GtkBox *) box2, volume, FALSE, FALSE, 0);

    /* main UI layout */
    layout_load ();

    GtkWidget * layout = layout_new ();
    gtk_box_pack_start ((GtkBox *) vbox_outer, layout, TRUE, TRUE, 0);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    layout_add_center (vbox);

    ui_playlist_notebook_new ();
    gtk_box_pack_start ((GtkBox *) vbox, (GtkWidget *) UI_PLAYLIST_NOTEBOOK, TRUE, TRUE, 0);

    /* optional UI elements */
    show_menu (aud_get_bool ("gtkui", "menu_visible"));
    show_infoarea (aud_get_bool ("gtkui", "infoarea_visible"));

    if (aud_get_bool ("gtkui", "statusbar_visible"))
    {
        statusbar = ui_statusbar_new ();
        gtk_box_pack_end ((GtkBox *) vbox_outer, statusbar, FALSE, FALSE, 0);
    }

    AUDDBG("hooks associate\n");
    ui_hooks_associate();

    AUDDBG("playlist associate\n");
    ui_playlist_notebook_populate();

    g_signal_connect(slider, "change-value", G_CALLBACK(ui_slider_change_value_cb), NULL);
    g_signal_connect(slider, "button-press-event", G_CALLBACK(ui_slider_button_press_cb), NULL);
    g_signal_connect(slider, "button-release-event", G_CALLBACK(ui_slider_button_release_cb), NULL);

    volume_change_handler_id = g_signal_connect(volume, "value-changed", G_CALLBACK(ui_volume_value_changed_cb), NULL);
    g_signal_connect(volume, "pressed", G_CALLBACK(ui_volume_pressed_cb), NULL);
    g_signal_connect(volume, "released", G_CALLBACK(ui_volume_released_cb), NULL);
    update_volume_timeout_source = g_timeout_add(250, (GSourceFunc) ui_volume_slider_update, volume);

    g_signal_connect (window, "map-event", (GCallback) window_mapped_cb, NULL);
    g_signal_connect (window, "key-press-event", (GCallback) window_keypress_cb, NULL);
    g_signal_connect (UI_PLAYLIST_NOTEBOOK, "key-press-event", (GCallback) playlist_keypress_cb, NULL);

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

    if (aud_get_bool ("gtkui", "player_visible"))
        ui_show (TRUE);

    update_toggles (NULL, NULL);

    menu_rclick = make_menu_rclick (accel);
    menu_tab = make_menu_tab (accel);

    return TRUE;
}

static void cleanup (void)
{
    if (error_win)
        gtk_widget_destroy (error_win);

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
        aud_plugin_remove_watch (search_tool, search_tool_toggled, NULL);

    pw_col_cleanup ();
    gtk_widget_destroy (window);
    layout_cleanup ();
}

static void menu_position_cb (GtkMenu * menu, int * x, int * y, int * push, void * button)
{
    int xorig, yorig, xwin, ywin;

    gdk_window_get_origin (gtk_widget_get_window (window), & xorig, & yorig);
    gtk_widget_translate_coordinates (button, window, 0, 0, & xwin, & ywin);

    * x = xorig + xwin;
    * y = yorig + ywin + gtk_widget_get_allocated_height (button);
    * push = TRUE;
}

static void menu_button_cb (void)
{
    if (gtk_toggle_tool_button_get_active ((GtkToggleToolButton *) menu_button))
        gtk_menu_popup ((GtkMenu *) menu_main, NULL, NULL, menu_position_cb,
         menu_button, 0, gtk_get_current_event_time ());
    else
        gtk_widget_hide (menu_main);
}

static void menu_hide_cb (void)
{
    gtk_toggle_tool_button_set_active ((GtkToggleToolButton *) menu_button, FALSE);
}

void show_menu (gboolean show)
{
    aud_set_bool ("gtkui", "menu_visible", show);

    if (show)
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
            g_signal_connect (menu_main, "hide", (GCallback) menu_hide_cb, NULL);
        }

        if (! menu_button)
        {
            menu_button = gtk_toggle_tool_button_new_from_stock (AUD_STOCK_AUDACIOUS);
            g_signal_connect (menu_button, "destroy", (GCallback)
             gtk_widget_destroyed, & menu_button);
            gtk_widget_show ((GtkWidget *) menu_button);
            gtk_toolbar_insert ((GtkToolbar *) toolbar, menu_button, 0);
            g_signal_connect (menu_button, "toggled", (GCallback) menu_button_cb, NULL);
        }
    }
}

void show_infoarea (gboolean show)
{
    aud_set_bool ("gtkui", "infoarea_visible", show);

    if (show && ! infoarea)
    {
        infoarea = ui_infoarea_new ();
        g_signal_connect (infoarea, "destroy", (GCallback) gtk_widget_destroyed, & infoarea);
        ui_infoarea_show_vis (aud_get_bool ("gtkui", "infoarea_show_vis"));
        gtk_box_pack_end ((GtkBox *) vbox, infoarea, FALSE, FALSE, 0);
        gtk_widget_show_all (infoarea);
    }

    if (! show && infoarea)
    {
        gtk_widget_destroy (infoarea);
        infoarea = NULL;
    }
}

void show_infoarea_vis (gboolean show)
{
    aud_set_bool ("gtkui", "infoarea_show_vis", show);
    ui_infoarea_show_vis (show);
}

void show_statusbar (gboolean show)
{
    aud_set_bool ("gtkui", "statusbar_visible", show);

    if (show && ! statusbar)
    {
        statusbar = ui_statusbar_new ();
        gtk_box_pack_end ((GtkBox *) vbox_outer, statusbar, FALSE, FALSE, 0);
        gtk_widget_show_all (statusbar);
    }

    if (! show && statusbar)
    {
        gtk_widget_destroy (statusbar);
        statusbar = NULL;
    }
}

void popup_menu_rclick (guint button, guint32 time)
{
    gtk_menu_popup ((GtkMenu *) menu_rclick, NULL, NULL, NULL, NULL, button,
     time);
}

void popup_menu_tab (guint button, guint32 time, int playlist)
{
    menu_tab_playlist_id = aud_playlist_get_unique_id (playlist);
    gtk_menu_popup ((GtkMenu *) menu_tab, NULL, NULL, NULL, NULL, button, time);
}

void activate_search_tool (void)
{
    if (! search_tool)
        return;

    if (! aud_plugin_get_enabled (search_tool))
        aud_plugin_enable (search_tool, TRUE);

    aud_plugin_send_message (search_tool, "grab focus", NULL, 0);
}
