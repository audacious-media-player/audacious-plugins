/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2007
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

#include "si_ui.h"
#include "si_audacious.h"
#include "si_cfg.h"
#include "si_common.h"
#include <audacious/ui_fileinfopopup.h>
#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

static void si_ui_statusicon_popup_timer_start ( GtkStatusIcon * );
static void si_ui_statusicon_popup_timer_stop ( GtkStatusIcon * );
static void si_ui_statusicon_smallmenu_show ( gint x, gint y, guint button, guint32 time , gpointer );
static void si_ui_statusicon_smallmenu_recreate ( GtkStatusIcon * );
static void si_ui_statusicon_popup_hide ( gpointer icon );

extern si_cfg_t si_cfg;
static gboolean recreate_smallmenu = FALSE;
static gint last_x = -1;
static gint last_y = -1;
static gint popup_step = 0;


/* this stuff required to make titlechange hook work properly */
typedef struct
{
  gchar *title;
  gchar *filename;
  gpointer applet;
}
si_aud_hook_tchange_prevs_t;


static GtkStatusIcon *
si_ui_statusicon_create ( void )
{
    GtkStatusIcon *icon;
    GtkIconTheme *theme;

    theme = gtk_icon_theme_get_default();
    if (gtk_icon_theme_has_icon(theme, "audacious"))
    {
        icon = gtk_status_icon_new_from_icon_name("audacious");
    } else {
        icon = gtk_status_icon_new_from_file(DATA_DIR "/images/audacious_player.xpm");
    }

    return icon;
}


static gboolean
si_ui_statusicon_cb_btpress ( GtkStatusIcon * icon , GdkEventButton * event , gpointer user_data )
{
  switch ( event->button )
  {
    case 1:
    {
      if (event->state & GDK_SHIFT_MASK)
        audacious_drct_pl_next();
      else
        si_audacious_toggle_visibility();
      break;
    }

    case 2:
    {
      si_audacious_toggle_playback();
      break;
    }

    case 3:
    {
      if (event->state & GDK_SHIFT_MASK)
        audacious_drct_pl_prev();
      else{
      switch ( si_cfg.rclick_menu )
      {
        case SI_CFG_RCLICK_MENU_SMALL1:
        case SI_CFG_RCLICK_MENU_SMALL2:
          if ( recreate_smallmenu == TRUE )
            si_ui_statusicon_smallmenu_recreate( icon );
          si_ui_statusicon_smallmenu_show( event->x_root , event->y_root , 3 , event->time , icon );
          break;
        case SI_CFG_RCLICK_MENU_AUD:
        default:
          aud_hook_call ("show main menu", event);
          break;
      }
      break;
      }
    }
  }

  return FALSE;
}


static gboolean
si_ui_statusicon_cb_btscroll ( GtkStatusIcon * icon , GdkEventScroll * event , gpointer user_data )
{
  switch ( event->direction )
  {
    case GDK_SCROLL_UP:
    {
      switch ( si_cfg.scroll_action )
      {
        case SI_CFG_SCROLL_ACTION_VOLUME:
          si_audacious_volume_change( si_cfg.volume_delta );
          break;
        case SI_CFG_SCROLL_ACTION_SKIP:
          si_audacious_playback_skip( -1 );
          break;
      }
      break;
    }

    case GDK_SCROLL_DOWN:
    {
      switch ( si_cfg.scroll_action )
      {
        case SI_CFG_SCROLL_ACTION_VOLUME:
          si_audacious_volume_change( -si_cfg.volume_delta );
          break;
        case SI_CFG_SCROLL_ACTION_SKIP:
          si_audacious_playback_skip( 1 );
          break;
      }
      break;
    }

    default: ;
  }

  return FALSE;
}


