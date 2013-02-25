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

#include "config.h"
#include "i_configure-alsa.h"

#include <stdlib.h>
#include <string.h>
#include <audacious/i18n.h>
#include <audacious/misc.h>

#include "i_backend.h"
#include "i_common.h"
#include "i_configure.h"

#ifdef AMIDIPLUG_ALSA

#include "backend-alsa/backend-alsa-icon.xpm"

enum
{
  LISTPORT_TOGGLE_COLUMN = 0,
  LISTPORT_PORTNUM_COLUMN,
  LISTPORT_CLIENTNAME_COLUMN,
  LISTPORT_PORTNAME_COLUMN,
  LISTPORT_POINTER_COLUMN,
  LISTPORT_N_COLUMNS
};

enum
{
  LISTCARD_NAME_COLUMN = 0,
  LISTCARD_ID_COLUMN,
  LISTCARD_MIXERPTR_COLUMN,
  LISTCARD_N_COLUMNS
};

enum
{
  LISTMIXER_NAME_COLUMN = 0,
  LISTMIXER_ID_COLUMN,
  LISTMIXER_N_COLUMNS
};

/* GCC does not like casting func * * to void * *. */
static void * get_symbol (GModule * mod, const char * name)
{
    void * sym = NULL;
    g_module_symbol (mod, name, & sym);
    return sym;
}

void i_configure_ev_portlv_changetoggle( GtkCellRendererToggle * rdtoggle ,
                                         char * path_str , void * data )
{
  GtkTreeModel *model = (GtkTreeModel*)data;
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string( path_str );
  bool_t toggled;

  gtk_tree_model_get_iter( model , &iter , path );
  gtk_tree_model_get( model , &iter , LISTPORT_TOGGLE_COLUMN , &toggled , -1 );

  toggled ^= 1;
  gtk_list_store_set( GTK_LIST_STORE(model), &iter , LISTPORT_TOGGLE_COLUMN , toggled , -1 );

  gtk_tree_path_free (path);
}


bool_t i_configure_ev_mixctlcmb_inspect( GtkTreeModel * store , GtkTreePath * path,
                                           GtkTreeIter * iter , void * mixctl_cmb )
{
  int ctl_id;
  char * ctl_name;
  amidiplug_cfg_alsa_t * alsacfg = amidiplug_cfg_backend->alsa;
  gtk_tree_model_get( GTK_TREE_MODEL(store) , iter ,
                      LISTMIXER_ID_COLUMN , &ctl_id ,
                      LISTMIXER_NAME_COLUMN , &ctl_name , -1 );
  if (( !strcmp( ctl_name , alsacfg->alsa_mixer_ctl_name ) ) &&
      ( ctl_id == alsacfg->alsa_mixer_ctl_id ))
  {
    /* this is the selected control in the mixer control combobox */
    gtk_combo_box_set_active_iter( GTK_COMBO_BOX(mixctl_cmb) , iter );
    return TRUE;
  }
  free( ctl_name );
  return FALSE;
}


void i_configure_ev_cardcmb_changed( GtkWidget * card_cmb , void * mixctl_cmb )
{
  GtkTreeIter iter;
  if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX(card_cmb) , &iter ) )
  {
    amidiplug_cfg_alsa_t * alsacfg = amidiplug_cfg_backend->alsa;
    void * mixctl_store;
    int card_id;
    GtkTreeModel * store = gtk_combo_box_get_model( GTK_COMBO_BOX(card_cmb) );
    gtk_tree_model_get( GTK_TREE_MODEL(store) , &iter ,
                        LISTCARD_ID_COLUMN , &card_id ,
                        LISTCARD_MIXERPTR_COLUMN , &mixctl_store , -1 );
    gtk_combo_box_set_model( GTK_COMBO_BOX(mixctl_cmb) , GTK_TREE_MODEL(mixctl_store) );

    /* check if the selected card is the one contained in configuration */
    if ( card_id == alsacfg->alsa_mixer_card_id )
    {
      /* search for the selected mixer control in combo box */
      gtk_tree_model_foreach( GTK_TREE_MODEL(mixctl_store) ,
                              i_configure_ev_mixctlcmb_inspect , mixctl_cmb );
    }
  }
}


