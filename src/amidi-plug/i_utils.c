/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2006
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


#include "i_utils.h"
#include <gtk/gtk.h>
#include "amidi-plug.logo.xpm"


void i_about_gui( void )
{
  static GtkWidget * aboutwin = NULL;
  GtkWidget *logo_image;
  GdkPixbuf *logo_pixbuf;

  if ( aboutwin != NULL )
    return;

  aboutwin = gtk_dialog_new_with_buttons (_("About AMIDI-Plug"), NULL, 0,
   GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
  gtk_window_set_resizable( GTK_WINDOW(aboutwin) , FALSE );

  g_signal_connect (aboutwin, "response", (GCallback) gtk_widget_destroy, NULL);
  g_signal_connect( G_OBJECT(aboutwin) , "destroy" , G_CALLBACK(gtk_widget_destroyed) , &aboutwin );

  GtkWidget * vbox = gtk_dialog_get_content_area ((GtkDialog *) aboutwin);

  logo_pixbuf = gdk_pixbuf_new_from_xpm_data( (const gchar **)amidiplug_xpm_logo );
  logo_image = gtk_image_new_from_pixbuf( logo_pixbuf );
  gtk_box_pack_start ((GtkBox *) vbox, logo_image, FALSE, FALSE, 0);
  g_object_unref( logo_pixbuf );

  gchar * text = g_strjoin (NULL, _("AMIDI-Plug "), AMIDIPLUG_VERSION,
                                       _("\nmodular MIDI music player\n"
                                         "http://www.develia.org/projects.php?p=amidiplug\n\n"
                                         "written by Giacomo Lozito\n"
                                         "<james@develia.org>\n\n"
                                         "special thanks to...\n\n"
                                         "Clemens Ladisch and Jaroslav Kysela\n"
                                         "for their cool programs aplaymidi and amixer; those\n"
                                         "were really useful, along with alsa-lib docs, in order\n"
                                         "to learn more about the ALSA API\n\n"
                                         "Alfredo Spadafina\n"
                                         "for the nice midi keyboard logo\n\n"
                                         "Tony Vroon\n"
                                         "for the good help with alpha testing"), NULL);

  GtkWidget * label = gtk_label_new (text);
  gtk_box_pack_start ((GtkBox *) vbox, label, FALSE, FALSE, 0);
  g_free (text);

  gtk_widget_show_all( aboutwin );
}


gpointer i_message_gui( gchar * title , gchar * message ,
                        gint type , gpointer parent_win , gboolean show_win )
{
  GtkWidget *win;
  GtkMessageType mtype = GTK_MESSAGE_INFO;

  switch ( type )
  {
    case AMIDIPLUG_MESSAGE_INFO:
      mtype = GTK_MESSAGE_INFO; break;
    case AMIDIPLUG_MESSAGE_WARN:
      mtype = GTK_MESSAGE_WARNING; break;
    case AMIDIPLUG_MESSAGE_ERR:
      mtype = GTK_MESSAGE_ERROR; break;
  }

  if ( parent_win != NULL )
    win = gtk_message_dialog_new ((GtkWindow *) parent_win,
     GTK_DIALOG_DESTROY_WITH_PARENT, mtype, GTK_BUTTONS_OK, "%s", message);
  else
    win = gtk_message_dialog_new (NULL, 0, mtype, GTK_BUTTONS_OK, "%s", message);

  gtk_window_set_title( GTK_WINDOW(win) , title );
  g_signal_connect_swapped( G_OBJECT(win) , "response" , G_CALLBACK(gtk_widget_destroy) , win );

  if ( show_win == TRUE )
    gtk_widget_show_all( win );

  return win;
}