static gboolean
si_ui_statusicon_popup_show ( gpointer icon )
{
    if ( GPOINTER_TO_INT(g_object_get_data( G_OBJECT(icon) , "timer_active" )) == 1 ) {
        switch (popup_step) {
        case 0:
            {
                Tuple *tuple;
                gint pl_active = aud_playlist_get_active();
                gint pos = aud_playlist_get_position(pl_active);
                GtkWidget *popup = g_object_get_data( G_OBJECT(icon) , "popup" );

                tuple = (Tuple*) aud_playlist_entry_get_tuple( pl_active , pos );
                if ( ( tuple == NULL ) || ( aud_tuple_get_int(tuple, FIELD_LENGTH, NULL) < 1 ) )  {
                    gchar *title = (gchar*) aud_playlist_entry_get_title( pl_active , pos );
                    audacious_fileinfopopup_show_from_title( popup , title );
                }
                else {
                    audacious_fileinfopopup_show_from_tuple( popup , tuple );
                }
                g_object_set_data( G_OBJECT(icon) , "popup_active" , GINT_TO_POINTER(1) );
                break;
            }
        case 2:
            {
                last_x = -1;
                last_y = -1;
                GdkDisplay *display = gdk_display_get_default();
                gtk_tooltip_trigger_tooltip_query(display);
                break;
            }
        default:
            {
                if (popup_step >= 3) {
                    popup_step = 1;
                    if (last_x<0 || last_y<0) {
                        si_ui_statusicon_popup_hide(icon);
                        si_ui_statusicon_popup_timer_stop(icon);
                        return FALSE;
                    }
                }
                break;
            }
        }
    }
    popup_step += 1;
    return TRUE;
}


static void
si_ui_statusicon_popup_hide ( gpointer icon )
{
  if ( GPOINTER_TO_INT(g_object_get_data( G_OBJECT(icon) , "popup_active" )) == 1 )
  {
    GtkWidget *popup = g_object_get_data( G_OBJECT(icon) , "popup" );
    g_object_set_data( G_OBJECT(icon) , "popup_active" , GINT_TO_POINTER(0) );
    audacious_fileinfopopup_hide( popup , NULL );
  }
}


static void
si_ui_statusicon_popup_timer_start ( GtkStatusIcon * icon )
{
  gint timer_id = g_timeout_add( 500 , si_ui_statusicon_popup_show , icon );
  popup_step = 0;
  g_object_set_data( G_OBJECT(icon) , "timer_id" , GINT_TO_POINTER(timer_id) );
  g_object_set_data( G_OBJECT(icon) , "timer_active" , GINT_TO_POINTER(1) );
  return;
}


static void
si_ui_statusicon_popup_timer_stop ( GtkStatusIcon * icon )
{
  if ( GPOINTER_TO_INT(g_object_get_data(G_OBJECT(icon),"timer_active")) == 1 )
    g_source_remove( GPOINTER_TO_INT(g_object_get_data(G_OBJECT(icon),"timer_id")) );

  g_object_set_data( G_OBJECT(icon) , "timer_id" , GINT_TO_POINTER(0) );
  g_object_set_data( G_OBJECT(icon) , "timer_active" , GINT_TO_POINTER(0) );
  return;
}


static gboolean
si_ui_statusicon_cb_tooltip(GtkStatusIcon *icon , gint x, gint y, gboolean keyboard_mode, GtkTooltip *tooltip, gpointer user_data) {
    if ( GPOINTER_TO_INT(g_object_get_data( G_OBJECT(icon), "popup_active" )) == 0 ) {
        if ( GPOINTER_TO_INT(g_object_get_data(G_OBJECT(icon),"timer_active")) == 0 ) {
            si_ui_statusicon_popup_timer_start( icon );
        }
    }
    last_x = x;
    last_y = y;
    return FALSE;
}


static void
si_ui_statusicon_cb_aud_hook_pbstart ( gpointer plentry_gp , gpointer icon )
{
  if ( ( GPOINTER_TO_INT(g_object_get_data( G_OBJECT(icon) , "popup_active" )) == 1 ) &&
       ( plentry_gp != NULL ) )
  {
    si_ui_statusicon_popup_hide( icon );
    si_ui_statusicon_popup_timer_start( icon );
  }
}