bool_t i_configure_ev_portlv_inspecttoggle( GtkTreeModel * model , GtkTreePath * path ,
                                              GtkTreeIter * iter , void * wpp )
{
  bool_t toggled = FALSE;
  char * portstring;
  GString * wps = wpp;
  gtk_tree_model_get ( model , iter ,
                       LISTPORT_TOGGLE_COLUMN , &toggled ,
                       LISTPORT_PORTNUM_COLUMN , &portstring , -1 );
  if ( toggled ) /* check if the row points to an enabled port */
  {
    /* if this is not the first port added to wp, use comma as separator */
    if ( wps->str[0] != '\0' ) g_string_append_c( wps , ',' );
    g_string_append( wps , portstring );
  }
  free( portstring );
  return FALSE;
}


void i_configure_ev_portlv_commit( void * port_lv )
{
  amidiplug_cfg_alsa_t * alsacfg = amidiplug_cfg_backend->alsa;
  GtkTreeModel * store;
  GString *wp = g_string_new( "" );
  /* get the model of the port listview */
  store = gtk_tree_view_get_model( GTK_TREE_VIEW(port_lv) );
  /* after going through this foreach, wp contains a comma-separated list of selected ports */
  gtk_tree_model_foreach( store , i_configure_ev_portlv_inspecttoggle , wp );
  free( alsacfg->alsa_seq_wports ); /* free previous */
  alsacfg->alsa_seq_wports = g_strdup( wp->str ); /* set with new */
  g_string_free( wp , TRUE ); /* not needed anymore */
  return;
}

void i_configure_ev_cardcmb_commit( void * card_cmb )
{
  GtkTreeModel * store;
  GtkTreeIter iter;
  store = gtk_combo_box_get_model( GTK_COMBO_BOX(card_cmb) );
  /* get the selected item */
  if ( gtk_combo_box_get_active_iter( GTK_COMBO_BOX(card_cmb) , &iter ) )
  {
    amidiplug_cfg_alsa_t * alsacfg = amidiplug_cfg_backend->alsa;
    /* update amidiplug_cfg.alsa_mixer_card_id */
    gtk_tree_model_get( GTK_TREE_MODEL(store) , &iter ,
                        LISTCARD_ID_COLUMN ,
                        &alsacfg->alsa_mixer_card_id , -1 );
  }
  return;
}

void i_configure_ev_mixctlcmb_commit( void * mixctl_cmb )
{
  GtkTreeModel * store;
  GtkTreeIter iter;
  store = gtk_combo_box_get_model( GTK_COMBO_BOX(mixctl_cmb) );
  /* get the selected item */
  if ( gtk_combo_box_get_active_iter( GTK_COMBO_BOX(mixctl_cmb) , &iter ) )
  {
    amidiplug_cfg_alsa_t * alsacfg = amidiplug_cfg_backend->alsa;
    free( alsacfg->alsa_mixer_ctl_name ); /* free previous */
    /* update amidiplug_cfg.alsa_mixer_card_id */
    gtk_tree_model_get( GTK_TREE_MODEL(store) , &iter ,
                        LISTMIXER_NAME_COLUMN ,
                        &alsacfg->alsa_mixer_ctl_name ,
                        LISTMIXER_ID_COLUMN ,
                        &alsacfg->alsa_mixer_ctl_id , -1 );
  }
  return;
}


void i_configure_gui_ctlcmb_datafunc( GtkCellLayout *cell_layout , GtkCellRenderer *cr ,
                                      GtkTreeModel *store , GtkTreeIter *iter , void * p )
{
  char *ctl_display, *ctl_name;
  int ctl_id;
  gtk_tree_model_get( store , iter ,
                      LISTMIXER_NAME_COLUMN , &ctl_name ,
                      LISTMIXER_ID_COLUMN , &ctl_id , -1 );
  if ( ctl_id == 0 )
    ctl_display = g_strdup_printf( "%s" , ctl_name );
  else
    ctl_display = g_strdup_printf( "%s (%i)" , ctl_name , ctl_id );
  g_object_set( G_OBJECT(cr) , "text" , ctl_display , NULL );
  free( ctl_display );
  free( ctl_name );
}


