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

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>
#include <libaudcore/preferences.h>
#include <libaudcore/hook.h>
#include <libaudcore/runtime.h>
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
static void si_smallmenu_show(int x, int y, unsigned button, uint32_t time, void *);
static void si_smallmenu_recreate(GtkStatusIcon *);
static void si_popup_hide(void * icon);

static PluginHandle * get_plugin_self ();

static const char * const si_defaults[] = {
 "scroll_action", "0", /* SI_CFG_SCROLL_ACTION_VOLUME */
 "volume_delta", "5",
 "disable_popup", "FALSE",
 "close_to_tray", "FALSE",
 "reverse_scroll", "FALSE",
 nullptr};

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
        char * path = g_strdup_printf ("%s/images/audacious.png",
         aud_get_path (AUD_PATH_DATA_DIR));
        icon = gtk_status_icon_new_from_file (path);
        g_free (path);
    }

    return icon;
}

static gboolean si_cb_btpress(GtkStatusIcon * icon, GdkEventButton * event, void * user_data)
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
          else if (! aud_get_headless_mode ())
              aud_ui_show (! aud_ui_is_shown ());
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

static gboolean si_cb_btscroll(GtkStatusIcon * icon, GdkEventScroll * event, void * user_data)
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

static gboolean si_popup_show(void * icon)
{
    GdkRectangle area;
    int x, y;
    static int count = 0;

    audgui_get_mouse_coords (nullptr, & x, & y);
    gtk_status_icon_get_geometry ((GtkStatusIcon *) icon, nullptr, & area, nullptr);

    if (x < area.x || x > area.x + area.width || y < area.y || y > area.y + area.width)
    {
        si_popup_timer_stop ((GtkStatusIcon *) icon);
        si_popup_hide ((GtkStatusIcon *) icon);
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

static void si_popup_hide(void * icon)
{
    if (POPUP_IS_ACTIVE)
    {
        g_object_set_data(G_OBJECT(icon), "popup_active", GINT_TO_POINTER(0));
        audgui_infopopup_hide();
    }
}

static void si_popup_reshow(void * data, void * icon)
{
    if (POPUP_IS_ACTIVE)
    {
        audgui_infopopup_hide();
        audgui_infopopup_show_current();
    }
}

static void si_popup_timer_start(GtkStatusIcon * icon)
{
    int timer_id = g_timeout_add(100, si_popup_show, icon);
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

static gboolean si_cb_tooltip(GtkStatusIcon * icon, int x, int y, gboolean keyboard_mode, GtkTooltip * tooltip, void * user_data)
{
    GtkWidget *menu = (GtkWidget *) g_object_get_data(G_OBJECT(icon), "smenu");

    if (aud_get_bool("statusicon", "disable_popup") || gtk_widget_get_visible(menu))
        return FALSE;

    if (!POPUP_IS_ACTIVE && !TIMER_IS_ACTIVE)
        si_popup_timer_start(icon);

    return FALSE;
}

static void si_smallmenu_show(int x, int y, unsigned button, uint32_t time, void * evbox)
{
    GtkWidget *si_smenu = (GtkWidget *) g_object_get_data(G_OBJECT(evbox), "smenu");
    gtk_menu_popup(GTK_MENU(si_smenu), nullptr, nullptr, nullptr, nullptr, button, time);
}

static void open_files (void)
{
    audgui_run_filebrowser (TRUE);
}

static GtkWidget *si_smallmenu_create(void)
{
    static const AudguiMenuItem items[] = {
        MenuCommand (N_("_Open Files ..."), "document-open", 0, (GdkModifierType) 0, open_files),
        MenuCommand (N_("Pre_vious"), "media-skip-backward", 0, (GdkModifierType) 0, aud_drct_pl_prev),
        MenuCommand (N_("_Play"), "media-playback-start", 0, (GdkModifierType) 0, aud_drct_play),
        MenuCommand (N_("Paus_e"), "media-playback-pause", 0, (GdkModifierType) 0, aud_drct_pause),
        MenuCommand (N_("_Stop"), "media-playback-stop", 0, (GdkModifierType) 0, aud_drct_stop),
        MenuCommand (N_("_Next"), "media-skip-forward", 0, (GdkModifierType) 0, aud_drct_pl_next),
        MenuSep (),
        MenuCommand (N_("Se_ttings ..."), "preferences-system", 0, (GdkModifierType) 0, audgui_show_prefs_window),
        MenuCommand (N_("_Quit"), "application-exit", 0, (GdkModifierType) 0, aud_quit)
    };

    GtkWidget *si_smenu = gtk_menu_new();
    audgui_menu_init (si_smenu, {items}, nullptr);
    return si_smenu;
}

static void si_smallmenu_recreate(GtkStatusIcon * icon)
{
    GtkWidget *smenu = (GtkWidget *) g_object_get_data(G_OBJECT(icon), "smenu");
    gtk_widget_destroy(GTK_WIDGET(smenu));
    smenu = si_smallmenu_create();
    g_object_set_data(G_OBJECT(icon), "smenu", smenu);
    recreate_smallmenu = FALSE;
}

static void si_window_close(void * data, void * user_data)
{
    gboolean *handle = (gboolean*) data;

    if (aud_get_bool ("statusicon", "close_to_tray"))
    {
        *handle = TRUE;
        aud_ui_show (FALSE);
    }
}

static void si_enable(gboolean enable)
{
    static GtkStatusIcon *si_applet = nullptr;

    if (enable && ! si_applet)
    {
        GtkWidget *si_smenu;

        si_applet = si_create();

        if (si_applet == nullptr)
        {
            g_warning("StatusIcon plugin: unable to create a status icon.\n");
            return;
        }

        g_object_set_data(G_OBJECT(si_applet), "timer_id", GINT_TO_POINTER(0));
        g_object_set_data(G_OBJECT(si_applet), "timer_active", GINT_TO_POINTER(0));
        g_object_set_data(G_OBJECT(si_applet), "popup_active", GINT_TO_POINTER(0));

        g_signal_connect(G_OBJECT(si_applet), "button-press-event", G_CALLBACK(si_cb_btpress), nullptr);
        g_signal_connect(G_OBJECT(si_applet), "scroll-event", G_CALLBACK(si_cb_btscroll), nullptr);
        g_signal_connect(G_OBJECT(si_applet), "query-tooltip", G_CALLBACK(si_cb_tooltip), nullptr);

        gtk_status_icon_set_has_tooltip(si_applet, TRUE);
        gtk_status_icon_set_visible(si_applet, TRUE);

        /* small menu that can be used in place of the audacious standard one */
        si_smenu = si_smallmenu_create();
        g_object_set_data(G_OBJECT(si_applet), "smenu", si_smenu);

        hook_associate("title change", si_popup_reshow, si_applet);
        hook_associate("window close", si_window_close, nullptr);
    }

    if (! enable && si_applet)
    {
        /* Prevent accidentally hiding of the interface
         * by disabling the plugin while Audacious is closed to the tray. */
        PluginHandle * si = get_plugin_self ();
        if (! aud_plugin_get_enabled(si) && ! aud_get_headless_mode() && ! aud_ui_is_shown())
            aud_ui_show(TRUE);

        GtkWidget *si_smenu = (GtkWidget *) g_object_get_data(G_OBJECT(si_applet), "smenu");
        si_popup_timer_stop(si_applet);   /* just in case the timer is active */
        gtk_widget_destroy(si_smenu);
        g_object_unref(si_applet);
        si_applet = nullptr;

        hook_dissociate("title change", si_popup_reshow);
        hook_dissociate("window close", si_window_close);
    }
}

static bool si_init (void)
{
    aud_config_set_defaults ("statusicon", si_defaults);
    audgui_init ();
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
    audgui_cleanup ();
}

static const char si_about[] =
 N_("Status Icon Plugin\n\n"
    "Copyright 2005-2007 Giacomo Lozito <james@develia.org>\n"
    "Copyright 2010 Michał Lipski <tallica@o2.pl>\n\n"
    "This plugin provides a status icon, placed in\n"
    "the system tray area of the window manager.");

static const PreferencesWidget si_widgets[] = {
    WidgetLabel (N_("<b>Mouse Scroll Action</b>")),
    WidgetRadio (N_("Change volume"),
        WidgetInt ("statusicon", "scroll_action"),
        {SI_CFG_SCROLL_ACTION_VOLUME}),
    WidgetRadio (N_("Change playing song"),
        WidgetInt ("statusicon", "scroll_action"),
        {SI_CFG_SCROLL_ACTION_SKIP}),
    WidgetLabel (N_("<b>Other Settings</b>")),
    WidgetCheck (N_("Disable the popup window"),
        WidgetBool ("statusicon", "disable_popup")),
    WidgetCheck (N_("Close to the system tray"),
        WidgetBool ("statusicon", "close_to_tray")),
    WidgetCheck (N_("Advance in playlist when scrolling upward"),
        WidgetBool ("statusicon", "reverse_scroll"))
};

static const PluginPreferences si_prefs = {{si_widgets}};

#define AUD_PLUGIN_NAME        N_("Status Icon")
#define AUD_PLUGIN_ABOUT       si_about
#define AUD_PLUGIN_PREFS       & si_prefs
#define AUD_PLUGIN_INIT        si_init
#define AUD_PLUGIN_CLEANUP     si_cleanup

#define AUD_DECLARE_GENERAL
#include <libaudcore/plugin-declare.h>

static PluginHandle * get_plugin_self ()
{
    return aud_plugin_by_header (& _aud_plugin_self);
}
