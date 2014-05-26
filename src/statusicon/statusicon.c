/*
* Status Icon Plugin for Audacious
*
* Copyright 2005-2007 Giacomo Lozito <james@develia.org>
* Copyright 2010 Michał Lipski <tallica@o2.pl>
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*
*/

#include "statusicon.h"

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <audacious/plugins.h>
#include <audacious/preferences.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>
#include <libaudgui/menu.h>

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#define POPUP_IS_ACTIVE GPOINTER_TO_INT(g_object_get_data(G_OBJECT(icon), "popup_active"))
#define TIMER_IS_ACTIVE GPOINTER_TO_INT(g_object_get_data(G_OBJECT(icon), "timer_active"))

static void si_popup_timer_start(GtkStatusIcon *);
static void si_popup_timer_stop(GtkStatusIcon *);
static void si_smallmenu_show(gint x, gint y, guint button, guint32 time, gpointer);
static void si_smallmenu_recreate(GtkStatusIcon *);
static void si_popup_hide(gpointer icon);

static const char * const si_defaults[] = {
 "scroll_action", "0", /* SI_CFG_SCROLL_ACTION_VOLUME */
 "volume_delta", "5",
 "disable_popup", "FALSE",
 "close_to_tray", "FALSE",
 "reverse_scroll", "FALSE",
 NULL};

static gboolean plugin_active = FALSE;
static gboolean recreate_smallmenu = FALSE;

static GtkStatusIcon *si_create(void)
{
    GtkStatusIcon *icon;
    GtkIconTheme *theme;

    theme = gtk_icon_theme_get_default();

    if (gtk_icon_theme_has_icon(theme, "audacious-panel"))
        icon = gtk_status_icon_new_from_icon_name("audacious-panel");
    else if (gtk_icon_theme_has_icon(theme, "audacious"))
        icon = gtk_status_icon_new_from_icon_name("audacious");
    else
    {
        gchar * path = g_strdup_printf ("%s/images/audacious.png",
         aud_get_path (AUD_PATH_DATA_DIR));
        icon = gtk_status_icon_new_from_file (path);
        g_free (path);
    }

    return icon;
}

static gboolean si_cb_btpress(GtkStatusIcon * icon, GdkEventButton * event, gpointer user_data)
{
    if (event->type != GDK_BUTTON_PRESS)
        return FALSE;

    si_popup_timer_stop(icon);
    si_popup_hide(icon);

    switch (event->button)
    {
      case 1:
      {
          if (event->state & GDK_SHIFT_MASK)
              aud_drct_pl_next();
          else if (! aud_headless_mode ())
              aud_interface_show (! aud_interface_is_shown ());
          break;
      }

      case 2:
      {
          aud_drct_pause();
          break;
      }

      case 3:
          if (event->state & GDK_SHIFT_MASK)
              aud_drct_pl_prev();
          else
          {
              if (recreate_smallmenu == TRUE)
                  si_smallmenu_recreate(icon);
              si_smallmenu_show(event->x_root, event->y_root, 3, event->time, icon);
          }
          break;
    }

    return TRUE;
}

static gboolean si_cb_btscroll(GtkStatusIcon * icon, GdkEventScroll * event, gpointer user_data)
{
    switch (event->direction)
    {
      case GDK_SCROLL_UP:
      {
          switch (aud_get_int ("statusicon", "scroll_action"))
          {
            case SI_CFG_SCROLL_ACTION_VOLUME:
                si_volume_change (aud_get_int ("statusicon", "volume_delta"));
                break;
            case SI_CFG_SCROLL_ACTION_SKIP:
                if (aud_get_bool ("statusicon", "reverse_scroll"))
                    aud_drct_pl_next ();
                else
                    aud_drct_pl_prev ();
                break;
          }
          break;
      }

      case GDK_SCROLL_DOWN:
      {
          switch (aud_get_int ("statusicon", "scroll_action"))
          {
            case SI_CFG_SCROLL_ACTION_VOLUME:
                si_volume_change (-aud_get_int ("statusicon", "volume_delta"));
                break;
            case SI_CFG_SCROLL_ACTION_SKIP:
                if (aud_get_bool ("statusicon", "reverse_scroll"))
                    aud_drct_pl_prev ();
                else
                    aud_drct_pl_next ();
                break;
          }
          break;
      }

      default:;
    }

    return FALSE;
}