static void
si_ui_statusicon_cb_aud_hook_tchange ( gpointer plentry_gp , gpointer prevs_gp )
{
  si_aud_hook_tchange_prevs_t *prevs = prevs_gp;
  gint playlist = aud_playlist_get_active();
  gint pl_entry = aud_playlist_get_position(playlist);
  gboolean upd_pop = FALSE;

  if (pl_entry >= 0)
  {
    gchar *pl_entry_filename = (gchar*) aud_playlist_entry_get_filename(playlist, pl_entry);
    gchar *pl_entry_title = (gchar*) aud_playlist_entry_get_title(playlist, pl_entry);

    if ( ( prevs->title != NULL ) && ( prevs->filename != NULL ) )
    {
      if ( ( pl_entry_filename != NULL ) &&
           ( !strcmp(pl_entry_filename, prevs->filename) ) )
      {
        if ( ( pl_entry_title != NULL ) &&
             ( strcmp(pl_entry_title, prevs->title) ) )
        {
          g_free( prevs->title );
          prevs->title = g_strdup(pl_entry_title);
          upd_pop = TRUE;
        }
      }
      else
      {
        g_free(prevs->filename);
        prevs->filename = g_strdup(pl_entry_filename);
        /* if filename changes, reset title as well */
        g_free(prevs->title);
        prevs->title = g_strdup(pl_entry_title);
        upd_pop = TRUE;
      }
    }
    else
    {
      if ( prevs->title != NULL )
        g_free(prevs->title);
      prevs->title = g_strdup(pl_entry_title);
      if ( prevs->filename != NULL )
        g_free(prevs->filename);
      prevs->filename = g_strdup(pl_entry_filename);
    }
  }

  if ( ( upd_pop == TRUE ) &&
       ( GPOINTER_TO_INT(g_object_get_data( G_OBJECT(prevs->applet) , "popup_active" )) == 1 ) )
  {
    si_ui_statusicon_popup_hide( prevs->applet );
    si_ui_statusicon_popup_timer_start( prevs->applet );
  }
}

static void
si_ui_statusicon_smallmenu_show ( gint x, gint y, guint button, guint32 time , gpointer evbox )
{
  GtkWidget *si_smenu = g_object_get_data( G_OBJECT(evbox) , "smenu" );
  gtk_menu_popup( GTK_MENU(si_smenu) , NULL , NULL , NULL , NULL , button , time );
}


