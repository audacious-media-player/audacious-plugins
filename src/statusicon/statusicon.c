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
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

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
        gchar * path = g_strdup_printf ("%s/images/audacious_player.xpm",
         aud_get_path (AUD_PATH_DATA_DIR));
        icon = gtk_status_icon_new_from_file (path);
        g_free (path);
    }

    return icon;
}

static gboolean si_cb_btpress(GtkStatusIcon * icon, GdkEventButton * event, gpointer user_data)
{
    si_popup_timer_stop(icon);
    si_popup_hide(icon);

    switch (event->button)
    {
      case 1:
      {
          if (event->state & GDK_SHIFT_MASK)
              aud_drct_pl_next();
          else
              aud_interface_show (! (aud_interface_is_shown () &&
               aud_interface_is_focused ()));
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

    return FALSE;
}

static gboolean si_cb_btscroll(GtkStatusIcon * icon, GdkEventScroll * event, gpointer user_data)
{
    switch (event->direction)
    {
      case GDK_SCROLL_UP:
      {
          switch (si_cfg.scroll_action)
          {
            case SI_CFG_SCROLL_ACTION_VOLUME:
                si_volume_change(si_cfg.volume_delta);
                break;
            case SI_CFG_SCROLL_ACTION_SKIP:
                si_playback_skip (aud_get_bool ("statusicon", "reverse_scroll") ? 1 : -1);
                break;
          }
          break;
      }

      case GDK_SCROLL_DOWN:
      {
          switch (si_cfg.scroll_action)
          {
            case SI_CFG_SCROLL_ACTION_VOLUME:
                si_volume_change(-si_cfg.volume_delta);
                break;
            case SI_CFG_SCROLL_ACTION_SKIP:
                si_playback_skip (aud_get_bool ("statusicon", "reverse_scroll") ? -1 : 1);
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

    GdkDisplay * display = gdk_display_get_default ();
    GdkDeviceManager * manager = gdk_display_get_device_manager (display);
    GdkDevice * device = gdk_device_manager_get_client_pointer (manager);
    gdk_device_get_position (device, NULL, & x, & y);

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
    if (si_cfg.disable_popup)
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

static GtkWidget *si_smallmenu_create(void)
{
    GtkWidget *si_smenu = gtk_menu_new();
    GtkWidget *si_smenu_prev_item, *si_smenu_play_item, *si_smenu_pause_item;
    GtkWidget *si_smenu_stop_item, *si_smenu_next_item, *si_smenu_sep_item, *si_smenu_eject_item;
    GtkWidget *si_smenu_quit_item;

    si_smenu_eject_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_OPEN, NULL);
    g_signal_connect_swapped(si_smenu_eject_item, "activate", G_CALLBACK(si_playback_ctrl), GINT_TO_POINTER(SI_PLAYBACK_CTRL_EJECT));
    gtk_menu_shell_append(GTK_MENU_SHELL(si_smenu), si_smenu_eject_item);
    gtk_widget_show(si_smenu_eject_item);
    si_smenu_sep_item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(si_smenu), si_smenu_sep_item);
    gtk_widget_show(si_smenu_sep_item);
    si_smenu_prev_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_MEDIA_PREVIOUS, NULL);
    g_signal_connect_swapped(si_smenu_prev_item, "activate", G_CALLBACK(si_playback_ctrl), GINT_TO_POINTER(SI_PLAYBACK_CTRL_PREV));
    gtk_menu_shell_append(GTK_MENU_SHELL(si_smenu), si_smenu_prev_item);
    gtk_widget_show(si_smenu_prev_item);
    si_smenu_play_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_MEDIA_PLAY, NULL);
    g_signal_connect_swapped(si_smenu_play_item, "activate", G_CALLBACK(si_playback_ctrl), GINT_TO_POINTER(SI_PLAYBACK_CTRL_PLAY));
    gtk_menu_shell_append(GTK_MENU_SHELL(si_smenu), si_smenu_play_item);
    gtk_widget_show(si_smenu_play_item);
    si_smenu_pause_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_MEDIA_PAUSE, NULL);
    g_signal_connect_swapped(si_smenu_pause_item, "activate", G_CALLBACK(si_playback_ctrl), GINT_TO_POINTER(SI_PLAYBACK_CTRL_PAUSE));
    gtk_menu_shell_append(GTK_MENU_SHELL(si_smenu), si_smenu_pause_item);
    gtk_widget_show(si_smenu_pause_item);
    si_smenu_stop_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_MEDIA_STOP, NULL);
    g_signal_connect_swapped(si_smenu_stop_item, "activate", G_CALLBACK(si_playback_ctrl), GINT_TO_POINTER(SI_PLAYBACK_CTRL_STOP));
    gtk_menu_shell_append(GTK_MENU_SHELL(si_smenu), si_smenu_stop_item);
    gtk_widget_show(si_smenu_stop_item);
    si_smenu_next_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_MEDIA_NEXT, NULL);
    g_signal_connect_swapped(si_smenu_next_item, "activate", G_CALLBACK(si_playback_ctrl), GINT_TO_POINTER(SI_PLAYBACK_CTRL_NEXT));
    gtk_menu_shell_append(GTK_MENU_SHELL(si_smenu), si_smenu_next_item);
    gtk_widget_show(si_smenu_next_item);

    if (si_cfg.rclick_menu == SI_CFG_RCLICK_MENU_SMALL2)
    {
        si_smenu_sep_item = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(si_smenu), si_smenu_sep_item);
        gtk_widget_show(si_smenu_sep_item);
        si_smenu_quit_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
        g_signal_connect_swapped(si_smenu_quit_item, "activate", G_CALLBACK(aud_drct_quit), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(si_smenu), si_smenu_quit_item);
        gtk_widget_show(si_smenu_quit_item);
    }

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

    if (si_cfg.close_to_tray)
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
    plugin_active = TRUE;
    si_cfg_load();
    si_enable(TRUE);
    return TRUE;
}