void i_configure_gui_tab_alsa( GtkWidget * alsa_page_alignment ,
                               void * backend_list_p ,
                               void * commit_button )
{
  GSList * backend_list = backend_list_p;
  char * alsa_module_pathfilename = NULL;

  GtkWidget * content_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

  /* check if the ALSA module is available */
  while ( backend_list != NULL )
  {
    amidiplug_sequencer_backend_name_t * mn = backend_list->data;
    if ( !strcmp( mn->name , "alsa" ) )
    {
      alsa_module_pathfilename = mn->filename;
      break;
    }
    backend_list = backend_list->next;
  }

  if (alsa_module_pathfilename)
  {
    GtkListStore *port_store, *mixer_card_store;
    GtkWidget *port_lv, *port_lv_sw, *port_lv_frame;
    GtkCellRenderer *port_lv_toggle_rndr, *port_lv_text_rndr;
    GtkTreeViewColumn *port_lv_toggle_col, *port_lv_portnum_col;
    GtkTreeViewColumn *port_lv_clientname_col, *port_lv_portname_col;
    GtkTreeSelection *port_lv_sel;
    GtkTreeIter iter;
    GtkWidget *mixer_grid, *mixer_frame;
    GtkCellRenderer *mixer_card_cmb_text_rndr, *mixer_ctl_cmb_text_rndr;
    GtkWidget *mixer_card_cmb_evbox, *mixer_card_cmb, *mixer_card_label;
    GtkWidget *mixer_ctl_cmb_evbox, *mixer_ctl_cmb, *mixer_ctl_label;

    amidiplug_cfg_alsa_t * alsacfg = amidiplug_cfg_backend->alsa;

    char **portstring_from_cfg = NULL;

    GModule * alsa_module;
    GSList *wports = NULL, *wports_h = NULL, *scards = NULL, *scards_h = NULL;
    GSList * (*get_port_list)( void );
    void (*free_port_list)( GSList * );
    GSList * (*get_card_list)( void );
    void (*free_card_list)( GSList * );

    if ( strlen( alsacfg->alsa_seq_wports ) > 0 )
      portstring_from_cfg = g_strsplit( alsacfg->alsa_seq_wports , "," , 0 );

    /* it's legit to assume that this can't fail,
       since the module is present in the backend_list */
    alsa_module = g_module_open( alsa_module_pathfilename , 0 );

    get_port_list = get_symbol (alsa_module, "sequencer_port_get_list");
    free_port_list = get_symbol (alsa_module, "sequencer_port_free_list");
    get_card_list = get_symbol (alsa_module, "alsa_card_get_list");
    free_card_list = get_symbol (alsa_module, "alsa_card_free_list");

    /* get an updated list of writable ALSA MIDI ports and ALSA-enabled sound cards*/
    wports = get_port_list(); wports_h = wports;
    scards = get_card_list(); scards_h = scards;

    /* ALSA MIDI PORT LISTVIEW */
    port_store = gtk_list_store_new( LISTPORT_N_COLUMNS, G_TYPE_BOOLEAN, G_TYPE_STRING ,
                                     G_TYPE_STRING , G_TYPE_STRING , G_TYPE_POINTER );
    while ( wports != NULL )
    {
      bool_t toggled = FALSE;
      data_bucket_t * portinfo = (data_bucket_t *)wports->data;
      GString * portstring = g_string_new("");
      g_string_printf( portstring , "%i:%i" , portinfo->bint[0] , portinfo->bint[1] );
      gtk_list_store_append( port_store , &iter );
      /* in the existing configuration there may be previously selected ports */
      if ( portstring_from_cfg != NULL )
      {
        int i = 0;
        /* check if current row contains a port selected by user */
        for ( i = 0 ; portstring_from_cfg[i] != NULL ; i++ )
        {
          /* if it's one of the selected ports, toggle its checkbox */
          if ( !strcmp( portstring->str, portstring_from_cfg[i] ) )
            toggled = TRUE;
        }
      }
      gtk_list_store_set( port_store , &iter ,
                          LISTPORT_TOGGLE_COLUMN , toggled ,
                          LISTPORT_PORTNUM_COLUMN , portstring->str ,
                          LISTPORT_CLIENTNAME_COLUMN , portinfo->bcharp[0] ,
                          LISTPORT_PORTNAME_COLUMN , portinfo->bcharp[1] ,
                          LISTPORT_POINTER_COLUMN , portinfo , -1 );
      g_string_free( portstring , TRUE );
      /* on with next */
      wports = wports->next;
    }
    g_strfreev( portstring_from_cfg ); /* not needed anymore */
    port_lv = gtk_tree_view_new_with_model( GTK_TREE_MODEL(port_store) );
    gtk_tree_view_set_rules_hint( GTK_TREE_VIEW(port_lv), TRUE );
    g_object_unref( port_store );
    port_lv_toggle_rndr = gtk_cell_renderer_toggle_new();
    gtk_cell_renderer_toggle_set_radio( GTK_CELL_RENDERER_TOGGLE(port_lv_toggle_rndr) , FALSE );
    gtk_cell_renderer_toggle_set_active( GTK_CELL_RENDERER_TOGGLE(port_lv_toggle_rndr) , TRUE );
    g_signal_connect( port_lv_toggle_rndr , "toggled" ,
                      G_CALLBACK(i_configure_ev_portlv_changetoggle) , port_store );
    port_lv_text_rndr = gtk_cell_renderer_text_new();
    port_lv_toggle_col = gtk_tree_view_column_new_with_attributes( "", port_lv_toggle_rndr,
                                                                   "active", LISTPORT_TOGGLE_COLUMN, NULL );
    port_lv_portnum_col = gtk_tree_view_column_new_with_attributes( _("Port"), port_lv_text_rndr,
                                                                    "text", LISTPORT_PORTNUM_COLUMN, NULL );
    port_lv_clientname_col = gtk_tree_view_column_new_with_attributes( _("Client name"), port_lv_text_rndr,
                                                                       "text", LISTPORT_CLIENTNAME_COLUMN, NULL );
    port_lv_portname_col = gtk_tree_view_column_new_with_attributes( _("Port name"), port_lv_text_rndr,
                                                                     "text", LISTPORT_PORTNAME_COLUMN, NULL );
    gtk_tree_view_append_column( GTK_TREE_VIEW(port_lv), port_lv_toggle_col );
    gtk_tree_view_append_column( GTK_TREE_VIEW(port_lv), port_lv_portnum_col );
    gtk_tree_view_append_column( GTK_TREE_VIEW(port_lv), port_lv_clientname_col );
    gtk_tree_view_append_column( GTK_TREE_VIEW(port_lv), port_lv_portname_col );
    port_lv_sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(port_lv) );
    gtk_tree_selection_set_mode( GTK_TREE_SELECTION(port_lv_sel) , GTK_SELECTION_NONE );

    port_lv_sw = gtk_scrolled_window_new( NULL , NULL );
    gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) port_lv_sw, GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy ((GtkScrolledWindow *) port_lv_sw,
     GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    port_lv_frame = gtk_frame_new( _("ALSA output ports") );
    gtk_container_add( GTK_CONTAINER(port_lv_sw) , port_lv );
    gtk_container_add( GTK_CONTAINER(port_lv_frame) , port_lv_sw );
    gtk_box_pack_start( GTK_BOX(content_vbox) , port_lv_frame , TRUE , TRUE , 0 );
    g_signal_connect_swapped( G_OBJECT(commit_button) , "ap-commit" ,
                              G_CALLBACK(i_configure_ev_portlv_commit) , port_lv );

    /* MIXER CARD/CONTROL COMBOBOXES */
    mixer_card_store = gtk_list_store_new( LISTCARD_N_COLUMNS , G_TYPE_STRING , G_TYPE_INT , G_TYPE_POINTER );
    mixer_card_cmb = gtk_combo_box_new_with_model( GTK_TREE_MODEL(mixer_card_store) ); /* soundcard combo box */
    mixer_ctl_cmb = gtk_combo_box_new(); /* mixer control combo box */
    g_signal_connect( mixer_card_cmb , "changed" , G_CALLBACK(i_configure_ev_cardcmb_changed) , mixer_ctl_cmb );
    while ( scards != NULL )
    {
      GtkListStore *mixer_ctl_store;
      GtkTreeIter itermix;
      data_bucket_t * cardinfo = (data_bucket_t *)scards->data;
      GSList *mixctl_list = cardinfo->bpointer[0];
      mixer_ctl_store = gtk_list_store_new( LISTMIXER_N_COLUMNS , G_TYPE_STRING , G_TYPE_INT );
      while ( mixctl_list != NULL )
      {
        data_bucket_t * mixctlinfo = (data_bucket_t *)mixctl_list->data;
        gtk_list_store_append( mixer_ctl_store , &itermix );
        gtk_list_store_set( mixer_ctl_store , &itermix ,
                            LISTMIXER_NAME_COLUMN , mixctlinfo->bcharp[0] ,
                            LISTMIXER_ID_COLUMN , mixctlinfo->bint[0] , -1 );
        mixctl_list = mixctl_list->next; /* on with next mixer control */
      }
      gtk_list_store_append( mixer_card_store , &iter );
      gtk_list_store_set( mixer_card_store , &iter ,
                          LISTCARD_NAME_COLUMN , cardinfo->bcharp[0] ,
                          LISTCARD_ID_COLUMN , cardinfo->bint[0] ,
                          LISTCARD_MIXERPTR_COLUMN , mixer_ctl_store , -1 );
      /* check if this corresponds to the value previously selected by user */
      if ( cardinfo->bint[0] == alsacfg->alsa_mixer_card_id )
        gtk_combo_box_set_active_iter( GTK_COMBO_BOX(mixer_card_cmb) , &iter );
      scards = scards->next; /* on with next card */
    }
    g_object_unref( mixer_card_store ); /* free a reference, no longer needed */
    /* create renderer to display text in the mixer combo box */
    mixer_card_cmb_text_rndr = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start( GTK_CELL_LAYOUT(mixer_card_cmb) , mixer_card_cmb_text_rndr , TRUE );
    gtk_cell_layout_add_attribute( GTK_CELL_LAYOUT(mixer_card_cmb) , mixer_card_cmb_text_rndr ,
                                   "text" , LISTCARD_NAME_COLUMN );
    mixer_ctl_cmb_text_rndr = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start( GTK_CELL_LAYOUT(mixer_ctl_cmb) , mixer_ctl_cmb_text_rndr , TRUE );
    gtk_cell_layout_set_cell_data_func( GTK_CELL_LAYOUT(mixer_ctl_cmb) , mixer_ctl_cmb_text_rndr ,
                                        i_configure_gui_ctlcmb_datafunc , NULL , NULL );
    /* the event box is needed to display a tooltip for the mixer combo box */
    mixer_card_cmb_evbox = gtk_event_box_new();
    gtk_widget_set_hexpand( mixer_card_cmb_evbox , TRUE );
    gtk_container_add( GTK_CONTAINER(mixer_card_cmb_evbox) , mixer_card_cmb );
    mixer_ctl_cmb_evbox = gtk_event_box_new();
    gtk_widget_set_hexpand( mixer_ctl_cmb_evbox , TRUE );
    gtk_container_add( GTK_CONTAINER(mixer_ctl_cmb_evbox) , mixer_ctl_cmb );
    mixer_card_label = gtk_label_new( _("Soundcard: ") );
    gtk_misc_set_alignment( GTK_MISC(mixer_card_label) , 0 , 0.5 );
    mixer_ctl_label = gtk_label_new( _("Mixer control: ") );
    gtk_misc_set_alignment( GTK_MISC(mixer_ctl_label) , 0 , 0.5 );
    mixer_grid = gtk_grid_new();
    gtk_grid_set_row_spacing( GTK_GRID(mixer_grid) , 4 );
    gtk_grid_set_column_spacing( GTK_GRID(mixer_grid) , 2 );
    gtk_container_set_border_width( GTK_CONTAINER(mixer_grid) , 5 );
    gtk_grid_attach( GTK_GRID(mixer_grid) , mixer_card_label , 0 , 0 , 1 , 1 );
    gtk_grid_attach( GTK_GRID(mixer_grid) , mixer_card_cmb_evbox , 1 , 0 , 1 , 1 );
    gtk_grid_attach( GTK_GRID(mixer_grid) , mixer_ctl_label , 0 , 1 , 1 , 1 );
    gtk_grid_attach( GTK_GRID(mixer_grid) , mixer_ctl_cmb_evbox , 1 , 1 , 1 , 1 );
    mixer_frame = gtk_frame_new( _("Mixer settings") );
    gtk_container_add( GTK_CONTAINER(mixer_frame) , mixer_grid );
    gtk_box_pack_start( GTK_BOX(content_vbox) , mixer_frame , TRUE , TRUE , 0 );
    g_signal_connect_swapped( G_OBJECT(commit_button) , "ap-commit" ,
                              G_CALLBACK(i_configure_ev_cardcmb_commit) , mixer_card_cmb );
    g_signal_connect_swapped( G_OBJECT(commit_button) , "ap-commit" ,
                              G_CALLBACK(i_configure_ev_mixctlcmb_commit) , mixer_ctl_cmb );

    free_card_list( scards_h );
    free_port_list( wports_h );
    g_module_close( alsa_module );
  }

  gtk_container_add ((GtkContainer *) alsa_page_alignment, content_vbox);
}


