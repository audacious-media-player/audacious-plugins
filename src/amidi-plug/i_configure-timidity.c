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


#include "i_configure-timidity.h"
#include "backend-timidity/backend-timidity-icon.xpm"


void i_configure_gui_tab_timi( GtkWidget * timi_page_alignment ,
                               gpointer backend_list_p ,
                               gpointer commit_button )
{
  GtkWidget *timi_page_vbox;
  GtkWidget *title_widget;
  GtkWidget *content_vbox; /* this vbox will contain two items of equal space (50%/50%) */
  GSList * backend_list = backend_list_p;
  gboolean timi_module_ok = FALSE;
  gchar * timi_module_pathfilename;

  timi_page_vbox = gtk_vbox_new( FALSE , 0 );

  title_widget = i_configure_gui_draw_title( _("TIMIDITY BACKEND CONFIGURATION") );
  gtk_box_pack_start( GTK_BOX(timi_page_vbox) , title_widget , FALSE , FALSE , 2 );

  content_vbox = gtk_vbox_new( TRUE , 2 );

  /* check if the TiMidity module is available */
  while ( backend_list != NULL )
  {
    amidiplug_sequencer_backend_name_t * mn = backend_list->data;
    if ( !strcmp( mn->name , "timidity" ) )
    {
      timi_module_ok = TRUE;
      timi_module_pathfilename = mn->filename;
      break;
    }
    backend_list = backend_list->next;
  }

  if ( timi_module_ok )
  {
  }
  else
  {
    /* display "not available" information */
    GtkWidget * info_label;
    info_label = gtk_label_new( _("TiMidity Backend not loaded or not available") );
    gtk_box_pack_start( GTK_BOX(timi_page_vbox) , info_label , FALSE , FALSE , 2 );
  }

  gtk_box_pack_start( GTK_BOX(timi_page_vbox) , content_vbox , TRUE , TRUE , 2 );
  gtk_container_add( GTK_CONTAINER(timi_page_alignment) , timi_page_vbox );
}


void i_configure_gui_tablabel_timi( GtkWidget * timi_page_alignment ,
                                  gpointer backend_list_p ,
                                  gpointer commit_button )
{
  GtkWidget *pagelabel_vbox, *pagelabel_image, *pagelabel_label;
  GdkPixbuf *pagelabel_image_pix;
  pagelabel_vbox = gtk_vbox_new( FALSE , 1 );
  pagelabel_image_pix = gdk_pixbuf_new_from_xpm_data( (const gchar **)backend_timidity_icon_xpm );
  pagelabel_image = gtk_image_new_from_pixbuf( pagelabel_image_pix ); g_object_unref( pagelabel_image_pix );
  pagelabel_label = gtk_label_new( "" );
  gtk_label_set_markup( GTK_LABEL(pagelabel_label) , _("<span size=\"smaller\">TiMidity\nbackend</span>") );
  gtk_label_set_justify( GTK_LABEL(pagelabel_label) , GTK_JUSTIFY_CENTER );
  gtk_box_pack_start( GTK_BOX(pagelabel_vbox) , pagelabel_image , FALSE , FALSE , 1 );
  gtk_box_pack_start( GTK_BOX(pagelabel_vbox) , pagelabel_label , FALSE , FALSE , 1 );
  gtk_container_add( GTK_CONTAINER(timi_page_alignment) , pagelabel_vbox );
  gtk_widget_show_all( timi_page_alignment );
  return;
}
