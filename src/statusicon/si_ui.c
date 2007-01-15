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
#include <glib.h>
#include <glib/gi18n.h>
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


static GtkWidget *
si_ui_rmenu_create ( GtkWidget * evbox )
{
  GtkWidget *menu;
  GtkWidget *menuitem;

  menu = gtk_menu_new();

  /* gtk_widget_show_all( GTK_WIDGET(menu) ); */
  return menu;
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

/*
    case 3:
    {
      GtkWidget *si_rmenu = GTK_WIDGET(g_object_get_data( G_OBJECT(evbox) , "rmenu" ));
      gtk_menu_popup( GTK_MENU(si_rmenu) , NULL , NULL ,
                      NULL , NULL , event->button , event->time );
      break;
    }
*/
  }

  return FALSE;
}


void
si_ui_statusicon_show ( void )
{
  GtkWidget *si_image;
  GtkWidget *si_rmenu;
  GdkPixbuf *si_pixbuf;
  GtkTrayIcon *si_applet;

  si_applet = si_ui_statusicon_create();
  if ( si_applet == NULL )
  {
    g_warning( "StatusIcon plugin: unable to create a status icon.\n" );
    return;
  }

  si_pixbuf = gdk_pixbuf_new_from_xpm_data( (const char**)si_xpm );
  si_image = gtk_image_new_from_pixbuf( si_pixbuf );
  g_object_unref( si_pixbuf );

  si_evbox = gtk_event_box_new();
  si_rmenu = si_ui_rmenu_create( si_evbox );

  g_object_set_data( G_OBJECT(si_evbox) , "rmenu" , si_rmenu );
  g_object_set_data( G_OBJECT(si_evbox) , "applet" , si_applet );
  g_signal_connect( G_OBJECT(si_evbox) , "button-press-event" ,
                    G_CALLBACK(si_ui_statusicon_cb_btpress) , NULL );

  gtk_container_add( GTK_CONTAINER(si_evbox), si_image );
  gtk_container_add( GTK_CONTAINER(si_applet), si_evbox );

  gtk_widget_show_all( GTK_WIDGET(si_applet) );
  return;
}


void
si_ui_statusicon_hide ( void )
{
  if ( si_evbox != NULL )
  {
    GtkTrayIcon *si_applet = g_object_get_data( G_OBJECT(si_evbox) , "applet" );
    GtkWidget *si_rmenu = g_object_get_data( G_OBJECT(si_evbox) , "rmenu" );
    gtk_widget_destroy( GTK_WIDGET(si_evbox) );
    gtk_widget_destroy( GTK_WIDGET(si_rmenu) );
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