static GtkWidget *
si_ui_statusicon_smallmenu_create ( void )
{
  GtkWidget *si_smenu = gtk_menu_new();
  GtkWidget *si_smenu_prev_item, *si_smenu_play_item, *si_smenu_pause_item;
  GtkWidget *si_smenu_stop_item, *si_smenu_next_item, *si_smenu_sep_item, *si_smenu_eject_item;
  GtkWidget *si_smenu_quit_item;

  si_smenu_eject_item = gtk_image_menu_item_new_from_stock(
                          GTK_STOCK_OPEN , NULL );
  g_signal_connect_swapped( si_smenu_eject_item , "activate" ,
                            G_CALLBACK(si_audacious_playback_ctrl) ,
                            GINT_TO_POINTER(SI_AUDACIOUS_PLAYBACK_CTRL_EJECT) );
  gtk_menu_shell_append( GTK_MENU_SHELL(si_smenu) , si_smenu_eject_item );
  gtk_widget_show(si_smenu_eject_item);
  si_smenu_sep_item = gtk_separator_menu_item_new();
  gtk_menu_shell_append( GTK_MENU_SHELL(si_smenu) , si_smenu_sep_item );
  gtk_widget_show(si_smenu_sep_item);
  si_smenu_prev_item = gtk_image_menu_item_new_from_stock(
                         GTK_STOCK_MEDIA_PREVIOUS , NULL );
  g_signal_connect_swapped( si_smenu_prev_item , "activate" ,
                            G_CALLBACK(si_audacious_playback_ctrl) ,
                            GINT_TO_POINTER(SI_AUDACIOUS_PLAYBACK_CTRL_PREV) );
  gtk_menu_shell_append( GTK_MENU_SHELL(si_smenu) , si_smenu_prev_item );
  gtk_widget_show(si_smenu_prev_item);
  si_smenu_play_item = gtk_image_menu_item_new_from_stock(
                         GTK_STOCK_MEDIA_PLAY , NULL );
  g_signal_connect_swapped( si_smenu_play_item , "activate" ,
                            G_CALLBACK(si_audacious_playback_ctrl) ,
                            GINT_TO_POINTER(SI_AUDACIOUS_PLAYBACK_CTRL_PLAY) );
  gtk_menu_shell_append( GTK_MENU_SHELL(si_smenu) , si_smenu_play_item );
  gtk_widget_show(si_smenu_play_item);
  si_smenu_pause_item = gtk_image_menu_item_new_from_stock(
                          GTK_STOCK_MEDIA_PAUSE , NULL );
  g_signal_connect_swapped( si_smenu_pause_item , "activate" ,
                            G_CALLBACK(si_audacious_playback_ctrl) ,
                            GINT_TO_POINTER(SI_AUDACIOUS_PLAYBACK_CTRL_PAUSE) );
  gtk_menu_shell_append( GTK_MENU_SHELL(si_smenu) , si_smenu_pause_item );
  gtk_widget_show(si_smenu_pause_item);
  si_smenu_stop_item = gtk_image_menu_item_new_from_stock(
                         GTK_STOCK_MEDIA_STOP , NULL );
  g_signal_connect_swapped( si_smenu_stop_item , "activate" ,
                            G_CALLBACK(si_audacious_playback_ctrl) ,
                            GINT_TO_POINTER(SI_AUDACIOUS_PLAYBACK_CTRL_STOP) );
  gtk_menu_shell_append( GTK_MENU_SHELL(si_smenu) , si_smenu_stop_item );
  gtk_widget_show(si_smenu_stop_item);
  si_smenu_next_item = gtk_image_menu_item_new_from_stock(
                         GTK_STOCK_MEDIA_NEXT , NULL );
  g_signal_connect_swapped( si_smenu_next_item , "activate" ,
                            G_CALLBACK(si_audacious_playback_ctrl) ,
                            GINT_TO_POINTER(SI_AUDACIOUS_PLAYBACK_CTRL_NEXT) );
  gtk_menu_shell_append( GTK_MENU_SHELL(si_smenu) , si_smenu_next_item );
  gtk_widget_show(si_smenu_next_item);

  if ( si_cfg.rclick_menu == SI_CFG_RCLICK_MENU_SMALL2 )
  {
    si_smenu_sep_item = gtk_separator_menu_item_new();
    gtk_menu_shell_append( GTK_MENU_SHELL(si_smenu) , si_smenu_sep_item );
    gtk_widget_show(si_smenu_sep_item);
    si_smenu_quit_item = gtk_image_menu_item_new_from_stock(
                           GTK_STOCK_QUIT , NULL );
    g_signal_connect_swapped( si_smenu_quit_item , "activate" ,
                              G_CALLBACK(si_audacious_quit) , NULL );
    gtk_menu_shell_append( GTK_MENU_SHELL(si_smenu) , si_smenu_quit_item );
    gtk_widget_show(si_smenu_quit_item);
  }

  return si_smenu;
}


static void
si_ui_statusicon_smallmenu_recreate ( GtkStatusIcon * icon )
{
  GtkWidget *smenu = g_object_get_data( G_OBJECT(icon) , "smenu" );
  gtk_widget_destroy( GTK_WIDGET(smenu) );
  smenu = si_ui_statusicon_smallmenu_create();
  g_object_set_data( G_OBJECT(icon) , "smenu" , smenu );
  recreate_smallmenu = FALSE;
  return;
}