void i_configure_gui_tablabel_alsa( GtkWidget * alsa_page_alignment ,
                                    void * backend_list_p ,
                                    void * commit_button )
{
  GtkWidget *pagelabel_vbox, *pagelabel_image, *pagelabel_label;
  GdkPixbuf *pagelabel_image_pix;
  pagelabel_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1 );
  pagelabel_image_pix = gdk_pixbuf_new_from_xpm_data( (const char **)backend_alsa_icon_xpm );
  pagelabel_image = gtk_image_new_from_pixbuf( pagelabel_image_pix ); g_object_unref( pagelabel_image_pix );
  pagelabel_label = gtk_label_new( "" );
  gtk_label_set_markup( GTK_LABEL(pagelabel_label) , _("<span size=\"smaller\">ALSA\nbackend</span>") );
  gtk_label_set_justify( GTK_LABEL(pagelabel_label) , GTK_JUSTIFY_CENTER );
  gtk_box_pack_start( GTK_BOX(pagelabel_vbox) , pagelabel_image , FALSE , FALSE , 1 );
  gtk_box_pack_start( GTK_BOX(pagelabel_vbox) , pagelabel_label , FALSE , FALSE , 1 );
  gtk_container_add( GTK_CONTAINER(alsa_page_alignment) , pagelabel_vbox );
  gtk_widget_show_all( alsa_page_alignment );
  return;
}


