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
#include "gtktrayicon.h"
#include "si.xpm"
#include <audacious/playlist.h>
#include <audacious/titlestring.h>
#include <audacious/ui_fileinfopopup.h>
#include <audacious/util.h>
#include <audacious/i18n.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>


static void si_ui_statusicon_popup_timer_start ( GtkWidget * );
static void si_ui_statusicon_popup_timer_stop ( GtkWidget * );
static void si_ui_statusicon_smallmenu_show ( gint x, gint y, guint button, guint32 time , gpointer );

extern si_cfg_t si_cfg;


/* this stuff required to make titlechange hook work properly */
typedef struct
{
  gchar *title;
  gchar *filename;
  gpointer evbox;
}
si_hook_tchange_prevs_t;


static AudGtkTrayIcon *
si_ui_statusicon_create ( void )
{
  AudGtkTrayIcon *si_applet = NULL;

  si_applet = _aud_gtk_tray_icon_new( "audacious" );

  gtk_widget_show( GTK_WIDGET(si_applet) );

  return si_applet;
}


static gboolean
si_ui_statusicon_cb_btpress ( GtkWidget * evbox , GdkEventButton * event )
{
  switch ( event->button )
  {
    case 1:
    {
      si_audacious_toggle_visibility();
      break;
    }

    case 3:
    {
      switch ( si_cfg.rclick_menu )
      {
        case SI_CFG_RCLICK_MENU_SMALL:
          si_ui_statusicon_smallmenu_show( event->x_root , event->y_root , 3 , event->time , evbox );
          break;
        case SI_CFG_RCLICK_MENU_AUD:
        default:
          audacious_menu_main_show( event->x_root , event->y_root , 3 , event->time );
          break;
      }
      break;
    }
  }

  return FALSE;
}


static gboolean
si_ui_statusicon_cb_btscroll ( GtkWidget * evbox , GdkEventScroll * event )
{
  switch ( event->direction )
  {
    case GDK_SCROLL_UP:
      si_audacious_volume_change( 5 );
      break;
    case GDK_SCROLL_DOWN:
      si_audacious_volume_change( -5 );
      break;
  }

  return FALSE;
}


static gboolean
si_ui_statusicon_popup_show ( gpointer evbox )
{
  if ( GPOINTER_TO_INT(g_object_get_data( G_OBJECT(evbox) , "timer_active" )) == 1 )
  {
    TitleInput *tuple;
    Playlist *pl_active = playlist_get_active();
    gint pos = playlist_get_position(pl_active);
    GtkWidget *popup = g_object_get_data( G_OBJECT(evbox) , "popup" );

    tuple = playlist_get_tuple( pl_active , pos );
    if ( ( tuple == NULL ) || ( tuple->length < 1 ) )
    {
      gchar *title = playlist_get_songtitle( pl_active , pos );
      audacious_fileinfopopup_show_from_title( popup , title );
      g_free( title );
    }
    else
    {
      audacious_fileinfopopup_show_from_tuple( popup , tuple );
    }

    g_object_set_data( G_OBJECT(evbox) , "popup_active" , GINT_TO_POINTER(1) );
  }

  si_ui_statusicon_popup_timer_stop( evbox );
  return FALSE;
}


static void
si_ui_statusicon_popup_hide ( gpointer evbox )
{
  if ( GPOINTER_TO_INT(g_object_get_data( G_OBJECT(evbox) , "popup_active" )) == 1 )
  {
    GtkWidget *popup = g_object_get_data( G_OBJECT(evbox) , "popup" );
    g_object_set_data( G_OBJECT(evbox) , "popup_active" , GINT_TO_POINTER(0) );
    audacious_fileinfopopup_hide( popup , NULL );
  }
}


static void
si_ui_statusicon_popup_timer_start ( GtkWidget * evbox )
{
  gint timer_id = g_timeout_add( 500 , si_ui_statusicon_popup_show , evbox );
  g_object_set_data( G_OBJECT(evbox) , "timer_id" , GINT_TO_POINTER(timer_id) );
  g_object_set_data( G_OBJECT(evbox) , "timer_active" , GINT_TO_POINTER(1) );
  return;
}


static void
si_ui_statusicon_popup_timer_stop ( GtkWidget * evbox )
{
  if ( GPOINTER_TO_INT(g_object_get_data(G_OBJECT(evbox),"timer_active")) == 1 )
    g_source_remove( GPOINTER_TO_INT(g_object_get_data(G_OBJECT(evbox),"timer_id")) );

  g_object_set_data( G_OBJECT(evbox) , "timer_id" , GINT_TO_POINTER(0) );
  g_object_set_data( G_OBJECT(evbox) , "timer_active" , GINT_TO_POINTER(0) );
  return;
}