void
si_ui_statusicon_enable ( gboolean enable )
{
  static GtkStatusIcon *si_applet = NULL;
  static si_aud_hook_tchange_prevs_t *si_aud_hook_tchange_prevs = NULL;

  if (( enable == TRUE ) && ( si_applet == NULL ))
  {
    GtkWidget *si_popup;
    GtkWidget *si_smenu;

    si_applet = si_ui_statusicon_create();
    if ( si_applet == NULL )
    {
      g_warning( "StatusIcon plugin: unable to create a status icon.\n" );
      return;
    }

    si_popup = audacious_fileinfopopup_create();

    g_object_set_data( G_OBJECT(si_applet) , "timer_id" , GINT_TO_POINTER(0) );
    g_object_set_data( G_OBJECT(si_applet) , "timer_active" , GINT_TO_POINTER(0) );

    g_object_set_data( G_OBJECT(si_applet) , "popup_active" , GINT_TO_POINTER(0) );
    g_object_set_data( G_OBJECT(si_applet) , "popup" , si_popup );

    g_signal_connect( G_OBJECT(si_applet) , "button-release-event" ,
                      G_CALLBACK(si_ui_statusicon_cb_btpress) , NULL );
    g_signal_connect( G_OBJECT(si_applet) , "scroll-event" ,
                      G_CALLBACK(si_ui_statusicon_cb_btscroll) , NULL );
    g_signal_connect( G_OBJECT(si_applet) , "query-tooltip" ,
                      G_CALLBACK(si_ui_statusicon_cb_tooltip) , NULL );

    gtk_status_icon_set_has_tooltip( si_applet, TRUE );
    gtk_status_icon_set_visible(si_applet, TRUE);

    /* small menu that can be used in place of the audacious standard one */
    si_smenu = si_ui_statusicon_smallmenu_create();
    g_object_set_data( G_OBJECT(si_applet) , "smenu" , si_smenu );

    aud_hook_associate( "playback begin" , si_ui_statusicon_cb_aud_hook_pbstart , si_applet );
    si_aud_hook_tchange_prevs = g_malloc0(sizeof(si_aud_hook_tchange_prevs_t));
    si_aud_hook_tchange_prevs->title = NULL;
    si_aud_hook_tchange_prevs->filename = NULL;
    si_aud_hook_tchange_prevs->applet = si_applet;
    aud_hook_associate( "title change" , si_ui_statusicon_cb_aud_hook_tchange , si_aud_hook_tchange_prevs );

    return;
  }
  else
  {
    if ( si_applet != NULL )
    {
      GtkWidget *si_smenu = g_object_get_data( G_OBJECT(si_applet) , "smenu" );
      si_ui_statusicon_popup_timer_stop( si_applet ); /* just in case the timer is active */
      aud_hook_dissociate( "playback begin" , si_ui_statusicon_cb_aud_hook_pbstart );
      aud_hook_dissociate( "title change" , si_ui_statusicon_cb_aud_hook_tchange );
      if ( si_aud_hook_tchange_prevs->title != NULL ) g_free( si_aud_hook_tchange_prevs->title );
      if ( si_aud_hook_tchange_prevs->filename != NULL ) g_free( si_aud_hook_tchange_prevs->filename );
      g_free( si_aud_hook_tchange_prevs );
      si_aud_hook_tchange_prevs = NULL;
      si_smenu = NULL;
      g_object_unref(si_applet);
      si_applet = NULL;
    }
    return;
  }
}


void
si_ui_about_show ( void )
{
  static GtkWidget *about_dlg = NULL;
  gchar *about_title;
  gchar *about_text;

  if ( about_dlg != NULL )
  {
    gtk_window_present( GTK_WINDOW(about_dlg) );
    return;
  }

  about_title = g_strdup( _("About Status Icon Plugin") );
  about_text = g_strjoin( "" , "Status Icon Plugin " , SI_VERSION_PLUGIN ,
                 _("\nwritten by Giacomo Lozito < james@develia.org >\n\n"
                   "This plugin provides a status icon, placed in\n"
                   "the system tray area of the window manager.\n") , NULL );

  about_dlg = audacious_info_dialog( about_title , about_text , _("Ok") , FALSE , NULL , NULL );
  g_signal_connect( G_OBJECT(about_dlg) , "destroy" ,
                    G_CALLBACK(gtk_widget_destroyed), &about_dlg );
  g_free( about_text );
  g_free( about_title );

  gtk_widget_show_all( about_dlg );
  return;
}


void
si_ui_prefs_cb_commit ( gpointer prefs_win )
{
  GSList *list = g_object_get_data( G_OBJECT(prefs_win) , "rcm_grp" );
  while ( list != NULL )
  {
    if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(list->data) ) == TRUE )
    {
      si_cfg.rclick_menu = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(list->data),"val"));
      break;
    }
    list = g_slist_next(list);
  }

  list = g_object_get_data( G_OBJECT(prefs_win) , "msa_grp" );
  while ( list != NULL )
  {
    if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(list->data) ) == TRUE )
    {
      si_cfg.scroll_action = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(list->data),"val"));
      break;
    }
    list = g_slist_next(list);
  }

  si_cfg_save();

  /* request the recreation of status icon small-menu if necessary */
  if ( si_cfg.rclick_menu != SI_CFG_RCLICK_MENU_AUD )
    recreate_smallmenu = TRUE;

  gtk_widget_destroy( GTK_WIDGET(prefs_win) );
}