static gboolean si_popup_show(gpointer icon)
{
    GdkRectangle area;
    gint x, y;
    static gint count = 0;

    audgui_get_mouse_coords (NULL, & x, & y);
    gtk_status_icon_get_geometry (icon, NULL, & area, NULL);

    if (x < area.x || x > area.x + area.width || y < area.y || y > area.y + area.width)
    {
        si_popup_timer_stop(icon);
        si_popup_hide(icon);
        count = 0;

        return TRUE;
    }

    if (!POPUP_IS_ACTIVE)
    {
        if (count < 10)
        {
            count++;
            return TRUE;
        }
        else
            count = 0;

        audgui_infopopup_show_current();
        g_object_set_data(G_OBJECT(icon), "popup_active", GINT_TO_POINTER(1));
    }

    return TRUE;
}

static void si_popup_hide(gpointer icon)
{
    if (POPUP_IS_ACTIVE)
    {
        g_object_set_data(G_OBJECT(icon), "popup_active", GINT_TO_POINTER(0));
        audgui_infopopup_hide();
    }
}

static void si_popup_reshow(gpointer data, gpointer icon)
{
    if (POPUP_IS_ACTIVE)
    {
        audgui_infopopup_hide();
        audgui_infopopup_show_current();
    }
}

static void si_popup_timer_start(GtkStatusIcon * icon)
{
    gint timer_id = g_timeout_add(100, si_popup_show, icon);
    g_object_set_data(G_OBJECT(icon), "timer_id", GINT_TO_POINTER(timer_id));
    g_object_set_data(G_OBJECT(icon), "timer_active", GINT_TO_POINTER(1));
}

static void si_popup_timer_stop(GtkStatusIcon * icon)
{
    if (TIMER_IS_ACTIVE)
        g_source_remove(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(icon), "timer_id")));

    g_object_set_data(G_OBJECT(icon), "timer_id", GINT_TO_POINTER(0));
    g_object_set_data(G_OBJECT(icon), "timer_active", GINT_TO_POINTER(0));
}

static gboolean si_cb_tooltip(GtkStatusIcon * icon, gint x, gint y, gboolean keyboard_mode, GtkTooltip * tooltip, gpointer user_data)
{
    GtkWidget *menu = g_object_get_data(G_OBJECT(icon), "smenu");

    if (aud_get_bool("statusicon", "disable_popup") || gtk_widget_get_visible(menu))
        return FALSE;

    if (!POPUP_IS_ACTIVE && !TIMER_IS_ACTIVE)
        si_popup_timer_start(icon);

    return FALSE;
}

static void si_smallmenu_show(gint x, gint y, guint button, guint32 time, gpointer evbox)
{
    GtkWidget *si_smenu = g_object_get_data(G_OBJECT(evbox), "smenu");
    gtk_menu_popup(GTK_MENU(si_smenu), NULL, NULL, NULL, NULL, button, time);
}

static void open_files (void)
{
    audgui_run_filebrowser (TRUE);
}

static GtkWidget *si_smallmenu_create(void)
{
    static const AudguiMenuItem items[] = {
        {N_("_Open Files ..."), "document-open", .func = open_files},
        {N_("Pre_vious"), "media-skip-backward", .func = aud_drct_pl_prev},
        {N_("_Play"), "media-playback-start", .func = aud_drct_play},
        {N_("Paus_e"), "media-playback-pause", .func = aud_drct_pause},
        {N_("_Stop"), "media-playback-stop", .func = aud_drct_stop},
        {N_("_Next"), "media-skip-forward", .func = aud_drct_pl_next},
        {.sep = TRUE},
        {N_("Se_ttings ..."), "preferences-system", .func = aud_show_prefs_window},
        {N_("_Quit"), "application-exit", .func = aud_drct_quit}
    };

    GtkWidget *si_smenu = gtk_menu_new();
    audgui_menu_init (si_smenu, items, ARRAY_LEN (items), NULL);
    return si_smenu;
}

static void si_smallmenu_recreate(GtkStatusIcon * icon)
{
    GtkWidget *smenu = g_object_get_data(G_OBJECT(icon), "smenu");
    gtk_widget_destroy(GTK_WIDGET(smenu));
    smenu = si_smallmenu_create();
    g_object_set_data(G_OBJECT(icon), "smenu", smenu);
    recreate_smallmenu = FALSE;
}

static void si_window_close(gpointer data, gpointer user_data)
{
    gboolean *handle = (gboolean*) data;

    if (aud_get_bool ("statusicon", "close_to_tray"))
    {
        *handle = TRUE;
        aud_interface_show (FALSE);
    }
}