static void
si_ui_statusicon_cb_hook_pbstart ( gpointer plentry_gp , gpointer evbox )
{
  if ( ( GPOINTER_TO_INT(g_object_get_data( G_OBJECT(evbox) , "popup_active" )) == 1 ) &&
       ( plentry_gp != NULL ) )
  {
g_print("hook change? 1\n");
    si_ui_statusicon_popup_hide( evbox );
    si_ui_statusicon_popup_timer_start( evbox );
  }
}


static void
si_ui_statusicon_cb_hook_tchange ( gpointer plentry_gp , gpointer prevs_gp )
{
  /* NOTE: this is quite intricated, but it works nicely and it's still
     much better than polling; wonder if it can be simplified with some
     help from the core player */
  si_hook_tchange_prevs_t *prevs = prevs_gp;
  PlaylistEntry *pl_entry = plentry_gp;
  if ( ( GPOINTER_TO_INT(g_object_get_data( G_OBJECT(prevs->evbox) , "popup_active" )) == 1 ) &&
       ( plentry_gp != NULL ) )
  {
    if ( ( prevs->title != NULL ) && ( prevs->filename != NULL ) )
    {
      if ( ( pl_entry->filename != NULL ) &&
           ( !strcmp(pl_entry->filename,prevs->filename) ) )
      {
        if ( ( pl_entry->title != NULL ) &&
             ( strcmp(pl_entry->title,prevs->title) ) )
        {
          si_ui_statusicon_popup_hide( prevs->evbox );
          si_ui_statusicon_popup_timer_start( prevs->evbox );
          g_free( prevs->title );
          prevs->title = g_strdup(pl_entry->title);
        }
      }
      else
      {
        g_free(prevs->filename);
        prevs->filename = g_strdup(pl_entry->filename);
        /* if filename changes, reset title as well */
        if ( prevs->title != NULL )
          g_free(prevs->title);
        prevs->title = g_strdup(pl_entry->title);
      }
    }
    else
    {
      if ( prevs->title != NULL )
        g_free(prevs->title);
      prevs->title = g_strdup(pl_entry->title);
      if ( prevs->filename != NULL )
        g_free(prevs->filename);
      prevs->filename = g_strdup(pl_entry->filename);
    }
  }
  else if ( ( prevs->title != NULL ) && ( strcmp(pl_entry->title,prevs->title) ) )
  {
    g_free(prevs->title);
    prevs->title = g_strdup(pl_entry->title);
  }
}


static gboolean
si_ui_statusicon_cb_popup ( GtkWidget * evbox , GdkEvent * event )
{
  if ((event->type == GDK_LEAVE_NOTIFY || event->type == GDK_ENTER_NOTIFY) &&
    event->crossing.detail == GDK_NOTIFY_INFERIOR)
      return FALSE;

  if ( event->type != GDK_KEY_PRESS && event->type != GDK_KEY_RELEASE )
  {
    GtkWidget *event_widget = gtk_get_event_widget( event );
    if ( event_widget != evbox )
      return FALSE;
  }

  switch (event->type)
  {
    case GDK_EXPOSE:
      /* do nothing */
      break;

    case GDK_ENTER_NOTIFY:
        si_ui_statusicon_popup_timer_start( evbox );
      break;

     case GDK_LEAVE_NOTIFY:
       si_ui_statusicon_popup_timer_stop( evbox );
       if ( GPOINTER_TO_INT(g_object_get_data( G_OBJECT(evbox) , "popup_active" )) == 1 )
         si_ui_statusicon_popup_hide( evbox );
       break;

     case GDK_MOTION_NOTIFY:
       break; /* ignore */

     case GDK_BUTTON_PRESS:
     case GDK_BUTTON_RELEASE:
     case GDK_KEY_PRESS:
     case GDK_KEY_RELEASE:
     case GDK_PROXIMITY_IN:
     case GDK_SCROLL:
       si_ui_statusicon_popup_timer_stop( evbox );
       if ( GPOINTER_TO_INT(g_object_get_data( G_OBJECT(evbox) , "popup_active" )) == 1 )
         si_ui_statusicon_popup_hide( evbox );
       break;

     default:
       break;
  }

  return FALSE;
}