char * i_configure_read_seq_ports_default( void )
{
  FILE * fp = NULL;
  /* first try, get seq ports from proc on card0 */
  fp = fopen( "/proc/asound/card0/wavetableD1" , "rb" );
  if ( fp )
  {
    char buffer[100];
    while ( !feof( fp ) )
    {
      if(!fgets( buffer , 100 , fp ))
        break;
      if (( strlen( buffer ) > 11 ) && ( !strncasecmp( buffer , "addresses: " , 11 ) ))
      {
        /* change spaces between ports (65:0 65:1 65:2 ...)
           into commas (65:0,65:1,65:2,...) */
        g_strdelimit( &buffer[11] , " " , ',' );
        /* remove lf and cr from the end of the string */
        g_strdelimit( &buffer[11] , "\r\n" , '\0' );
        /* ready to go */
        DEBUGMSG( "init, default values for seq ports detected: %s\n" , &buffer[11] );
        fclose( fp );
        return g_strdup( &buffer[11] );
      }
    }
    fclose( fp );
  }

  /* second option: do not set ports at all, let the user
     select the right ones in the nice config window :) */
  return g_strdup( "" );
}



void i_configure_cfg_alsa_free( void )
{
  amidiplug_cfg_alsa_t * alsacfg = amidiplug_cfg_backend->alsa;
  free( alsacfg->alsa_seq_wports );
  free( alsacfg->alsa_mixer_ctl_name );
  free( amidiplug_cfg_backend->alsa );
}