void si_cleanup(void)
{
    if (!plugin_active)
        return;

    plugin_active = FALSE;
    si_enable(FALSE);
    si_cfg_save();
}

void si_about(void)
{
    static GtkWidget *about_dlg = NULL;

    if (about_dlg != NULL)
    {
        gtk_window_present(GTK_WINDOW(about_dlg));
        return;
    }

    audgui_simple_message(&about_dlg, GTK_MESSAGE_INFO, _("About Status Icon Plugin"),
                        _("Status Icon Plugin\n\n"
                          "Copyright 2005-2007 Giacomo Lozito <james@develia.org>\n"
                          "Copyright 2010 Michał Lipski <tallica@o2.pl>\n\n"
                          "This plugin provides a status icon, placed in\n"
                          "the system tray area of the window manager.\n"));
}

static GtkWidget *prefs_disable_popup_chkbtn;
static GtkWidget *prefs_close_to_tray_chkbtn;
static GtkWidget * reverse_scroll_toggle;

void si_prefs_cb_commit(gpointer prefs_win)
{
    GSList *list = g_object_get_data(G_OBJECT(prefs_win), "rcm_grp");
    while (list != NULL)
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(list->data)) == TRUE)
        {
            si_cfg.rclick_menu = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(list->data), "val"));
            break;
        }
        list = g_slist_next(list);
    }

    list = g_object_get_data(G_OBJECT(prefs_win), "msa_grp");
    while (list != NULL)
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(list->data)) == TRUE)
        {
            si_cfg.scroll_action = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(list->data), "val"));
            break;
        }
        list = g_slist_next(list);
    }

    si_cfg.disable_popup = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefs_disable_popup_chkbtn));
    si_cfg.close_to_tray = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefs_close_to_tray_chkbtn));
    aud_set_bool ("statusicon", "reverse_scroll", gtk_toggle_button_get_active
     ((GtkToggleButton *) reverse_scroll_toggle));

    si_cfg_save();

    /* request the recreation of status icon small-menu if necessary */
    recreate_smallmenu = TRUE;

    gtk_widget_destroy(GTK_WIDGET(prefs_win));
}


