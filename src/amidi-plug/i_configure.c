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

#include <gtk/gtk.h>

#include <audacious/drct.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>

#include "config.h"
#include "i_configure.h"
#include "i_configure_private.h"
#include "i_configure_file.h"
#include "i_backend.h"
#include "i_configure-ap.h"
#include "i_configure-alsa.h"
#include "i_configure-fluidsynth.h"
#include "i_utils.h"

#ifdef AMIDIPLUG_ALSA
#define DEFAULT_BACKEND "alsa"
#elif defined AMIDIPLUG_FLUIDSYNTH
#define DEFAULT_BACKEND "fluidsynth"
#else
#define DEFAULT_BACKEND ""
#endif

amidiplug_cfg_backend_t * amidiplug_cfg_backend;

static void i_configure_cancel (GtkWidget * configwin);
static void i_configure_commit (GtkWidget * configwin);

void i_configure_cfg_backend_alloc( void );
void i_configure_cfg_backend_free( void );
void i_configure_cfg_backend_save( void );
void i_configure_cfg_backend_read( void );
void i_configure_cfg_ap_save( void );
void i_configure_cfg_ap_read( void );


void i_configure_ev_browse_for_entry( GtkWidget * target_entry )
{
  GtkWidget *parent_window = gtk_widget_get_toplevel( target_entry );
  GtkFileChooserAction act = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(target_entry),"fc-act"));
  if (gtk_widget_is_toplevel (parent_window))
  {
    GtkWidget *browse_dialog = gtk_file_chooser_dialog_new( _("AMIDI-Plug - select file") ,
                                                            GTK_WINDOW(parent_window) , act ,
                                                            GTK_STOCK_CANCEL , GTK_RESPONSE_CANCEL ,
                                                            GTK_STOCK_OPEN , GTK_RESPONSE_ACCEPT , NULL );
    if ( strcmp( gtk_entry_get_text(GTK_ENTRY(target_entry)) , "" ) )
      gtk_file_chooser_set_filename( GTK_FILE_CHOOSER(browse_dialog) ,
                                     gtk_entry_get_text(GTK_ENTRY(target_entry)) );
    if ( gtk_dialog_run(GTK_DIALOG(browse_dialog)) == GTK_RESPONSE_ACCEPT )
    {
      gchar *filename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(browse_dialog) );
      gtk_entry_set_text( GTK_ENTRY(target_entry) , filename );
      DEBUGMSG( "selected file: %s\n" , filename );
      g_free( filename );
    }
    gtk_widget_destroy( browse_dialog );
  }
}

static void response_cb (GtkWidget * window, int response)
{
    if (response == GTK_RESPONSE_OK)
    {
        if (aud_drct_get_playing ())
            aud_drct_stop ();

        g_signal_emit_by_name (window, "ap-commit");
        i_configure_commit (window);
    }
    else
        i_configure_cancel (window);
}