void i_configure_cfg_alsa_read (void)
{
  static const char * const defaults[] = {
    "alsa_mixer_card_id", "0",
    "alsa_mixer_ctl_name", "Synth",
    "alsa_mixer_ctl_id", "0",
    NULL};

  aud_config_set_defaults ("amidiplug", defaults);

  amidiplug_cfg_alsa_t * alsacfg = malloc (sizeof (amidiplug_cfg_alsa_t));
  amidiplug_cfg_backend->alsa = alsacfg;

  alsacfg->alsa_seq_wports = aud_get_string ("amidiplug", "alsa_seq_wports");
  alsacfg->alsa_mixer_card_id = aud_get_int ("amidiplug", "alsa_mixer_card_id");
  alsacfg->alsa_mixer_ctl_name = aud_get_string ("amidiplug", "alsa_mixer_ctl_name");
  alsacfg->alsa_mixer_ctl_id = aud_get_int ("amidiplug", "alsa_mixer_ctl_id");

  if (! alsacfg->alsa_seq_wports[0])
  {
    free (alsacfg->alsa_seq_wports);
    alsacfg->alsa_seq_wports = i_configure_read_seq_ports_default ();
  }
}


void i_configure_cfg_alsa_save (void)
{
  amidiplug_cfg_alsa_t * alsacfg = amidiplug_cfg_backend->alsa;
  aud_set_string ("amidiplug", "alsa_seq_wports", alsacfg->alsa_seq_wports);
  aud_set_int ("amidiplug", "alsa_mixer_card_id", alsacfg->alsa_mixer_card_id);
  aud_set_string ("amidiplug", "alsa_mixer_ctl_name", alsacfg->alsa_mixer_ctl_name);
  aud_set_int ("amidiplug", "alsa_mixer_ctl_id", alsacfg->alsa_mixer_ctl_id);
}

#endif /* AMIDIPLUG_ALSA */