static void
si_ui_statusicon_image_update ( GtkWidget * image )
{
  GdkPixbuf *si_pixbuf, *si_scaled_pixbuf;
  gint size = GPOINTER_TO_INT(g_object_get_data( G_OBJECT(image) , "size" ));
  static gchar *wmname = NULL;

  /* sometimes, KDE won't give the correct size-allocation; workaround this */
  if ( wmname == NULL )
  {
    GdkScreen *screen = gdk_screen_get_default();
    if ( screen != NULL )
      wmname = (gchar*)gdk_x11_screen_get_window_manager_name( screen );
  }
  if ( ( size > 22 ) && ( wmname != NULL ) && ( !strcmp("KWin",wmname) ) )
    size = 22;

  si_pixbuf = gdk_pixbuf_new_from_xpm_data( (const char**)si_xpm );
  si_scaled_pixbuf = gdk_pixbuf_scale_simple( si_pixbuf , size , size , GDK_INTERP_BILINEAR );
  gtk_image_set_from_pixbuf( GTK_IMAGE(image) , si_scaled_pixbuf );
  g_object_unref( si_pixbuf );
  g_object_unref( si_scaled_pixbuf );

  return;
}


static void
si_ui_statusicon_cb_image_sizalloc ( GtkWidget * image , GtkAllocation * allocation , gpointer si_applet )
{
  GtkOrientation orientation;
  static gint prev_size = 0;
  gint size = 0;

  orientation = _aud_gtk_tray_icon_get_orientation( AUD_GTK_TRAY_ICON(si_applet) );
  if ( orientation == GTK_ORIENTATION_HORIZONTAL )
    size = allocation->height;
  else
    size = allocation->width;

  if ( prev_size != size )
  {
    prev_size = size;
    g_object_set_data( G_OBJECT(image) , "size" , GINT_TO_POINTER(size) );
    si_ui_statusicon_image_update( image );
  }

  return;
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
  si_smenu_sep_item = gtk_separator_menu_item_new();
  gtk_menu_shell_append( GTK_MENU_SHELL(si_smenu) , si_smenu_sep_item );
  gtk_widget_show(si_smenu_sep_item);
  si_smenu_eject_item = gtk_image_menu_item_new_from_stock(
                          GTK_STOCK_OPEN , NULL );
  g_signal_connect_swapped( si_smenu_eject_item , "activate" ,
                            G_CALLBACK(si_audacious_playback_ctrl) ,
                            GINT_TO_POINTER(SI_AUDACIOUS_PLAYBACK_CTRL_EJECT) );
  gtk_menu_shell_append( GTK_MENU_SHELL(si_smenu) , si_smenu_eject_item );
  gtk_widget_show(si_smenu_eject_item);

  return si_smenu;
}