void
si_ui_prefs_show ( void )
{
  static GtkWidget *prefs_win = NULL;
  GtkWidget *prefs_vbox;
  GtkWidget *prefs_rclick_frame, *prefs_rclick_vbox;
  GtkWidget *prefs_rclick_audmenu_rbt, *prefs_rclick_smallmenu1_rbt, *prefs_rclick_smallmenu2_rbt;
  GtkWidget *prefs_scroll_frame, *prefs_scroll_vbox;
  GtkWidget *prefs_scroll_vol_rbt, *prefs_scroll_skip_rbt;
  GtkWidget *prefs_bbar_bbox;
  GtkWidget *prefs_bbar_bt_ok, *prefs_bbar_bt_cancel;
  GdkGeometry prefs_win_hints;

  if ( prefs_win != NULL )
  {
    gtk_window_present( GTK_WINDOW(prefs_win) );
    return;
  }

  prefs_win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_window_set_type_hint( GTK_WINDOW(prefs_win), GDK_WINDOW_TYPE_HINT_DIALOG );
  gtk_window_set_position( GTK_WINDOW(prefs_win), GTK_WIN_POS_CENTER );
  gtk_window_set_title( GTK_WINDOW(prefs_win), _("Status Icon Plugin - Preferences") );
  gtk_container_set_border_width( GTK_CONTAINER(prefs_win) , 10 );
  prefs_win_hints.min_width = 320;
  prefs_win_hints.min_height = -1;
  gtk_window_set_geometry_hints( GTK_WINDOW(prefs_win) , GTK_WIDGET(prefs_win) ,
                                 &prefs_win_hints , GDK_HINT_MIN_SIZE );
  g_signal_connect( G_OBJECT(prefs_win) , "destroy" , G_CALLBACK(gtk_widget_destroyed) , &prefs_win );

  prefs_vbox = gtk_vbox_new( FALSE , 0 );
  gtk_container_add( GTK_CONTAINER(prefs_win) , prefs_vbox );

  prefs_rclick_frame = gtk_frame_new( _("Right-Click Menu") );
  prefs_rclick_vbox = gtk_vbox_new( TRUE , 0 );
  gtk_container_set_border_width( GTK_CONTAINER(prefs_rclick_vbox) , 6 );
  gtk_container_add( GTK_CONTAINER(prefs_rclick_frame) , prefs_rclick_vbox );
  prefs_rclick_audmenu_rbt = gtk_radio_button_new_with_label( NULL ,
                               _("Audacious standard menu") );
  g_object_set_data( G_OBJECT(prefs_rclick_audmenu_rbt) , "val" ,
                     GINT_TO_POINTER(SI_CFG_RCLICK_MENU_AUD) );
  prefs_rclick_smallmenu1_rbt = gtk_radio_button_new_with_label_from_widget(
                                 GTK_RADIO_BUTTON(prefs_rclick_audmenu_rbt) ,
                                 _("Small playback menu #1") );
  g_object_set_data( G_OBJECT(prefs_rclick_smallmenu1_rbt) , "val" ,
                     GINT_TO_POINTER(SI_CFG_RCLICK_MENU_SMALL1) );
  prefs_rclick_smallmenu2_rbt = gtk_radio_button_new_with_label_from_widget(
                                 GTK_RADIO_BUTTON(prefs_rclick_audmenu_rbt) ,
                                 _("Small playback menu #2") );
  g_object_set_data( G_OBJECT(prefs_rclick_smallmenu2_rbt) , "val" ,
                     GINT_TO_POINTER(SI_CFG_RCLICK_MENU_SMALL2) );
  g_object_set_data( G_OBJECT(prefs_win) , "rcm_grp" ,
                     gtk_radio_button_get_group(GTK_RADIO_BUTTON(prefs_rclick_smallmenu1_rbt)) );
  switch ( si_cfg.rclick_menu )
  {
    case SI_CFG_RCLICK_MENU_SMALL1:
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(prefs_rclick_smallmenu1_rbt) , TRUE );
      break;
    case SI_CFG_RCLICK_MENU_SMALL2:
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(prefs_rclick_smallmenu2_rbt) , TRUE );
      break;
    case SI_CFG_RCLICK_MENU_AUD:
    default:
      gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(prefs_rclick_audmenu_rbt) , TRUE );
      break;
  }
  gtk_box_pack_start( GTK_BOX(prefs_rclick_vbox) , prefs_rclick_audmenu_rbt , TRUE , TRUE , 0 );
  gtk_box_pack_start( GTK_BOX(prefs_rclick_vbox) , prefs_rclick_smallmenu1_rbt , TRUE , TRUE , 0 );
  gtk_box_pack_start( GTK_BOX(prefs_rclick_vbox) , prefs_rclick_smallmenu2_rbt , TRUE , TRUE , 0 );
  gtk_box_pack_start( GTK_BOX(prefs_vbox) , prefs_rclick_frame , TRUE , TRUE , 0 );

  prefs_scroll_frame = gtk_frame_new( _("Mouse Scroll Action") );
  prefs_scroll_vbox = gtk_vbox_new( TRUE , 0 );
  gtk_container_set_border_width( GTK_CONTAINER(prefs_scroll_vbox) , 6 );
  gtk_container_add( GTK_CONTAINER(prefs_scroll_frame) , prefs_scroll_vbox );
  prefs_scroll_vol_rbt = gtk_radio_button_new_with_label( NULL ,
                               _("Change volume") );
  g_object_set_data( G_OBJECT(prefs_scroll_vol_rbt) , "val" ,
                     GINT_TO_POINTER(SI_CFG_SCROLL_ACTION_VOLUME) );
  prefs_scroll_skip_rbt = gtk_radio_button_new_with_label_from_widget(
                                 GTK_RADIO_BUTTON(prefs_scroll_vol_rbt) ,
                                 _("Change playing song") );
  g_object_set_data( G_OBJECT(prefs_scroll_skip_rbt) , "val" ,
                     GINT_TO_POINTER(SI_CFG_SCROLL_ACTION_SKIP) );
  g_object_set_data( G_OBJECT(prefs_win) , "msa_grp" ,
                     gtk_radio_button_get_group(GTK_RADIO_BUTTON(prefs_scroll_skip_rbt)) );
  if ( si_cfg.scroll_action == SI_CFG_SCROLL_ACTION_VOLUME )
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(prefs_scroll_vol_rbt) , TRUE );
  else
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(prefs_scroll_skip_rbt) , TRUE );
  gtk_box_pack_start( GTK_BOX(prefs_scroll_vbox) , prefs_scroll_vol_rbt , TRUE , TRUE , 0 );
  gtk_box_pack_start( GTK_BOX(prefs_scroll_vbox) , prefs_scroll_skip_rbt , TRUE , TRUE , 0 );
  gtk_box_pack_start( GTK_BOX(prefs_vbox) , prefs_scroll_frame , TRUE , TRUE , 0 );

  /* horizontal separator and buttons */
  gtk_box_pack_start( GTK_BOX(prefs_vbox) , gtk_hseparator_new() , FALSE , FALSE , 4 );
  prefs_bbar_bbox = gtk_hbutton_box_new();
  gtk_button_box_set_layout( GTK_BUTTON_BOX(prefs_bbar_bbox) , GTK_BUTTONBOX_END );
  prefs_bbar_bt_cancel = gtk_button_new_from_stock( GTK_STOCK_CANCEL );
  g_signal_connect_swapped( G_OBJECT(prefs_bbar_bt_cancel) , "clicked" ,
                            G_CALLBACK(gtk_widget_destroy) , prefs_win );
  gtk_container_add( GTK_CONTAINER(prefs_bbar_bbox) , prefs_bbar_bt_cancel );
  prefs_bbar_bt_ok = gtk_button_new_from_stock( GTK_STOCK_OK );
  gtk_container_add( GTK_CONTAINER(prefs_bbar_bbox) , prefs_bbar_bt_ok );
  g_signal_connect_swapped( G_OBJECT(prefs_bbar_bt_ok) , "clicked" ,
                            G_CALLBACK(si_ui_prefs_cb_commit) , prefs_win );
  gtk_box_pack_start( GTK_BOX(prefs_vbox) , prefs_bbar_bbox , FALSE , FALSE , 0 );

  gtk_widget_show_all( prefs_win );
}