static void si_enable(gboolean enable)
{
    static GtkStatusIcon *si_applet = NULL;

    if (enable && ! si_applet)
    {
        GtkWidget *si_smenu;

        si_applet = si_create();

        if (si_applet == NULL)
        {
            g_warning("StatusIcon plugin: unable to create a status icon.\n");
            return;
        }

        g_object_set_data(G_OBJECT(si_applet), "timer_id", GINT_TO_POINTER(0));
        g_object_set_data(G_OBJECT(si_applet), "timer_active", GINT_TO_POINTER(0));
        g_object_set_data(G_OBJECT(si_applet), "popup_active", GINT_TO_POINTER(0));

        g_signal_connect(G_OBJECT(si_applet), "button-press-event", G_CALLBACK(si_cb_btpress), NULL);
        g_signal_connect(G_OBJECT(si_applet), "scroll-event", G_CALLBACK(si_cb_btscroll), NULL);
        g_signal_connect(G_OBJECT(si_applet), "query-tooltip", G_CALLBACK(si_cb_tooltip), NULL);

        gtk_status_icon_set_has_tooltip(si_applet, TRUE);
        gtk_status_icon_set_visible(si_applet, TRUE);

        /* small menu that can be used in place of the audacious standard one */
        si_smenu = si_smallmenu_create();
        g_object_set_data(G_OBJECT(si_applet), "smenu", si_smenu);

        hook_associate("title change", si_popup_reshow, si_applet);
        hook_associate("window close", si_window_close, NULL);
    }

    if (! enable && si_applet)
    {
        /* Prevent accidentally hiding of the interface
         * by disabling the plugin while Audacious is closed to the tray. */
        extern GeneralPlugin _aud_plugin_self;
        PluginHandle *si = aud_plugin_by_header(&_aud_plugin_self);
        if (! aud_plugin_get_enabled(si) && ! aud_headless_mode() && ! aud_interface_is_shown())
            aud_interface_show(TRUE);

        GtkWidget *si_smenu = g_object_get_data(G_OBJECT(si_applet), "smenu");
        si_popup_timer_stop(si_applet);   /* just in case the timer is active */
        gtk_widget_destroy(si_smenu);
        g_object_unref(si_applet);
        si_applet = NULL;

        hook_dissociate("title change", si_popup_reshow);
        hook_dissociate("window close", si_window_close);
    }
}

static gboolean si_init (void)
{
    // libaudgui is not initialized in headless mode
    if (aud_headless_mode ())
        return TRUE;

    aud_config_set_defaults ("statusicon", si_defaults);
    plugin_active = TRUE;
    si_enable(TRUE);
    return TRUE;
}

void si_cleanup(void)
{
    if (!plugin_active)
        return;

    plugin_active = FALSE;
    si_enable(FALSE);
}

static const char si_about[] =
 N_("Status Icon Plugin\n\n"
    "Copyright 2005-2007 Giacomo Lozito <james@develia.org>\n"
    "Copyright 2010 Michał Lipski <tallica@o2.pl>\n\n"
    "This plugin provides a status icon, placed in\n"
    "the system tray area of the window manager.");

static const PreferencesWidget si_widgets[] = {
 {WIDGET_LABEL, N_("<b>Mouse Scroll Action</b>")},
 {WIDGET_RADIO_BTN, N_("Change volume"),
  .cfg_type = VALUE_INT, .csect = "statusicon", .cname = "scroll_action",
  .data = {.radio_btn = {SI_CFG_SCROLL_ACTION_VOLUME}}},
 {WIDGET_RADIO_BTN, N_("Change playing song"),
  .cfg_type = VALUE_INT, .csect = "statusicon", .cname = "scroll_action",
  .data = {.radio_btn = {SI_CFG_SCROLL_ACTION_SKIP}}},
 {WIDGET_LABEL, N_("<b>Other Settings</b>")},
 {WIDGET_CHK_BTN, N_("Disable the popup window"),
  .cfg_type = VALUE_BOOLEAN, .csect = "statusicon", .cname = "disable_popup"},
 {WIDGET_CHK_BTN, N_("Close to the system tray"),
  .cfg_type = VALUE_BOOLEAN, .csect = "statusicon", .cname = "close_to_tray"},
 {WIDGET_CHK_BTN, N_("Advance in playlist when scrolling upward"),
  .cfg_type = VALUE_BOOLEAN, .csect = "statusicon", .cname = "reverse_scroll"}};

static const PluginPreferences si_prefs = {
 .widgets = si_widgets,
 .n_widgets = ARRAY_LEN (si_widgets)};

AUD_GENERAL_PLUGIN
(
    .name = N_("Status Icon"),
    .domain = PACKAGE,
    .about_text = si_about,
    .prefs = & si_prefs,
    .init = si_init,
    .cleanup = si_cleanup,
)