void
si_ui_statusicon_enable ( gboolean enable )
{
  static GtkWidget *si_evbox = NULL;
  static GtkWidget *si_smenu = NULL;
  static si_hook_tchange_prevs_t *si_hook_tchange_prevs = NULL;

  if (( enable == TRUE ) && ( si_evbox == NULL ))
  {
    GtkWidget *si_image;
    GtkWidget *si_popup;
    AudGtkTrayIcon *si_applet;
    GtkRequisition req;
    GtkAllocation allocation;

    si_applet = si_ui_statusicon_create();
    if ( si_applet == NULL )
    {
      g_warning( "StatusIcon plugin: unable to create a status icon.\n" );
      return;
    }

    si_image = gtk_image_new();
    g_object_set_data( G_OBJECT(si_image) , "size" , GINT_TO_POINTER(0) );

    g_signal_connect( si_image , "size-allocate" ,
                      G_CALLBACK(si_ui_statusicon_cb_image_sizalloc) , si_applet );

    si_evbox = gtk_event_box_new();
    si_popup = audacious_fileinfopopup_create();

    g_object_set_data( G_OBJECT(si_evbox) , "applet" , si_applet );

    g_object_set_data( G_OBJECT(si_evbox) , "timer_id" , GINT_TO_POINTER(0) );
    g_object_set_data( G_OBJECT(si_evbox) , "timer_active" , GINT_TO_POINTER(0) );

    g_object_set_data( G_OBJECT(si_evbox) , "popup_active" , GINT_TO_POINTER(0) );
    g_object_set_data( G_OBJECT(si_evbox) , "popup" , si_popup );

    g_signal_connect( G_OBJECT(si_evbox) , "button-press-event" ,
                      G_CALLBACK(si_ui_statusicon_cb_btpress) , NULL );
    g_signal_connect( G_OBJECT(si_evbox) , "scroll-event" ,
                      G_CALLBACK(si_ui_statusicon_cb_btscroll) , NULL );
    g_signal_connect_after( G_OBJECT(si_evbox) , "event-after" ,
                            G_CALLBACK(si_ui_statusicon_cb_popup) , NULL );

    gtk_container_add( GTK_CONTAINER(si_evbox), si_image );
    gtk_container_add( GTK_CONTAINER(si_applet), si_evbox );

    gtk_widget_show_all( GTK_WIDGET(si_applet) );

    gtk_widget_size_request( GTK_WIDGET(si_applet) , &req );
    allocation.x = 0;
    allocation.y = 0;
    allocation.width = req.width;
    allocation.height = req.height;
    gtk_widget_size_allocate( GTK_WIDGET(si_applet) , &allocation );

    /* small menu that can be used in place of the audacious standard one */
    si_smenu = si_ui_statusicon_smallmenu_create();
    g_object_set_data( G_OBJECT(si_evbox) , "smenu" , si_smenu );

    hook_associate( "playback begin" , si_ui_statusicon_cb_hook_pbstart , si_evbox );
    si_hook_tchange_prevs = g_malloc0(sizeof(si_hook_tchange_prevs_t));
    si_hook_tchange_prevs->title = NULL;
    si_hook_tchange_prevs->filename = NULL;
    si_hook_tchange_prevs->evbox = si_evbox;
    hook_associate( "playlist set info" , si_ui_statusicon_cb_hook_tchange , si_hook_tchange_prevs );

    return;
  }
  else
  {
    if ( si_evbox != NULL )
    {
      AudGtkTrayIcon *si_applet = g_object_get_data( G_OBJECT(si_evbox) , "applet" );
      si_ui_statusicon_popup_timer_stop( si_evbox ); /* just in case the timer is active */
      gtk_widget_destroy( GTK_WIDGET(si_evbox) );
      gtk_widget_destroy( GTK_WIDGET(si_applet) );
      gtk_widget_destroy( GTK_WIDGET(si_smenu) );
      hook_dissociate( "playback begin" , si_ui_statusicon_cb_hook_pbstart );
      hook_dissociate( "playlist set info" , si_ui_statusicon_cb_hook_tchange );
      if ( si_hook_tchange_prevs->title != NULL ) g_free( si_hook_tchange_prevs->title );
      if ( si_hook_tchange_prevs->filename != NULL ) g_free( si_hook_tchange_prevs->filename );
      g_free( si_hook_tchange_prevs );
      si_hook_tchange_prevs = NULL;
      si_smenu = NULL;
      si_evbox = NULL;
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

  about_dlg = xmms_show_message( about_title , about_text , _("Ok") , FALSE , NULL , NULL );
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
  GSList *rcm_grp = g_object_get_data( G_OBJECT(prefs_win) , "rcm_grp" );
  while ( rcm_grp != NULL )
  {
    if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(rcm_grp->data) ) == TRUE )
    {
      si_cfg.rclick_menu = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(rcm_grp->data),"val"));
      break;
    }
    rcm_grp = g_slist_next(rcm_grp);
  }
  si_cfg_save();
  gtk_widget_destroy( GTK_WIDGET(prefs_win) );
}


void
si_ui_prefs_show ( void )
{
  static GtkWidget *prefs_win = NULL;
  GtkWidget *prefs_vbox;
  GtkWidget *prefs_rclick_frame, *prefs_rclick_vbox;
  GtkWidget *prefs_rclick_audmenu_rbt, *prefs_rclick_smallmenu_rbt;
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
  gtk_window_set_title( GTK_WINDOW(prefs_win), _("Audacious OSD - Preferences") );
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
  prefs_rclick_smallmenu_rbt = gtk_radio_button_new_with_label_from_widget(
                                 GTK_RADIO_BUTTON(prefs_rclick_audmenu_rbt) ,
                                 _("Small playback menu") );
  g_object_set_data( G_OBJECT(prefs_rclick_smallmenu_rbt) , "val" ,
                     GINT_TO_POINTER(SI_CFG_RCLICK_MENU_SMALL) );
  g_object_set_data( G_OBJECT(prefs_win) , "rcm_grp" ,
                     gtk_radio_button_get_group(GTK_RADIO_BUTTON(prefs_rclick_smallmenu_rbt)) );
  if ( si_cfg.rclick_menu == SI_CFG_RCLICK_MENU_AUD )
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(prefs_rclick_audmenu_rbt) , TRUE );
  else
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(prefs_rclick_smallmenu_rbt) , TRUE );
  gtk_box_pack_start( GTK_BOX(prefs_rclick_vbox) , prefs_rclick_audmenu_rbt , TRUE , TRUE , 0 );
  gtk_box_pack_start( GTK_BOX(prefs_rclick_vbox) , prefs_rclick_smallmenu_rbt , TRUE , TRUE , 0 );
  gtk_box_pack_start( GTK_BOX(prefs_vbox) , prefs_rclick_frame , TRUE , TRUE , 0 );

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