void si_config(void)
{
    static GtkWidget *prefs_win = NULL;
    GtkWidget *prefs_vbox;
    GtkWidget *prefs_rclick_frame, *prefs_rclick_vbox;
    GtkWidget *prefs_rclick_smallmenu1_rbt, *prefs_rclick_smallmenu2_rbt;
    GtkWidget *prefs_scroll_frame, *prefs_scroll_vbox;
    GtkWidget *prefs_other_frame, *prefs_other_vbox;
    GtkWidget *prefs_scroll_vol_rbt, *prefs_scroll_skip_rbt;
    GtkWidget *prefs_bbar_bbox;
    GtkWidget *prefs_bbar_bt_ok, *prefs_bbar_bt_cancel;
    GdkGeometry prefs_win_hints;

    if (prefs_win != NULL)
    {
        gtk_window_present(GTK_WINDOW(prefs_win));
        return;
    }

    prefs_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_type_hint(GTK_WINDOW(prefs_win), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_position(GTK_WINDOW(prefs_win), GTK_WIN_POS_CENTER);
    gtk_window_set_title(GTK_WINDOW(prefs_win), _("Status Icon Plugin - Preferences"));
    gtk_container_set_border_width(GTK_CONTAINER(prefs_win), 10);
    prefs_win_hints.min_width = 320;
    prefs_win_hints.min_height = -1;
    gtk_window_set_geometry_hints(GTK_WINDOW(prefs_win), GTK_WIDGET(prefs_win), &prefs_win_hints, GDK_HINT_MIN_SIZE);
    g_signal_connect(G_OBJECT(prefs_win), "destroy", G_CALLBACK(gtk_widget_destroyed), &prefs_win);

    prefs_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(prefs_win), prefs_vbox);

    prefs_rclick_frame = gtk_frame_new(_("Right-Click Menu"));
    prefs_rclick_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(prefs_rclick_vbox), 6);
    gtk_container_add(GTK_CONTAINER(prefs_rclick_frame), prefs_rclick_vbox);
    prefs_rclick_smallmenu1_rbt = gtk_radio_button_new_with_label (NULL,
     _("Small playback menu #1"));
    g_object_set_data(G_OBJECT(prefs_rclick_smallmenu1_rbt), "val", GINT_TO_POINTER(SI_CFG_RCLICK_MENU_SMALL1));
    prefs_rclick_smallmenu2_rbt = gtk_radio_button_new_with_label_from_widget
     ((GtkRadioButton *) prefs_rclick_smallmenu1_rbt, _("Small playback menu #2"));
    g_object_set_data(G_OBJECT(prefs_rclick_smallmenu2_rbt), "val", GINT_TO_POINTER(SI_CFG_RCLICK_MENU_SMALL2));
    g_object_set_data(G_OBJECT(prefs_win), "rcm_grp", gtk_radio_button_get_group(GTK_RADIO_BUTTON(prefs_rclick_smallmenu1_rbt)));
    switch (si_cfg.rclick_menu)
    {
      case SI_CFG_RCLICK_MENU_SMALL1:
          gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_rclick_smallmenu1_rbt), TRUE);
          break;
      case SI_CFG_RCLICK_MENU_SMALL2:
          gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_rclick_smallmenu2_rbt), TRUE);
          break;
    }
    gtk_box_pack_start(GTK_BOX(prefs_rclick_vbox), prefs_rclick_smallmenu1_rbt, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(prefs_rclick_vbox), prefs_rclick_smallmenu2_rbt, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(prefs_vbox), prefs_rclick_frame, TRUE, TRUE, 0);

    prefs_scroll_frame = gtk_frame_new(_("Mouse Scroll Action"));
    prefs_scroll_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(prefs_scroll_vbox), 6);
    gtk_container_add(GTK_CONTAINER(prefs_scroll_frame), prefs_scroll_vbox);
    prefs_scroll_vol_rbt = gtk_radio_button_new_with_label(NULL, _("Change volume"));
    g_object_set_data(G_OBJECT(prefs_scroll_vol_rbt), "val", GINT_TO_POINTER(SI_CFG_SCROLL_ACTION_VOLUME));
    prefs_scroll_skip_rbt = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(prefs_scroll_vol_rbt), _("Change playing song"));
    g_object_set_data(G_OBJECT(prefs_scroll_skip_rbt), "val", GINT_TO_POINTER(SI_CFG_SCROLL_ACTION_SKIP));
    g_object_set_data(G_OBJECT(prefs_win), "msa_grp", gtk_radio_button_get_group(GTK_RADIO_BUTTON(prefs_scroll_skip_rbt)));

    if (si_cfg.scroll_action == SI_CFG_SCROLL_ACTION_VOLUME)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_scroll_vol_rbt), TRUE);
    else
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_scroll_skip_rbt), TRUE);

    gtk_box_pack_start(GTK_BOX(prefs_scroll_vbox), prefs_scroll_vol_rbt, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(prefs_scroll_vbox), prefs_scroll_skip_rbt, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(prefs_vbox), prefs_scroll_frame, TRUE, TRUE, 0);

    prefs_other_frame = gtk_frame_new(_("Other settings"));
    prefs_other_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(prefs_other_vbox), 6);
    gtk_container_add(GTK_CONTAINER(prefs_other_frame), prefs_other_vbox);

    prefs_disable_popup_chkbtn = gtk_check_button_new_with_label(_("Disable the popup window"));

    if (si_cfg.disable_popup)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_disable_popup_chkbtn), TRUE);

    gtk_box_pack_start(GTK_BOX(prefs_other_vbox), prefs_disable_popup_chkbtn, TRUE, TRUE, 0);

    prefs_close_to_tray_chkbtn = gtk_check_button_new_with_label(_("Close to the notification area (system tray)"));

    if (si_cfg.close_to_tray)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_close_to_tray_chkbtn), TRUE);

    gtk_box_pack_start(GTK_BOX(prefs_other_vbox), prefs_close_to_tray_chkbtn, TRUE, TRUE, 0);

    reverse_scroll_toggle = gtk_check_button_new_with_label
     (_("Advance in playlist when scrolling upward"));
    gtk_toggle_button_set_active ((GtkToggleButton *) reverse_scroll_toggle,
     aud_get_bool ("statusicon", "reverse_scroll"));
    gtk_box_pack_start ((GtkBox *) prefs_other_vbox, reverse_scroll_toggle, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(prefs_vbox), prefs_other_frame, TRUE, TRUE, 0);

    /* horizontal separator and buttons */
    gtk_box_pack_start(GTK_BOX(prefs_vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);
    prefs_bbar_bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(prefs_bbar_bbox), GTK_BUTTONBOX_END);
    prefs_bbar_bt_cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    g_signal_connect_swapped(G_OBJECT(prefs_bbar_bt_cancel), "clicked", G_CALLBACK(gtk_widget_destroy), prefs_win);
    gtk_container_add(GTK_CONTAINER(prefs_bbar_bbox), prefs_bbar_bt_cancel);
    prefs_bbar_bt_ok = gtk_button_new_from_stock(GTK_STOCK_OK);
    gtk_container_add(GTK_CONTAINER(prefs_bbar_bbox), prefs_bbar_bt_ok);
    g_signal_connect_swapped(G_OBJECT(prefs_bbar_bt_ok), "clicked", G_CALLBACK(si_prefs_cb_commit), prefs_win);
    gtk_box_pack_start(GTK_BOX(prefs_vbox), prefs_bbar_bbox, FALSE, FALSE, 0);

    gtk_widget_show_all(prefs_win);
}

AUD_GENERAL_PLUGIN
(
    .name = "Status Icon",
    .init = si_init,
    .cleanup = si_cleanup,
    .about = si_about,
    .configure = si_config
)
