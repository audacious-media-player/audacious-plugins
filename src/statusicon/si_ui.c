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
#include "si_common.h"
#include "gtktrayicon.h"
#include "si.xpm"
#include <audacious/playlist.h>
#include <audacious/titlestring.h>
#include <audacious/ui_fileinfopopup.h>
#include <audacious/util.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>


static GtkWidget * si_evbox = NULL;


static GtkTrayIcon *
si_ui_statusicon_create ( void )
{
  GtkTrayIcon *si_applet = NULL;

  si_applet = _gtk_tray_icon_new( "audacious" );

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
      audacious_menu_main_show( event->x_root , event->y_root , 3 , event->time );
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
    audacious_fileinfopopup_show_from_tuple( popup , tuple );

    g_object_set_data( G_OBJECT(evbox) , "popup_active" , GINT_TO_POINTER(1) );
  }

  g_object_set_data( G_OBJECT(evbox) , "timer_id" , GINT_TO_POINTER(0) );
  g_object_set_data( G_OBJECT(evbox) , "timer_active" , GINT_TO_POINTER(0) );
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

  orientation = _gtk_tray_icon_get_orientation( GTK_TRAY_ICON(si_applet) );
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


void
si_ui_statusicon_show ( void )
{
  GtkWidget *si_image;
  GtkWidget *si_popup;
  GtkTrayIcon *si_applet;
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
    
  return;
}


void
si_ui_statusicon_hide ( void )
{
  if ( si_evbox != NULL )
  {
    GtkTrayIcon *si_applet = g_object_get_data( G_OBJECT(si_evbox) , "applet" );
    si_ui_statusicon_popup_timer_stop( si_evbox ); /* just in case the timer is active */
    gtk_widget_destroy( GTK_WIDGET(si_evbox) );
    gtk_widget_destroy( GTK_WIDGET(si_applet) );
  }
  return;
}


void
si_ui_about_show ( void )
{
  GtkWidget *about_dlg = gtk_message_dialog_new(
    NULL , 0 , GTK_MESSAGE_INFO , GTK_BUTTONS_CLOSE ,
    _( "Status Icon Plugin " SI_VERSION_PLUGIN "\n"
       "written by Giacomo Lozito < james@develia.org >\n\n"
       "This plugin provides a status icon, placed in\n"
       "the system tray area of the window manager.\n" ) );
  gtk_dialog_run( GTK_DIALOG(about_dlg) );
  gtk_widget_destroy( about_dlg );
  return;
}