void i_configure_gui( void )
{
  static GtkWidget *configwin = NULL;

  if ( configwin != NULL )
  {
    DEBUGMSG( "config window is already open!\n" );
    return;
  }

  /* get configuration information for backends */
  i_configure_cfg_backend_alloc();
  i_configure_cfg_backend_read();

  configwin = gtk_dialog_new_with_buttons (_("AMIDI-Plug Settings"), NULL, 0,
   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

  if (! g_signal_lookup ("ap-commit", G_OBJECT_TYPE (configwin)))
    g_signal_new ("ap-commit", G_OBJECT_TYPE (configwin), G_SIGNAL_ACTION, 0,
     NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  g_signal_connect (configwin, "response", (GCallback) response_cb, NULL);
  g_signal_connect (configwin, "destroy", (GCallback) gtk_widget_destroyed, & configwin);

  GtkWidget * configwin_vbox = gtk_dialog_get_content_area ((GtkDialog *) configwin);

  GtkWidget * configwin_notebook = gtk_notebook_new ();
  gtk_notebook_set_tab_pos( GTK_NOTEBOOK(configwin_notebook) , GTK_POS_LEFT );
  gtk_box_pack_start( GTK_BOX(configwin_vbox) , configwin_notebook , TRUE , TRUE , 2 );

  /* GET A LIST OF BACKENDS */
  GSList * backend_list = i_backend_list_lookup ();
  GSList * backend_list_h = backend_list;

  /* AMIDI-PLUG PREFERENCES TAB */
  GtkWidget * ap_pagelabel_alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
  GtkWidget * ap_page_alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_alignment_set_padding( GTK_ALIGNMENT(ap_page_alignment) , 3 , 3 , 8 , 3 );
  i_configure_gui_tab_ap (ap_page_alignment, backend_list, configwin);
  i_configure_gui_tablabel_ap (ap_pagelabel_alignment, backend_list, configwin);
  gtk_notebook_append_page( GTK_NOTEBOOK(configwin_notebook) ,
                            ap_page_alignment , ap_pagelabel_alignment );

#ifdef AMIDIPLUG_ALSA
  /* ALSA BACKEND CONFIGURATION TAB */
  GtkWidget * alsa_pagelabel_alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
  GtkWidget * alsa_page_alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_alignment_set_padding( GTK_ALIGNMENT(alsa_page_alignment) , 3 , 3 , 8 , 3 );
  i_configure_gui_tab_alsa (alsa_page_alignment, backend_list, configwin);
  i_configure_gui_tablabel_alsa (alsa_pagelabel_alignment, backend_list, configwin);
  gtk_notebook_append_page( GTK_NOTEBOOK(configwin_notebook) ,
                            alsa_page_alignment , alsa_pagelabel_alignment );
#endif /* AMIDIPLUG_ALSA */

#if AMIDIPLUG_FLUIDSYNTH
  /* FLUIDSYNTH BACKEND CONFIGURATION TAB */
  GtkWidget * fsyn_pagelabel_alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
  GtkWidget * fsyn_page_alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_alignment_set_padding( GTK_ALIGNMENT(fsyn_page_alignment) , 3 , 3 , 8 , 3 );
  i_configure_gui_tab_fsyn (fsyn_page_alignment, backend_list, configwin);
  i_configure_gui_tablabel_fsyn (fsyn_pagelabel_alignment, backend_list, configwin);
  gtk_notebook_append_page( GTK_NOTEBOOK(configwin_notebook) ,
                            fsyn_page_alignment , fsyn_pagelabel_alignment );
#endif

  i_backend_list_free( backend_list_h ); /* done, free the list of available backends */

  gtk_widget_show_all( configwin );
}

static void i_configure_cancel (GtkWidget * configwin)
{
  i_configure_cfg_backend_free(); /* free backend settings */
  gtk_widget_destroy(GTK_WIDGET(configwin));
}

static void i_configure_commit (GtkWidget * configwin)
{
  DEBUGMSG( "saving configuration...\n" );
  i_configure_cfg_ap_save(); /* save amidiplug settings */
  i_configure_cfg_backend_save(); /* save backend settings */
  DEBUGMSG( "configuration saved\n" );

  /* check if a different backend has been selected */
  if (( backend.name == NULL ) || ( strcmp( amidiplug_cfg_ap.ap_seq_backend , backend.name ) ))
  {
    DEBUGMSG( "a new backend has been selected, unloading previous and loading the new one\n" );
    i_backend_unload(); /* unload previous backend */
    i_backend_load( amidiplug_cfg_ap.ap_seq_backend ); /* load new backend */
  }
  else /* same backend, just reload updated configuration */
  {
    if ( backend.gmodule != NULL )
    {
      DEBUGMSG( "the selected backend is already loaded, so just perform backend cleanup and reinit\n" );
      backend.cleanup();
      backend.init( i_configure_cfg_get_file );
    }
  }

  i_configure_cfg_backend_free ();
  gtk_widget_destroy (configwin);
}


void i_configure_cfg_backend_alloc( void )
{
  amidiplug_cfg_backend = g_malloc(sizeof(amidiplug_cfg_backend));

#ifdef AMIDIPLUG_ALSA
  i_configure_cfg_alsa_alloc(); /* alloc alsa backend configuration */
#endif
#ifdef AMIDIPLUG_FLUIDSYNTH
  i_configure_cfg_fsyn_alloc(); /* alloc fluidsynth backend configuration */
#endif
}


void i_configure_cfg_backend_free( void )
{
#ifdef AMIDIPLUG_ALSA
  i_configure_cfg_alsa_free(); /* free alsa backend configuration */
#endif
#ifdef AMIDIPLUG_FLUIDSYNTH
  i_configure_cfg_fsyn_free(); /* free fluidsynth backend configuration */
#endif

  g_free( amidiplug_cfg_backend );
}


void i_configure_cfg_backend_read( void )
{
  pcfg_t *cfgfile;
  gchar *config_pathfilename = i_configure_cfg_get_file();

  cfgfile = i_pcfg_new_from_file( config_pathfilename );

#ifdef AMIDIPLUG_ALSA
  i_configure_cfg_alsa_read( cfgfile ); /* get alsa backend configuration */
#endif
#ifdef AMIDIPLUG_FLUIDSYNTH
  i_configure_cfg_fsyn_read( cfgfile ); /* get fluidsynth backend configuration */
#endif

  if ( cfgfile != NULL )
    i_pcfg_free(cfgfile);

  g_free( config_pathfilename );
}


void i_configure_cfg_backend_save( void )
{
  pcfg_t *cfgfile;
  gchar *config_pathfilename = i_configure_cfg_get_file();

  cfgfile = i_pcfg_new_from_file( config_pathfilename );

  if (!cfgfile)
    cfgfile = i_pcfg_new();

#ifdef AMIDIPLUG_ALSA
  i_configure_cfg_alsa_save( cfgfile ); /* save alsa backend configuration */
#endif
#ifdef AMIDIPLUG_FLUIDSYNTH
  i_configure_cfg_fsyn_save( cfgfile ); /* save fluidsynth backend configuration */
#endif

  i_pcfg_write_to_file( cfgfile , config_pathfilename );
  i_pcfg_free( cfgfile );
  g_free( config_pathfilename );
}


/* read only the amidi-plug part of configuration */
void i_configure_cfg_ap_read( void )
{
  pcfg_t *cfgfile;
  gchar *config_pathfilename = i_configure_cfg_get_file();

  cfgfile = i_pcfg_new_from_file( config_pathfilename );

  if (!cfgfile)
  {
    /* amidi-plug defaults */

    amidiplug_cfg_ap.ap_seq_backend = g_strdup (DEFAULT_BACKEND);
    amidiplug_cfg_ap.ap_opts_transpose_value = 0;
    amidiplug_cfg_ap.ap_opts_drumshift_value = 0;
    amidiplug_cfg_ap.ap_opts_length_precalc = 0;
    amidiplug_cfg_ap.ap_opts_lyrics_extract = 0;
    amidiplug_cfg_ap.ap_opts_comments_extract = 0;
  }
  else
  {
    i_pcfg_read_string ( cfgfile , "general" , "ap_seq_backend" ,
                         &amidiplug_cfg_ap.ap_seq_backend, DEFAULT_BACKEND );
    i_pcfg_read_integer( cfgfile , "general" , "ap_opts_transpose_value" ,
                         &amidiplug_cfg_ap.ap_opts_transpose_value , 0 );
    i_pcfg_read_integer( cfgfile , "general" , "ap_opts_drumshift_value" ,
                         &amidiplug_cfg_ap.ap_opts_drumshift_value , 0 );
    i_pcfg_read_integer( cfgfile , "general" , "ap_opts_length_precalc" ,
                         &amidiplug_cfg_ap.ap_opts_length_precalc , 0 );
    i_pcfg_read_integer( cfgfile , "general" , "ap_opts_lyrics_extract" ,
                         &amidiplug_cfg_ap.ap_opts_lyrics_extract , 0 );
    i_pcfg_read_integer( cfgfile , "general" , "ap_opts_comments_extract" ,
                         &amidiplug_cfg_ap.ap_opts_comments_extract , 0 );
    i_pcfg_free( cfgfile );
  }

  g_free( config_pathfilename );
}


void i_configure_cfg_ap_save( void )
{
  pcfg_t *cfgfile;
  gchar *config_pathfilename = i_configure_cfg_get_file();
  cfgfile = i_pcfg_new_from_file( config_pathfilename );

  if (!cfgfile)
    cfgfile = i_pcfg_new();

  /* save amidi-plug config information */
  i_pcfg_write_string( cfgfile , "general" , "ap_seq_backend" ,
                       amidiplug_cfg_ap.ap_seq_backend );
  i_pcfg_write_integer( cfgfile , "general" , "ap_opts_transpose_value" ,
                        amidiplug_cfg_ap.ap_opts_transpose_value );
  i_pcfg_write_integer( cfgfile , "general" , "ap_opts_drumshift_value" ,
                        amidiplug_cfg_ap.ap_opts_drumshift_value );
  i_pcfg_write_integer( cfgfile , "general" , "ap_opts_length_precalc" ,
                        amidiplug_cfg_ap.ap_opts_length_precalc );
  i_pcfg_write_integer( cfgfile , "general" , "ap_opts_lyrics_extract" ,
                        amidiplug_cfg_ap.ap_opts_lyrics_extract );
  i_pcfg_write_integer( cfgfile , "general" , "ap_opts_comments_extract" ,
                        amidiplug_cfg_ap.ap_opts_comments_extract );

  i_pcfg_write_to_file( cfgfile , config_pathfilename );
  i_pcfg_free( cfgfile );
  g_free( config_pathfilename );
}


gchar * i_configure_cfg_get_file( void )
{
  return g_build_filename (aud_get_path (AUD_PATH_USER_DIR), "amidi-plug.conf", NULL);
}
