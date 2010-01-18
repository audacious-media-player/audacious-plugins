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

#include "ed_ui.h"
#include "ed_types.h"
#include "ed_internals.h"
#include "ed_actions.h"
#include "ed_bindings_store.h"
#include "ed_common.h"
#include <stdlib.h>
#include <string.h>
#include <linux/input.h>

#include <audacious/i18n.h>
#include <gtk/gtk.h>


static void cfg_ui_bindings_show ( ed_device_t * , GtkTreeRowReference * );

extern ed_action_t player_actions[];
extern gboolean plugin_is_active;

static GtkWidget *cfg_win = NULL;

enum
{
  DEVLIST_COL_ISACTIVE = 0,
  DEVLIST_COL_NAME,
  DEVLIST_COL_FILENAME,
  DEVLIST_COL_PHYS,
  DEVLIST_COL_ISAVAILABLE,
  DEVLIST_COL_BINDINGS,
  DEVLIST_NUMCOLS
};

enum
{
  DEVLIST_ISAVAILABLE_NOTDET = 0,
  DEVLIST_ISAVAILABLE_DET,
  DEVLIST_ISAVAILABLE_CUSTOM
};



static void
cfg_device_lv_populate( GtkListStore * store )
{
  /* store is logically divided in three families of devices,
     depending on the ISAVAILABLE value;
     DEVLIST_ISAVAILABLE_NOTDET -> not detected devices: these are not-custom
         devices that are present in configuration file but they do not seem
         to be plugged at the moment, cause they're not in system_devices_list;
     DEVLIST_ISAVAILABLE_DET -> detected devices: these are not-custom devices that
         are currently plugged, no matter whether they are in configuration or not;
     DEVLIST_ISAVAILABLE_CUSTOM -> custom devices: defined by user in configuration
         file, these are not checked against system_devices_list;
  */

  GList *system_devices_list = NULL;
  GList *config_devices_list = NULL;
  GList *list_iter = NULL;

  system_devices_list = ed_device_get_list_from_system();
  config_devices_list = ed_device_get_list_from_config();

  /* first, for each not-custom device in config_devices_list,
     perform a search in system_devices_list */
  list_iter = config_devices_list;
  while ( list_iter != NULL )
  {
    ed_device_info_t *info_from_cfg = list_iter->data;
    if ( info_from_cfg->is_custom == 0 ) /* only try to detect not-custom devices */
    {
      /* note: if device is found, filename and phys in info_from_cfg could be updated by the
               check; the found device will be marked as "found" in system_devices_list too */
      if ( ed_device_check( system_devices_list ,
           info_from_cfg->name , &(info_from_cfg->filename) , &(info_from_cfg->phys) ) == ED_DEVCHECK_OK )
      {
        info_from_cfg->reg = 1; /* mark our device from config as "found" */
      }
    }
    list_iter = g_list_next( list_iter );
  }

  /* at this point, we have three logical groups:
     #1 - not-custom devices in config_devices_list, marked as "found" -> DEVLIST_ISAVAILABLE_DET
     #2 - not-custom devices in config_devices_list, not-marked -> DEVLIST_ISAVAILABLE_NOTDET
     #3 - custom devices in config_devices_list -> DEVLIST_ISAVAILABLE_CUSTOM
     #1bis - plus, we pick not-marked devices from system_devices_list, which
             are plugged devices that are not present in config_devices_list -> DEVLIST_ISAVAILABLE_DET
  */

  /* let's start to fill the store with items from group #1 (DEVLIST_ISAVAILABLE_DET) */
  list_iter = config_devices_list;
  while ( list_iter != NULL )
  {
    ed_device_info_t *info_from_cfg = list_iter->data;
    if (( info_from_cfg->reg == 1 ) && ( info_from_cfg->is_custom == 0 ))
    {
      GtkTreeIter iter;
      gtk_list_store_append( store , &iter );
      gtk_list_store_set( store , &iter ,
        DEVLIST_COL_ISACTIVE , info_from_cfg->is_active ,
        DEVLIST_COL_NAME , info_from_cfg->name ,
        DEVLIST_COL_FILENAME , info_from_cfg->filename ,
        DEVLIST_COL_PHYS , info_from_cfg->phys ,
        DEVLIST_COL_ISAVAILABLE , DEVLIST_ISAVAILABLE_DET ,
        DEVLIST_COL_BINDINGS , info_from_cfg->bindings ,
        -1 );
    }
    list_iter = g_list_next( list_iter );
  }

  /* add items from group #1bis (DEVLIST_ISAVAILABLE_DET) */
  list_iter = system_devices_list;
  while ( list_iter != NULL )
  {
    ed_device_info_t *info_from_sys = list_iter->data;
    if ( info_from_sys->reg == 0 )
    {
      GtkTreeIter iter;
      gtk_list_store_append( store , &iter );
      gtk_list_store_set( store , &iter ,
        DEVLIST_COL_ISACTIVE , info_from_sys->is_active ,
        DEVLIST_COL_NAME , info_from_sys->name ,
        DEVLIST_COL_FILENAME , info_from_sys->filename ,
        DEVLIST_COL_PHYS , info_from_sys->phys ,
        DEVLIST_COL_ISAVAILABLE , DEVLIST_ISAVAILABLE_DET ,
        DEVLIST_COL_BINDINGS , info_from_sys->bindings ,
        -1 );
    }
    list_iter = g_list_next( list_iter );
  }

  /* add items from group #2 (DEVLIST_ISAVAILABLE_NOTDET) */
  list_iter = config_devices_list;
  while ( list_iter != NULL )
  {
    ed_device_info_t *info_from_cfg = list_iter->data;
    if (( info_from_cfg->reg == 0 ) && ( info_from_cfg->is_custom == 0 ))
    {
      GtkTreeIter iter;
      gtk_list_store_append( store , &iter );
      gtk_list_store_set( store , &iter ,
        DEVLIST_COL_ISACTIVE , info_from_cfg->is_active ,
        DEVLIST_COL_NAME , info_from_cfg->name ,
        DEVLIST_COL_FILENAME , info_from_cfg->filename ,
        DEVLIST_COL_PHYS , info_from_cfg->phys ,
        DEVLIST_COL_ISAVAILABLE , DEVLIST_ISAVAILABLE_NOTDET ,
        DEVLIST_COL_BINDINGS , info_from_cfg->bindings ,
        -1 );
    }
    list_iter = g_list_next( list_iter );
  }

  /* add items from group #3 (DEVLIST_ISAVAILABLE_CUSTOM) */
  list_iter = config_devices_list;
  while ( list_iter != NULL )
  {
    ed_device_info_t *info_from_cfg = list_iter->data;
    if ( info_from_cfg->is_custom == 1 )
    {
      GtkTreeIter iter;
      gtk_list_store_append( store , &iter );
      gtk_list_store_set( store , &iter ,
        DEVLIST_COL_ISACTIVE , info_from_cfg->is_active ,
        DEVLIST_COL_NAME , info_from_cfg->name ,
        DEVLIST_COL_FILENAME , info_from_cfg->filename ,
        DEVLIST_COL_PHYS , info_from_cfg->phys ,
        DEVLIST_COL_ISAVAILABLE , DEVLIST_ISAVAILABLE_CUSTOM ,
        DEVLIST_COL_BINDINGS , info_from_cfg->bindings ,
        -1 );
    }
    list_iter = g_list_next( list_iter );
  }

  /* delete lists; bindings objects are not deleted,
     they must be accessible from liststore for customization
     and they'll be deleted when the config win is destroyed */
  ed_device_free_list( config_devices_list );
  ed_device_free_list( system_devices_list );
  return;
}


static void
cfg_device_lv_celldatafunc_isavailable( GtkTreeViewColumn * col , GtkCellRenderer * renderer ,
                                        GtkTreeModel * model , GtkTreeIter * iter , gpointer data )
{
  guint is_available = 0;
  gtk_tree_model_get( model , iter , DEVLIST_COL_ISAVAILABLE , &is_available , -1 );
  switch (is_available)
  {
    case DEVLIST_ISAVAILABLE_DET:
      g_object_set( renderer , "text" , _("Detected") ,
        "foreground" , "Green" , "foreground-set" , TRUE ,
        "background" , "Black" , "background-set" , TRUE , NULL );
      break;
    case DEVLIST_ISAVAILABLE_CUSTOM:
      g_object_set( renderer , "text" , _("Custom") ,
        "foreground" , "Yellow" , "foreground-set" , TRUE ,
        "background" , "Black" , "background-set" , TRUE , NULL );
      break;
    case DEVLIST_ISAVAILABLE_NOTDET:
    default:
      g_object_set( renderer , "text" , _("Not Detected") ,
        "foreground" , "Orange" , "foreground-set" , TRUE ,
        "background" , "Black" , "background-set" , TRUE , NULL );
      break;
  }
  return;
}


static void
cfg_device_lv_changetoggle ( GtkCellRendererToggle * toggle ,
                             gchar * path_str , gpointer cfg_device_lv )
{
  GtkTreeModel *model = gtk_tree_view_get_model( GTK_TREE_VIEW(cfg_device_lv) );
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string( path_str );
  gboolean toggled;

  gtk_tree_model_get_iter( model , &iter , path );
  gtk_tree_model_get( model , &iter , DEVLIST_COL_ISACTIVE , &toggled , -1 );

  toggled ^= 1;
  gtk_list_store_set( GTK_LIST_STORE(model), &iter , DEVLIST_COL_ISACTIVE , toggled , -1 );

  gtk_tree_path_free( path );
  return;
}


static void
cfg_config_cb_bindings_show ( gpointer cfg_device_lv )
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter;

  /* check if a row is selected */
  sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(cfg_device_lv) );
  if ( gtk_tree_selection_get_selected( sel , &model , &iter ) == TRUE )
  {
    /* check availability and active status */
    guint is_available = 0;
    gboolean is_active = FALSE;

    gtk_tree_model_get( model , &iter ,
      DEVLIST_COL_ISACTIVE , &is_active ,
      DEVLIST_COL_ISAVAILABLE , &is_available ,
      -1 );

    if ( is_available == DEVLIST_ISAVAILABLE_NOTDET )
    {
      /* do not open bindings window for a not-detected device */
      ed_ui_message_show( _("Information") ,
                          _("Cannot open bindings window for a not-detected device.\n"
                            "Ensure that the device has been correctly plugged in.") ,
                          cfg_win );
      return;
    }
    else
    {
      /* ok, create a ed_device_t item from selected row information */
      ed_device_t *dev;
      gchar *device_name = NULL, *device_file = NULL, *device_phys = NULL;
      gpointer bindings = NULL;

      gtk_tree_model_get( model , &iter ,
        DEVLIST_COL_NAME , &device_name ,
        DEVLIST_COL_FILENAME , &device_file ,
        DEVLIST_COL_PHYS , &device_phys ,
        DEVLIST_COL_BINDINGS , &bindings ,
        -1 );

      dev = ed_device_new(
               device_name , device_file , device_phys ,
               ( is_available == DEVLIST_ISAVAILABLE_CUSTOM ? 1 : 0 ) );
      g_free( device_name ); g_free( device_file ); g_free( device_phys );

      if ( dev != NULL )
      {
        GtkTreePath *path;
        GtkTreeRowReference *rowref;

        dev->info->bindings = bindings;

        path = gtk_tree_model_get_path( model , &iter );
        rowref = gtk_tree_row_reference_new( model , path );
        gtk_tree_path_free( path );
        /* show bindings window for device "dev"; also pass to the bindings window
           the GtkTreeRowReference (it keeps track of the model too) for selected row;
           if changes are committed in the bindings window, the row reference will be
           needed to update the treemodel with the new bindings information */
        cfg_ui_bindings_show( dev , rowref );
      }
      else
      {
        /* something went wrong */
        ed_ui_message_show( _("Error") ,
                            _("Unable to open selected device.\n"
                            "Please check read permissions on device file.") ,
                            cfg_win );
      }
    }
  }
}


static void
cfg_config_cb_addcustom_show ( gpointer cfg_device_lv )
{
  GtkWidget *addc_dlg;
  GtkWidget *addc_data_frame, *addc_data_vbox;
  GtkWidget *addc_data_label;
  GtkWidget *addc_data_table;
  GtkWidget *addc_data_name_label, *addc_data_name_entry;
  GtkWidget *addc_data_file_label, *addc_data_file_entry;
  gint result;
  gboolean task_done = FALSE;

  addc_dlg = gtk_dialog_new_with_buttons( _("EvDev-Plug - Add custom device") ,
               GTK_WINDOW(cfg_win) , GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT ,
               GTK_STOCK_CANCEL , GTK_RESPONSE_REJECT , GTK_STOCK_OK , GTK_RESPONSE_ACCEPT , NULL );

  addc_data_frame = gtk_frame_new( NULL );
  gtk_container_add( GTK_CONTAINER(GTK_DIALOG(addc_dlg)->vbox) , addc_data_frame );
  addc_data_vbox = gtk_vbox_new( FALSE , 5 );
  gtk_container_set_border_width( GTK_CONTAINER(addc_data_vbox) , 5 );
  gtk_container_add( GTK_CONTAINER(addc_data_frame) , addc_data_vbox );

  addc_data_label = gtk_label_new( "" );
  gtk_label_set_text( GTK_LABEL(addc_data_label) ,
                      _("EvDev-Plug tries to automatically detect and update information about\n"
                        "event devices available on the system.\n"
                        "However, if auto-detect doesn't work for your system, or you have event\n"
                        "devices in a non-standard location (currently they're only searched in\n"
                        "/dev/input/ ), you may want to add a custom device, explicitly specifying\n"
                        "name and device file.") );
  gtk_box_pack_start( GTK_BOX(addc_data_vbox) , addc_data_label , FALSE , FALSE , 0 );

  addc_data_name_label = gtk_label_new( _("Device name:") );
  gtk_misc_set_alignment( GTK_MISC(addc_data_name_label) , 0 , 0.5 );
  addc_data_name_entry = gtk_entry_new();

  addc_data_file_label = gtk_label_new( _("Device file:") );
  gtk_misc_set_alignment( GTK_MISC(addc_data_file_label) , 0 , 0.5 );
  addc_data_file_entry = gtk_entry_new();

  addc_data_table = gtk_table_new( 2 , 2 , FALSE );
  gtk_table_set_col_spacings( GTK_TABLE(addc_data_table) , 2 );
  gtk_table_set_row_spacings( GTK_TABLE(addc_data_table) , 2 );
  gtk_table_attach( GTK_TABLE(addc_data_table) , addc_data_name_label , 0 , 1 , 0 , 1 ,
                    GTK_FILL , GTK_EXPAND | GTK_FILL , 0 , 0 );
  gtk_table_attach( GTK_TABLE(addc_data_table) , addc_data_name_entry , 1 , 2 , 0 , 1 ,
                    GTK_EXPAND | GTK_FILL , GTK_EXPAND | GTK_FILL , 0 , 0 );
  gtk_table_attach( GTK_TABLE(addc_data_table) , addc_data_file_label , 0 , 1 , 1 , 2 ,
                    GTK_FILL , GTK_EXPAND | GTK_FILL , 0 , 0 );
  gtk_table_attach( GTK_TABLE(addc_data_table) , addc_data_file_entry , 1 , 2 , 1 , 2 ,
                    GTK_EXPAND | GTK_FILL , GTK_EXPAND | GTK_FILL , 0 , 0 );
  gtk_box_pack_start( GTK_BOX(addc_data_vbox) , addc_data_table , TRUE , TRUE , 0 );

  gtk_widget_show_all( addc_dlg );

  while ( task_done == FALSE )
  {
    result = gtk_dialog_run( GTK_DIALOG(addc_dlg) );
    if ( result == GTK_RESPONSE_ACCEPT )
    {
      const gchar *name = gtk_entry_get_text( GTK_ENTRY(addc_data_name_entry) );
      const gchar *file = gtk_entry_get_text( GTK_ENTRY(addc_data_file_entry) );
      if ( ( strcmp( name , "" ) != 0 ) &&
           ( strcmp( name , "___plugin___" ) != 0 ) &&
           ( strcmp( file , "" ) != 0 ) &&
           ( file[0] == '/' ) )
      {
        GtkTreeModel *model = gtk_tree_view_get_model( GTK_TREE_VIEW(cfg_device_lv) );
        GtkTreeIter iter;

        gtk_list_store_append( GTK_LIST_STORE(model) , &iter );
        gtk_list_store_set( GTK_LIST_STORE(model) , &iter ,
          DEVLIST_COL_ISACTIVE , FALSE ,
          DEVLIST_COL_NAME , name ,
          DEVLIST_COL_FILENAME , file ,
          DEVLIST_COL_PHYS , _("(custom)") ,
          DEVLIST_COL_ISAVAILABLE , DEVLIST_ISAVAILABLE_CUSTOM ,
          DEVLIST_COL_BINDINGS , NULL , -1 );
        task_done = TRUE;
      }
      else
      {
        ed_ui_message_show( _("Information") ,
                            _("Please specify both name and filename.\n"
                              "Filename must be specified with absolute path.") ,
                            addc_dlg );
      }
    }
    else
      task_done = TRUE;
  }

  gtk_widget_destroy( addc_dlg );
}


static void
cfg_config_cb_remove ( gpointer cfg_device_lv )
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter;

  /* check if a row is selected */
  sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(cfg_device_lv) );
  if ( gtk_tree_selection_get_selected( sel , &model , &iter ) == TRUE )
  {
    guint is_available = 0;
    gtk_tree_model_get( model , &iter , DEVLIST_COL_ISAVAILABLE , &is_available , -1 );
    switch ( is_available )
    {
      case DEVLIST_ISAVAILABLE_NOTDET:
      {
        /* if remove is called on a not-detected device,
           it allows to remove existing configuration for it */
        GtkWidget *yesno_dlg = gtk_message_dialog_new( GTK_WINDOW(cfg_win) ,
          GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT , GTK_MESSAGE_QUESTION ,
          GTK_BUTTONS_YES_NO ,
          _("Do you want to remove the existing configuration for selected device?\n") );
        if ( gtk_dialog_run( GTK_DIALOG(yesno_dlg) ) == GTK_RESPONSE_YES )
        {
          gpointer bindings = NULL;
          gtk_tree_model_get( model , &iter , DEVLIST_COL_BINDINGS , &bindings , -1 );
          if ( bindings != NULL ) ed_bindings_store_delete( bindings );
          gtk_list_store_remove( GTK_LIST_STORE(model) , &iter );
        }
        gtk_widget_destroy( yesno_dlg );
        break;
      }

      case DEVLIST_ISAVAILABLE_CUSTOM:
      {
        /* if remove is called on a custom device,
           it allows to remove it along with its configuration */
        GtkWidget *yesno_dlg = gtk_message_dialog_new( GTK_WINDOW(cfg_win) ,
          GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT , GTK_MESSAGE_QUESTION ,
          GTK_BUTTONS_YES_NO ,
          _("Do you want to remove the selected custom device?\n") );
        if ( gtk_dialog_run( GTK_DIALOG(yesno_dlg) ) == GTK_RESPONSE_YES )
        {
          gpointer bindings = NULL;
          gtk_tree_model_get( model , &iter , DEVLIST_COL_BINDINGS , &bindings , -1 );
          if ( bindings != NULL ) ed_bindings_store_delete( bindings );
          gtk_list_store_remove( GTK_LIST_STORE(model) , &iter );
        }
        gtk_widget_destroy( yesno_dlg );
        break;
      }

      case DEVLIST_ISAVAILABLE_DET:
      default:
      {
        /* do nothing for detected devices */
        break;
      }
    }
  }
}


static gboolean
cfg_config_cb_bindings_commit_foreach ( GtkTreeModel * model ,
                                        GtkTreePath * path ,
                                        GtkTreeIter * iter ,
                                        gpointer config_device_list_p )
{
  gchar *device_name = NULL;
  gchar *device_file = NULL;
  gchar *device_phys = NULL;
  gboolean is_active = FALSE;
  guint is_available = 0;
  gpointer bindings = NULL;
  ed_device_info_t *info;
  GList **config_device_list = config_device_list_p;

  gtk_tree_model_get( model , iter ,
    DEVLIST_COL_ISACTIVE , &is_active ,
    DEVLIST_COL_NAME , &device_name ,
    DEVLIST_COL_FILENAME , &device_file ,
    DEVLIST_COL_PHYS , &device_phys ,
    DEVLIST_COL_ISAVAILABLE , &is_available ,
    DEVLIST_COL_BINDINGS , &bindings , -1 );

  info = ed_device_info_new( device_name , device_file , device_phys ,
           ( is_available == DEVLIST_ISAVAILABLE_CUSTOM ? 1 : 0 ) );
  info->bindings = bindings;
  info->is_active = is_active;

  *config_device_list = g_list_append( *config_device_list , info );
  return FALSE;
}


static gboolean
cfg_config_cb_bindings_delbindings_foreach ( GtkTreeModel * model ,
                                             GtkTreePath * path ,
                                             GtkTreeIter * iter ,
                                             gpointer data )
{
  gpointer bindings = NULL;
  gtk_tree_model_get( model , iter , DEVLIST_COL_BINDINGS , &bindings , -1 );
  if ( bindings != NULL )
    ed_bindings_store_delete( bindings );
  return FALSE;
}


static void
cfg_config_cb_commit ( gpointer cfg_device_lv )
{
  GList *config_device_list = NULL;
  GtkTreeModel *model;

  model = gtk_tree_view_get_model( GTK_TREE_VIEW(cfg_device_lv) );
  /* fill config_device_list with information from the treeview */
  gtk_tree_model_foreach( model , cfg_config_cb_bindings_commit_foreach , &config_device_list );
  /* ok, now we have a list of ed_device_info_t objects */

  /* commit changes in configuration file */
  ed_config_save_from_list( config_device_list );

  /* not needed anymore */
  ed_device_free_list( config_device_list );

  /* free bindings stored in the liststore */
  gtk_tree_model_foreach( model , cfg_config_cb_bindings_delbindings_foreach , NULL );

  if ( plugin_is_active == TRUE )
  {
    /* restart device listening according to the (possibly changed) config file */
    ed_device_start_listening_from_config();
  }

  gtk_widget_destroy( cfg_win );
}


static void
cfg_config_cb_cancel ( gpointer cfg_device_lv )
{
  GtkTreeModel *model;
  model = gtk_tree_view_get_model( GTK_TREE_VIEW(cfg_device_lv) );

  /* free bindings stored in the liststore */
  gtk_tree_model_foreach( model , cfg_config_cb_bindings_delbindings_foreach , NULL );

  if ( plugin_is_active == TRUE )
  {
    /* restart device listening according to the (unchanged) config file */
    ed_device_start_listening_from_config();
  }

  gtk_widget_destroy( cfg_win );
}


void
ed_ui_config_show( void )
{
  GtkWidget *cfg_vbox;
  GtkWidget *cfg_device_lv, *cfg_device_lv_frame, *cfg_device_lv_sw;
  GtkTreeViewColumn *cfg_device_lv_col_name, *cfg_device_lv_col_filename, *cfg_device_lv_col_phys;
  GtkTreeViewColumn *cfg_device_lv_col_isactive, *cfg_device_lv_col_isavailable;
  GtkCellRenderer *cfg_device_lv_rndr_text, *cfg_device_lv_rndr_toggle;
  GtkCellRenderer *cfg_device_lv_rndr_textphys, *cfg_device_lv_rndr_isavailable;
  GtkListStore *device_store;
  GtkWidget *cfg_bbar_hbbox;
  GtkWidget *cfg_bbar_bt_bind, *cfg_bbar_bt_addc, *cfg_bbar_bt_remc;
  GtkWidget *cfg_bbar_bt_cancel, *cfg_bbar_bt_ok;
  GdkGeometry cfg_win_hints;

  if ( cfg_win != NULL )
  {
    gtk_window_present( GTK_WINDOW(cfg_win) );
    return;
  }

  /* IMPORTANT: stop listening for all devices when config window is opened */
  ed_device_stop_listening_all( TRUE );

  cfg_win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_window_set_type_hint( GTK_WINDOW(cfg_win), GDK_WINDOW_TYPE_HINT_DIALOG );
  gtk_window_set_position( GTK_WINDOW(cfg_win), GTK_WIN_POS_CENTER );
  gtk_window_set_title( GTK_WINDOW(cfg_win), _("EvDev-Plug - Configuration") );
  gtk_container_set_border_width( GTK_CONTAINER(cfg_win), 10 );
  g_signal_connect( G_OBJECT(cfg_win) , "destroy" ,
                    G_CALLBACK(gtk_widget_destroyed) , &cfg_win );
  cfg_win_hints.min_width = -1;
  cfg_win_hints.min_height = 300;
  gtk_window_set_geometry_hints( GTK_WINDOW(cfg_win) , GTK_WIDGET(cfg_win) ,
                                 &cfg_win_hints , GDK_HINT_MIN_SIZE );

  cfg_vbox = gtk_vbox_new( FALSE , 0 );
  gtk_container_add( GTK_CONTAINER(cfg_win) , cfg_vbox );

  /* current liststore model
     ----------------------------------------------
     G_TYPE_BOOLEAN -> device listening on/off
     G_TYPE_STRING -> device name
     G_TYPE_STRING -> device filename
     G_TYPE_STRING -> device physical port
     G_TYPE_UINT -> device is available
     G_TYPE_POINTER -> device bindings
     ----------------------------------------------
  */
  device_store = gtk_list_store_new(
    DEVLIST_NUMCOLS , G_TYPE_BOOLEAN , G_TYPE_STRING ,
    G_TYPE_STRING , G_TYPE_STRING , G_TYPE_UINT , G_TYPE_POINTER );
  cfg_device_lv_populate( device_store );

  cfg_device_lv_frame = gtk_frame_new( NULL );
  cfg_device_lv = gtk_tree_view_new_with_model( GTK_TREE_MODEL(device_store) );
  g_object_unref( device_store );

  cfg_device_lv_rndr_text = gtk_cell_renderer_text_new();
  cfg_device_lv_rndr_toggle = gtk_cell_renderer_toggle_new();
  cfg_device_lv_rndr_isavailable = gtk_cell_renderer_text_new();
  cfg_device_lv_rndr_textphys = gtk_cell_renderer_text_new();
  g_object_set( G_OBJECT(cfg_device_lv_rndr_textphys) ,
                "ellipsize-set" , TRUE , "ellipsize" , PANGO_ELLIPSIZE_END , NULL );

  cfg_device_lv_col_isactive = gtk_tree_view_column_new_with_attributes(
    _("Active") , cfg_device_lv_rndr_toggle , "active" , DEVLIST_COL_ISACTIVE , NULL );
  gtk_tree_view_column_set_expand( GTK_TREE_VIEW_COLUMN(cfg_device_lv_col_isactive) , FALSE );
  gtk_tree_view_append_column( GTK_TREE_VIEW(cfg_device_lv), cfg_device_lv_col_isactive );
  cfg_device_lv_col_isavailable = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title( cfg_device_lv_col_isavailable , _("Status") );
  gtk_tree_view_column_pack_start( cfg_device_lv_col_isavailable , cfg_device_lv_rndr_isavailable , TRUE );
  gtk_tree_view_column_set_cell_data_func(
    cfg_device_lv_col_isavailable , cfg_device_lv_rndr_isavailable ,
    (GtkTreeCellDataFunc)cfg_device_lv_celldatafunc_isavailable ,
    NULL , NULL );
  gtk_tree_view_column_set_expand( GTK_TREE_VIEW_COLUMN(cfg_device_lv_col_isavailable) , FALSE );
  gtk_tree_view_append_column( GTK_TREE_VIEW(cfg_device_lv), cfg_device_lv_col_isavailable );
  cfg_device_lv_col_name = gtk_tree_view_column_new_with_attributes(
    _("Device Name") , cfg_device_lv_rndr_text , "text" , DEVLIST_COL_NAME , NULL );
  gtk_tree_view_column_set_expand( GTK_TREE_VIEW_COLUMN(cfg_device_lv_col_name) , FALSE );
  gtk_tree_view_append_column( GTK_TREE_VIEW(cfg_device_lv), cfg_device_lv_col_name );
  cfg_device_lv_col_filename = gtk_tree_view_column_new_with_attributes(
    _("Device File") , cfg_device_lv_rndr_text , "text" , DEVLIST_COL_FILENAME , NULL );
  gtk_tree_view_column_set_expand( GTK_TREE_VIEW_COLUMN(cfg_device_lv_col_filename) , FALSE );
  gtk_tree_view_append_column( GTK_TREE_VIEW(cfg_device_lv), cfg_device_lv_col_filename );
  cfg_device_lv_col_phys = gtk_tree_view_column_new_with_attributes(
    _("Device Address") , cfg_device_lv_rndr_textphys , "text" , DEVLIST_COL_PHYS , NULL );
  gtk_tree_view_column_set_expand( GTK_TREE_VIEW_COLUMN(cfg_device_lv_col_phys) , TRUE );
  gtk_tree_view_column_set_resizable( GTK_TREE_VIEW_COLUMN(cfg_device_lv_col_phys) , TRUE );
  gtk_tree_view_append_column( GTK_TREE_VIEW(cfg_device_lv), cfg_device_lv_col_phys );
  cfg_device_lv_sw = gtk_scrolled_window_new( NULL , NULL );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(cfg_device_lv_sw) ,
                                  GTK_POLICY_NEVER , GTK_POLICY_ALWAYS );

  gtk_container_add( GTK_CONTAINER(cfg_device_lv_sw) , cfg_device_lv );
  gtk_container_add( GTK_CONTAINER(cfg_device_lv_frame) , cfg_device_lv_sw );
  gtk_box_pack_start( GTK_BOX(cfg_vbox) , cfg_device_lv_frame , TRUE , TRUE , 0 );

  gtk_box_pack_start( GTK_BOX(cfg_vbox) , gtk_hseparator_new() , FALSE , FALSE , 4 );

  /* button bar */
  cfg_bbar_hbbox = gtk_hbutton_box_new();
  gtk_button_box_set_layout( GTK_BUTTON_BOX(cfg_bbar_hbbox) , GTK_BUTTONBOX_START );
  cfg_bbar_bt_bind = gtk_button_new_with_mnemonic( _("_Bindings") );
  gtk_button_set_image( GTK_BUTTON(cfg_bbar_bt_bind) ,
    gtk_image_new_from_stock( GTK_STOCK_PREFERENCES , GTK_ICON_SIZE_BUTTON ) );
  cfg_bbar_bt_addc = gtk_button_new_from_stock( GTK_STOCK_ADD );
  cfg_bbar_bt_remc = gtk_button_new_from_stock( GTK_STOCK_REMOVE );
  /*cfg_bbar_bt_refresh = gtk_button_new_from_stock( GTK_STOCK_REFRESH );*/
  cfg_bbar_bt_cancel = gtk_button_new_from_stock( GTK_STOCK_CANCEL );
  cfg_bbar_bt_ok = gtk_button_new_from_stock( GTK_STOCK_OK );
  gtk_container_add( GTK_CONTAINER(cfg_bbar_hbbox) , cfg_bbar_bt_bind );
  gtk_container_add( GTK_CONTAINER(cfg_bbar_hbbox) , cfg_bbar_bt_addc );
  gtk_container_add( GTK_CONTAINER(cfg_bbar_hbbox) , cfg_bbar_bt_remc );
  /*gtk_container_add( GTK_CONTAINER(cfg_bbar_hbbox) , cfg_bbar_bt_refresh );*/
  gtk_container_add( GTK_CONTAINER(cfg_bbar_hbbox) , cfg_bbar_bt_cancel );
  gtk_container_add( GTK_CONTAINER(cfg_bbar_hbbox) , cfg_bbar_bt_ok );
  gtk_button_box_set_child_secondary( GTK_BUTTON_BOX(cfg_bbar_hbbox) , cfg_bbar_bt_cancel , TRUE );
  gtk_button_box_set_child_secondary( GTK_BUTTON_BOX(cfg_bbar_hbbox) , cfg_bbar_bt_ok , TRUE );
  gtk_box_pack_start( GTK_BOX(cfg_vbox) , cfg_bbar_hbbox , FALSE , FALSE , 0 );

  g_signal_connect( cfg_device_lv_rndr_toggle , "toggled" ,
                    G_CALLBACK(cfg_device_lv_changetoggle) , cfg_device_lv );
  g_signal_connect_swapped( G_OBJECT(cfg_bbar_bt_bind) , "clicked" ,
                            G_CALLBACK(cfg_config_cb_bindings_show) , cfg_device_lv );
  g_signal_connect_swapped( G_OBJECT(cfg_bbar_bt_addc) , "clicked" ,
                            G_CALLBACK(cfg_config_cb_addcustom_show) , cfg_device_lv );
  g_signal_connect_swapped( G_OBJECT(cfg_bbar_bt_remc) , "clicked" ,
                            G_CALLBACK(cfg_config_cb_remove) , cfg_device_lv );
  g_signal_connect_swapped( G_OBJECT(cfg_bbar_bt_cancel) , "clicked" ,
                            G_CALLBACK(cfg_config_cb_cancel) , cfg_device_lv );
  g_signal_connect_swapped( G_OBJECT(cfg_bbar_bt_ok) , "clicked" ,
                            G_CALLBACK(cfg_config_cb_commit) , cfg_device_lv );

  gtk_widget_show_all( cfg_win );
}



/* bindings window */

static void cfg_bindbox_new_empty_row( GtkTable * , GtkWidget * , gboolean );

enum
{
  BINDLIST_COL_COMBO_ACTION = 0,
  BINDLIST_COL_BT_ASSIGN,
  BINDLIST_COL_LABEL_EVCODE,
  BINDLIST_COL_BT_DELETE,
  BINDLIST_NUMCOLS
};


static gboolean
cfg_bindbox_assign_binding_timeout_func ( gpointer bindings_win )
{
  GtkWidget *trigger_dlg =  g_object_get_data( G_OBJECT(bindings_win) , "trigger-win" );
  if ( trigger_dlg != NULL )
    gtk_dialog_response( GTK_DIALOG(trigger_dlg) , GTK_RESPONSE_CANCEL );

  return FALSE;
}


static gboolean
cfg_bindbox_assign_binding_input_func ( GIOChannel * iochan ,
                                        GIOCondition cond ,
                                        gpointer bindings_win )
{
  switch ( cond )
  {
    case G_IO_IN:
    {
      gsize rb = 0;
      struct input_event inputev;
      g_io_channel_read_chars( iochan , (gchar*)&inputev ,
                               sizeof(struct input_event) , &rb , NULL );
      if ( rb == sizeof(struct input_event) )
      {
        GtkWidget *trigger_dlg = g_object_get_data( G_OBJECT(bindings_win) , "trigger-win" );
        if ( trigger_dlg == NULL )
        {
          /* the trigger-dialog window is not open; ignore input */
          return TRUE;
        }
        else
        {
          /* currently, only care about events of type 'key' and 'absolute'
             TODO: should we handle some other event type as well? */
          switch ( inputev.type )
          {
            case EV_KEY:
            case EV_ABS:
            {
              /* the trigger-dialog window is open; record input and
               store it in a container managed by the trigger-dialog itself */
              ed_inputevent_t *dinputev = g_malloc(sizeof(ed_inputevent_t));
              dinputev->type = inputev.type;
              dinputev->code = inputev.code;
              dinputev->value = inputev.value;
              g_object_set_data( G_OBJECT(trigger_dlg) , "trigger-data" , dinputev );
              gtk_dialog_response( GTK_DIALOG(trigger_dlg) , GTK_RESPONSE_OK );
              break;
            }
          }
        }
      }
    }

  default:
    ;
  }
  return TRUE;
}


static gboolean
cfg_bindbox_assign_binding_checkdups( GtkWidget * table , ed_inputevent_t * inputev )
{
  /* check if inputev is already assigned in table */
  GList *children = GTK_TABLE(table)->children;
  for ( ; children != NULL ; children = g_list_next(children) )
  {
    GtkTableChild *child = children->data;

    if ( child->top_attach + 1 == GTK_TABLE(table)->nrows )
      continue; /* skip last empty row */

    if ( child->left_attach == BINDLIST_COL_LABEL_EVCODE )
    {
      /* it's a label, pick its inputevent */
      ed_inputevent_t * iev = g_object_get_data( G_OBJECT(child->widget) , "inputevent" );
      if ( ( iev != NULL ) && ( ed_inputevent_check_equality( inputev , iev ) == TRUE ) )
        return TRUE; /* a duplicate was found */
    }
  }
  return FALSE;
}


static void
cfg_bindbox_assign_binding ( GtkButton * assign_button , gpointer bindings_win )
{
  GtkWidget *trigger_dlg;
  GtkWidget *trigger_dlg_frame;
  GtkWidget *trigger_dlg_label;
  guint timeout_sid = 0;
  gint res = 0;

  /* create a not-decorated window to inform the user about what's going on */
  trigger_dlg = gtk_dialog_new();
  gtk_widget_set_name( trigger_dlg , "trigger_dlg" );
  trigger_dlg_label = gtk_label_new( _("Press a key of your device to bind it;\n"
                                    "if no key is pressed in five seconds, this window\n"
                                    "will close without binding changes.") );
  gtk_widget_hide( GTK_WIDGET(GTK_DIALOG(trigger_dlg)->action_area) );
  gtk_misc_set_padding( GTK_MISC(trigger_dlg_label) , 10 , 10 );
  trigger_dlg_frame = gtk_frame_new( NULL );
  gtk_container_add( GTK_CONTAINER(trigger_dlg_frame) , trigger_dlg_label );
  gtk_container_add( GTK_CONTAINER(GTK_DIALOG(trigger_dlg)->vbox) , trigger_dlg_frame );
  gtk_window_set_position( GTK_WINDOW(trigger_dlg) , GTK_WIN_POS_CENTER );
  gtk_window_set_decorated( GTK_WINDOW(trigger_dlg) , FALSE );
  gtk_window_set_transient_for( GTK_WINDOW(trigger_dlg) ,
    GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(assign_button))) );
  gtk_dialog_set_has_separator( GTK_DIALOG(trigger_dlg) , FALSE );
  gtk_widget_show_all( trigger_dlg );

  /* close the trigger-window if no input is received in 5 seconds */
  timeout_sid = g_timeout_add( 5000 , cfg_bindbox_assign_binding_timeout_func , bindings_win );

  g_object_set_data( G_OBJECT(bindings_win) , "trigger-win" , trigger_dlg ); /* enable trigger-listening */
  res = gtk_dialog_run( GTK_DIALOG(trigger_dlg) );
  g_object_set_data( G_OBJECT(bindings_win) , "trigger-win" , NULL ); /* stop trigger-listening */
  switch ( res )
  {
    case GTK_RESPONSE_OK:
    {
      ed_inputevent_t *dinputev = g_object_get_data( G_OBJECT(trigger_dlg) , "trigger-data" );
      GtkWidget *label = g_object_get_data( G_OBJECT(assign_button) , "label" );
      GtkWidget *table = g_object_get_data( G_OBJECT(bindings_win) , "table" );
      gchar *input_str;

      /* we got a new input event; ensure that this has not been assigned to an action already */
      if ( cfg_bindbox_assign_binding_checkdups( table , dinputev ) == TRUE )
      {
        /* this input event has been already used */
        g_free( dinputev );
        g_source_remove( timeout_sid );
        ed_ui_message_show( _("Information") ,
                            _("This input event has been already assigned.\n\n"
                              "It's not possible to assign multiple actions to the same input event "
                              "(although it's possible to assign the same action to multiple events).") ,
                            bindings_win );
      }
      else
      {
        /* if this is the last row ("Assign" button) , create a new empty row */
        if ( GPOINTER_TO_INT(g_object_get_data(G_OBJECT(assign_button),"last")) == 1 )
        {
          cfg_bindbox_new_empty_row( GTK_TABLE(table) , bindings_win , TRUE );
          /* so, the current row is not the last one anymore;
             remove its "last" mark and enable its delete button */
          g_object_set_data( G_OBJECT(assign_button) , "last" , GINT_TO_POINTER(0) );
          gtk_widget_set_sensitive( GTK_WIDGET(g_object_get_data(G_OBJECT(assign_button),"delbt")) , TRUE );
        }

        g_object_set_data_full( G_OBJECT(label) , "inputevent" , dinputev , g_free );
        input_str = g_strdup_printf( "%i:%i:%i" , dinputev->type , dinputev->code , dinputev->value );
        gtk_label_set_text( GTK_LABEL(label) , input_str );
        g_free( input_str );
        g_source_remove( timeout_sid );
      }

      break;
    }

    default:
    {
      break; /* nothing should be changed */
    }
  }

  gtk_widget_destroy( trigger_dlg );
}


static void
cfg_bindbox_delete_row( GtkButton * delete_button , gpointer bindings_win )
{
  GtkTable *oldtable, *newtable;
  GtkWidget *table_sw, *table_vp;
  GList *children = NULL;
  gint drow = 0;

  /* here comes the drawback of using GtkTable to display
     a list-like set of widgets :) deletion can be a complex task */

  /* first, get the row number of the row that should be deleted
     (this can't be the last one cause it has the delete button disabled) */
  oldtable = g_object_get_data( G_OBJECT(bindings_win) , "table" );
  children = oldtable->children;
  while ( children != NULL )
  {
    GtkTableChild *child = children->data;
    if ( child->widget == (GtkWidget*)delete_button )
      drow = child->top_attach;
    children = g_list_next( children );
  }

  /* now, a simple trick: create a new GtkTable and move all
     of the widgets there, excepting those positioned in row "drow" */
  newtable = (GtkTable*)gtk_table_new( oldtable->nrows - 1 , oldtable->ncols , FALSE );

  children = oldtable->children;
  while ( children != NULL )
  {
    GtkTableChild *child = children->data;
    if ( child->top_attach < drow )
    {
      GtkWidget *widget = child->widget;
      guint widget_row = child->top_attach;
      guint widget_col = child->left_attach;
      g_object_ref( widget );
      gtk_container_remove( GTK_CONTAINER(oldtable) , widget );
      gtk_table_attach( GTK_TABLE(newtable) , widget ,
        widget_col , widget_col + 1 , widget_row , widget_row + 1 ,
        (widget_col == BINDLIST_COL_LABEL_EVCODE ? GTK_EXPAND | GTK_FILL : GTK_FILL ) , GTK_FILL , 0 , 0 );
      /* gtk_container_remove() changed the structure used to iterate; restart */
      children = oldtable->children;
    }
    else if ( child->top_attach > drow )
    {
      GtkWidget *widget = child->widget;
      guint widget_row = child->top_attach;
      guint widget_col = child->left_attach;
      g_object_ref( widget );
      gtk_container_remove( GTK_CONTAINER(oldtable) , widget );
      gtk_table_attach( GTK_TABLE(newtable) , widget ,
        widget_col , widget_col + 1 , widget_row - 1 , widget_row ,
        (widget_col == BINDLIST_COL_LABEL_EVCODE ? GTK_EXPAND | GTK_FILL : GTK_FILL ) , GTK_FILL , 0 , 0 );
      /* gtk_container_remove() changed the structure used to iterate; restart */
      children = oldtable->children;
    }
    else
      children = g_list_next(children);
  }

  /* now, replace the old GtkTable with the new one */
  table_sw = g_object_get_data( G_OBJECT(bindings_win) , "tablesw" );
  table_vp = gtk_bin_get_child( GTK_BIN(table_sw) ); /* get the viewport */
  gtk_widget_destroy( GTK_WIDGET(oldtable) ); /* destroy old GtkTable */
  gtk_container_add( GTK_CONTAINER(table_vp) , GTK_WIDGET(newtable) );
  g_object_set_data( G_OBJECT(bindings_win) , "table" , newtable ); /* update table in bindings_win */
  gtk_widget_show_all( GTK_WIDGET(newtable) );
  return;
}


/* the three structure types defined below
   are only used in cfg_bindbox_populate process */
typedef struct
{
  GtkWidget *combobox;
  gint action_code;
}
combosas_helper_t;

typedef struct
{
  gint action_code;
  ed_inputevent_t * inputev;
}
popul_binding_t;

typedef struct
{
  gint id;
  popul_binding_t *bindings;
}
popul_container_binding_t;

static gboolean
cfg_bindbox_populate_foreach_combosas ( GtkTreeModel * model ,
                                        GtkTreePath * path ,
                                        GtkTreeIter * iter ,
                                        gpointer combosas_helper_gp )
{
  gint action_code = 0;
  combosas_helper_t *combosas_helper = combosas_helper_gp;
  gtk_tree_model_get( model , iter , 1 , &action_code , -1 );
  if ( action_code == combosas_helper->action_code )
  {
    gtk_combo_box_set_active_iter( GTK_COMBO_BOX(combosas_helper->combobox) , iter );
    return TRUE;
  }
  return FALSE;
}

static void
cfg_bindbox_populate_foreach ( ed_inputevent_t * sinputev ,
                               gint action_code ,
                               gpointer popul_container_gp ,
                               gpointer none )
{
  ed_inputevent_t *inputev;
  popul_container_binding_t *popul_container = popul_container_gp;

  /* make a copy of the ed_inputevent_t object "sinputev" */
  inputev = g_malloc(sizeof(ed_inputevent_t));
  inputev->type = sinputev->type;
  inputev->code = sinputev->code;
  inputev->value = sinputev->value;

  popul_container->bindings[popul_container->id].inputev = inputev;
  popul_container->bindings[popul_container->id].action_code = action_code;

  popul_container->id++;
  return;
}

static int
cfg_bindbox_populate_qsortfunc( const void * p_binding_a , const void * p_binding_b )
{
  return ((popul_binding_t*)p_binding_a)->action_code - ((popul_binding_t*)p_binding_b)->action_code;
}

static void
cfg_bindbox_populate( GtkWidget * bind_table , GtkWidget * bindings_win , gpointer bindings )
{
  if ( bindings != NULL )
  {
    guint bindings_num = ed_bindings_store_size( bindings );
    popul_binding_t *popul_binding = (popul_binding_t*)calloc( bindings_num , sizeof(popul_binding_t) );
    popul_container_binding_t *popul_container = g_malloc(sizeof(popul_container_binding_t));
    gint i = 0;

    popul_container->bindings = popul_binding;
    popul_container->id = 0;

    ed_bindings_store_foreach( bindings ,
      (ed_bindings_store_foreach_func)cfg_bindbox_populate_foreach , popul_container , NULL );

    /* ok, now we have a popul_binding array, reorder it using action_codes */
    qsort( popul_binding , bindings_num , sizeof(popul_binding_t) , cfg_bindbox_populate_qsortfunc );

    /* attach to table each popul_binding */
    for ( i = 0 ; i < bindings_num ; i++ )
    {
      GList *children = GTK_TABLE(bind_table)->children;
      for ( ; children != NULL ; children = g_list_next(children) )
      {
        GtkTableChild *child = children->data;
        if ( ( (child->top_attach + 1) == GTK_TABLE(bind_table)->nrows ) &&
             ( child->left_attach == BINDLIST_COL_BT_ASSIGN ) &&
             ( GPOINTER_TO_INT(g_object_get_data(G_OBJECT(child->widget),"last")) == 1 ) )
        {
          /* ok, this child->widget is the last assign button */
          GtkWidget *combobox = g_object_get_data(G_OBJECT(child->widget),"combobox");
          GtkWidget *label = g_object_get_data(G_OBJECT(child->widget),"label");
          GtkWidget *delbt = g_object_get_data(G_OBJECT(child->widget),"delbt");
          GtkTreeModel *combomodel;
          combosas_helper_t *combosas_helper;
          gchar *input_str;

          combomodel = gtk_combo_box_get_model( GTK_COMBO_BOX(combobox) );
          combosas_helper = g_malloc(sizeof(combosas_helper_t));
          combosas_helper->combobox = combobox;
          combosas_helper->action_code = popul_binding[i].action_code;
          gtk_tree_model_foreach( combomodel , cfg_bindbox_populate_foreach_combosas , combosas_helper );
          g_free( combosas_helper );

          g_object_set_data_full( G_OBJECT(label) , "inputevent" , popul_binding[i].inputev , g_free );
          input_str = g_strdup_printf( "%i:%i:%i" ,
            popul_binding[i].inputev->type ,
            popul_binding[i].inputev->code ,
            popul_binding[i].inputev->value );
          gtk_label_set_text( GTK_LABEL(label) , input_str );
          g_free( input_str );

          /* so, the current row wont' be the last one anymore;
             remove its "last" mark and enable its delete button */
          g_object_set_data( G_OBJECT(child->widget) , "last" , GINT_TO_POINTER(0) );
          gtk_widget_set_sensitive( delbt , TRUE );
          cfg_bindbox_new_empty_row( GTK_TABLE(bind_table) , bindings_win , TRUE );

        }
      }
      DEBUGMSG( "populating bindings window: code %i -> event %d:%d:%d\n" ,
        popul_binding[i].action_code , popul_binding[i].inputev->type,
        popul_binding[i].inputev->code, popul_binding[i].inputev->value );
    }
    g_free( popul_binding );
    g_free( popul_container );
  }
  return;
}


static void
cfg_bindbox_new_empty_row( GtkTable * bind_table , GtkWidget * bindings_win , gboolean resize_table )
{
  GtkWidget *action_combobox;
  GtkCellRenderer *action_combobox_rndr;
  GtkWidget *assign_button;
  GtkWidget *assign_label;
  GtkWidget *delete_button;

  /* add one row to the table */
  if ( resize_table == TRUE )
    gtk_table_resize( bind_table , bind_table->nrows + 1 , bind_table->ncols );

  /* action combobox */
  action_combobox = gtk_combo_box_new_with_model(
   GTK_TREE_MODEL(g_object_get_data(G_OBJECT(bindings_win),"action_store")) );
  action_combobox_rndr = gtk_cell_renderer_text_new();
  gtk_cell_layout_pack_start( GTK_CELL_LAYOUT(action_combobox) , action_combobox_rndr , TRUE );
  gtk_cell_layout_set_attributes( GTK_CELL_LAYOUT(action_combobox) ,
    action_combobox_rndr , "text" , 0 , NULL );
  gtk_combo_box_set_active( GTK_COMBO_BOX(action_combobox) , 0 );

  gtk_table_attach( bind_table , action_combobox ,
    BINDLIST_COL_COMBO_ACTION , BINDLIST_COL_COMBO_ACTION + 1 ,
    bind_table->nrows - 1 , bind_table->nrows ,
    GTK_FILL , GTK_FILL , 0 , 0 );

  /* assign button */
  assign_button = gtk_button_new();
  gtk_button_set_image( GTK_BUTTON(assign_button) ,
                        gtk_image_new_from_stock( GTK_STOCK_EXECUTE , GTK_ICON_SIZE_BUTTON ) );
  g_object_set_data( G_OBJECT(assign_button) , "last" , GINT_TO_POINTER(1) );
  g_signal_connect( G_OBJECT(assign_button) , "clicked" ,
                    G_CALLBACK(cfg_bindbox_assign_binding) , bindings_win );

  gtk_table_attach( bind_table , assign_button ,
    BINDLIST_COL_BT_ASSIGN , BINDLIST_COL_BT_ASSIGN + 1 ,
    bind_table->nrows - 1 , bind_table->nrows ,
    GTK_FILL , GTK_FILL , 0 , 0 );

  /* assigned-code label */
  assign_label = gtk_label_new( "" );
  gtk_misc_set_alignment( GTK_MISC(assign_label) , 0 , 0.5 );
  gtk_misc_set_padding( GTK_MISC(assign_label) , 10 , 0 );
  g_object_set_data_full( G_OBJECT(assign_label) , "inputevent" , NULL , g_free );

  gtk_table_attach( bind_table , assign_label ,
    BINDLIST_COL_LABEL_EVCODE , BINDLIST_COL_LABEL_EVCODE +1 ,
    bind_table->nrows - 1 , bind_table->nrows ,
    GTK_EXPAND | GTK_FILL , GTK_FILL , 0 , 0 );

  /* delete button */
  delete_button = gtk_button_new();
  gtk_button_set_image( GTK_BUTTON(delete_button) ,
                        gtk_image_new_from_stock( GTK_STOCK_DELETE , GTK_ICON_SIZE_BUTTON ) );
  g_signal_connect( G_OBJECT(delete_button) , "clicked" ,
                    G_CALLBACK(cfg_bindbox_delete_row) , bindings_win );
  gtk_widget_set_sensitive( delete_button , FALSE );

  gtk_table_attach( bind_table , delete_button ,
    BINDLIST_COL_BT_DELETE , BINDLIST_COL_BT_DELETE + 1 ,
    bind_table->nrows - 1 , bind_table->nrows ,
    GTK_FILL , GTK_FILL , 0 , 0 );

  /* data for assign button */
  g_object_set_data( G_OBJECT(assign_button) , "combobox" , action_combobox );
  g_object_set_data( G_OBJECT(assign_button) , "label" , assign_label );
  g_object_set_data( G_OBJECT(assign_button) , "delbt" , delete_button );

  gtk_widget_show_all( GTK_WIDGET(bind_table) );
  return;
}


static void
cfg_bindings_cb_destroy ( GtkObject * bindings_win , gpointer dev_gp )
{
  ed_device_t *dev = dev_gp;
  /* bindings window is going to be destroyed;
     deactivate device listening and free the ed_device_t object */
  g_source_remove( dev->iochan_sid );
  ed_device_delete( dev );

  /* the rowreference is no longer needed as well */
  gtk_tree_row_reference_free(
    (GtkTreeRowReference*)g_object_get_data(G_OBJECT(bindings_win),"rowref") );
  return;
}


static void
cfg_bindings_cb_commit ( gpointer bindings_win )
{
  GtkTreeRowReference *rowref = g_object_get_data(G_OBJECT(bindings_win),"rowref");
  if ( gtk_tree_row_reference_valid( rowref ) == TRUE )
  {
    GtkTreeModel *model = gtk_tree_row_reference_get_model( rowref );
    GtkTreePath *path = gtk_tree_row_reference_get_path( rowref );
    GtkTreeIter iter;
    GtkTable *table;
    gpointer new_bindings = NULL, old_bindings = NULL;

    /****************************************************************************/
    table = g_object_get_data( G_OBJECT(bindings_win) , "table" );
    if ( table->nrows > 1 )
    {
      /* bindings defined */
      GList *children = table->children;
      gint *array_actioncode;
      ed_inputevent_t **array_inputevent;
      gint i = 0;

      array_actioncode = calloc( table->nrows - 1 , sizeof(gint) );
      array_inputevent = calloc( table->nrows - 1 , sizeof(ed_inputevent_t*) );

      for ( ; children != NULL ; children = g_list_next( children ) )
      {
        /* pick information from relevant table cells and put them in arrays */
        GtkTableChild *child = children->data;

        if ( ( child->top_attach + 1 ) == table->nrows )
          continue; /* skip last empty row */

        if ( child->left_attach == BINDLIST_COL_COMBO_ACTION )
        {
          GtkTreeModel *combomodel = gtk_combo_box_get_model( GTK_COMBO_BOX(child->widget) );
          GtkTreeIter comboiter;
          gint actioncode = 0;

          gtk_combo_box_get_active_iter( GTK_COMBO_BOX(child->widget) , &comboiter );
          gtk_tree_model_get( combomodel , &comboiter , 1 , &actioncode , -1 ); /* pick the action code */
          array_actioncode[child->top_attach] = actioncode;
        }
        else if ( child->left_attach == BINDLIST_COL_LABEL_EVCODE )
        {
          ed_inputevent_t *inputevent = g_object_get_data( G_OBJECT(child->widget) , "inputevent" );
          array_inputevent[child->top_attach] = inputevent;
        }
      }

      /* ok, now copy data from arrays to the bindings store */
      new_bindings = ed_bindings_store_new(); /* new bindings store object */

      for ( i = 0 ; i < ( table->nrows - 1 ) ; i++ )
        ed_bindings_store_insert( new_bindings , array_inputevent[i] , array_actioncode[i] );

      g_free( array_actioncode ); g_free( array_inputevent ); /* not needed anymore */
    }
    else
    {
      /* no bindings defined */
      new_bindings = NULL;
    }
    /****************************************************************************/

    /* pick old bindings store and delete it */
    gtk_tree_model_get_iter( model , &iter , path );
    gtk_tree_model_get( model , &iter, DEVLIST_COL_BINDINGS , &old_bindings , -1 );
    if ( old_bindings != NULL )
      ed_bindings_store_delete( old_bindings );
    /* replace it with new one */
    gtk_list_store_set( GTK_LIST_STORE(model) , &iter , DEVLIST_COL_BINDINGS , new_bindings , -1 );
    /* everything done! */
  }
  gtk_widget_destroy( GTK_WIDGET(bindings_win) );
}


#define action_store_add(x) { gtk_list_store_append(action_store,&iter); gtk_list_store_set(action_store,&iter,0,_(player_actions[x].desc),1,x,-1); }

static void
cfg_ui_bindings_show ( ed_device_t * dev , GtkTreeRowReference * rowref )
{
  GtkWidget *bindings_win;
  GdkGeometry bindings_win_hints;
  GtkWidget *bindings_vbox;
  GtkWidget *bindings_info_table, *bindings_info_frame;
  GtkWidget *bindings_info_label_name_p, *bindings_info_label_name_c;
  GtkWidget *bindings_info_label_file_p, *bindings_info_label_file_c;
  GtkWidget *bindings_info_label_phys_p, *bindings_info_label_phys_c;
  GtkWidget *bindings_bind_frame, *bindings_bind_table, *bindings_bind_table_sw;
  GtkWidget *bindings_bbar_hbbox, *bindings_bbar_bt_cancel, *bindings_bbar_bt_ok;
  GtkListStore *action_store;
  GtkTreeIter iter;
  static gboolean style_added = FALSE;

  if ( !style_added )
  {
    /* this is used by trigger dialogs, calling it once is enough */
    gtk_rc_parse_string( "style \"noaaborder\" { GtkDialog::action-area-border = 0 }\n"
                         "widget \"trigger_dlg\" style \"noaaborder\"\n" );
    style_added = TRUE;
  }

  bindings_win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_window_set_type_hint( GTK_WINDOW(bindings_win), GDK_WINDOW_TYPE_HINT_DIALOG );
  gtk_window_set_position( GTK_WINDOW(bindings_win), GTK_WIN_POS_CENTER );
  gtk_window_set_transient_for( GTK_WINDOW(bindings_win), GTK_WINDOW(cfg_win) );
  gtk_window_set_modal( GTK_WINDOW(bindings_win) , TRUE );
  gtk_window_set_title( GTK_WINDOW(bindings_win), _("EvDev-Plug - Bindings Configuration") );
  gtk_container_set_border_width( GTK_CONTAINER(bindings_win), 10 );
  bindings_win_hints.min_width = 400;
  bindings_win_hints.min_height = 400;
  gtk_window_set_geometry_hints( GTK_WINDOW(bindings_win) , GTK_WIDGET(bindings_win) ,
                                 &bindings_win_hints , GDK_HINT_MIN_SIZE );

  g_object_set_data( G_OBJECT(bindings_win) , "rowref" , rowref );
  g_object_set_data( G_OBJECT(bindings_win) , "trigger-win" , NULL );

  bindings_vbox = gtk_vbox_new( FALSE , 4 );
  gtk_container_add( GTK_CONTAINER(bindings_win) , bindings_vbox );

  /* action combobox model
     column 0 -> action desc
     column 1 -> action code */
  action_store = gtk_list_store_new( 2 , G_TYPE_STRING , G_TYPE_INT );
  action_store_add( ED_ACTION_PB_PLAY );
  action_store_add( ED_ACTION_PB_STOP );
  action_store_add( ED_ACTION_PB_PAUSE );
  action_store_add( ED_ACTION_PB_PREV );
  action_store_add( ED_ACTION_PB_NEXT );
  action_store_add( ED_ACTION_PB_EJECT );
  action_store_add( ED_ACTION_PL_REPEAT );
  action_store_add( ED_ACTION_PL_SHUFFLE );
  action_store_add( ED_ACTION_VOL_UP5 );
  action_store_add( ED_ACTION_VOL_DOWN5 );
  action_store_add( ED_ACTION_VOL_UP10 );
  action_store_add( ED_ACTION_VOL_DOWN10 );
  action_store_add( ED_ACTION_VOL_MUTE );
  action_store_add( ED_ACTION_WIN_MAIN );
  action_store_add( ED_ACTION_WIN_PLAYLIST );
  action_store_add( ED_ACTION_WIN_EQUALIZER );
  action_store_add( ED_ACTION_WIN_JTF );
  g_object_set_data_full( G_OBJECT(bindings_win) , "action_store" , action_store , g_object_unref );

  /* info table */
  bindings_info_table = gtk_table_new( 3 , 2 , FALSE );
  gtk_container_set_border_width( GTK_CONTAINER(bindings_info_table), 4 );
  bindings_info_label_name_p = gtk_label_new( "" );
  gtk_label_set_markup( GTK_LABEL(bindings_info_label_name_p) , _("<b>Name: </b>") );
  gtk_misc_set_alignment( GTK_MISC(bindings_info_label_name_p) , 0 , 0.5 );
  bindings_info_label_name_c = gtk_label_new( dev->info->name );
  gtk_misc_set_alignment( GTK_MISC(bindings_info_label_name_c) , 0 , 0.5 );
  gtk_table_attach( GTK_TABLE(bindings_info_table) , bindings_info_label_name_p , 0 , 1 , 0 , 1 ,
                    GTK_FILL , GTK_FILL | GTK_EXPAND , 0 , 0 );
  gtk_table_attach( GTK_TABLE(bindings_info_table) , bindings_info_label_name_c , 1 , 2 , 0 , 1 ,
                    GTK_FILL , GTK_FILL | GTK_EXPAND , 0 , 0 );
  bindings_info_label_file_p = gtk_label_new( "" );
  gtk_label_set_markup( GTK_LABEL(bindings_info_label_file_p) , _("<b>Filename: </b>") );
  gtk_misc_set_alignment( GTK_MISC(bindings_info_label_file_p) , 0 , 0.5 );
  bindings_info_label_file_c = gtk_label_new( dev->info->filename );
  gtk_misc_set_alignment( GTK_MISC(bindings_info_label_file_c) , 0 , 0.5 );
  gtk_table_attach( GTK_TABLE(bindings_info_table) , bindings_info_label_file_p , 0 , 1 , 1 , 2 ,
                    GTK_FILL , GTK_FILL | GTK_EXPAND , 0 , 0 );
  gtk_table_attach( GTK_TABLE(bindings_info_table) , bindings_info_label_file_c , 1 , 2 , 1 , 2 ,
                    GTK_FILL , GTK_FILL | GTK_EXPAND , 0 , 0 );
  bindings_info_label_phys_p = gtk_label_new( "" );
  gtk_label_set_markup( GTK_LABEL(bindings_info_label_phys_p) , _("<b>Phys.Address: </b>") );
  gtk_misc_set_alignment( GTK_MISC(bindings_info_label_phys_p) , 0 , 0.5 );
  bindings_info_label_phys_c = gtk_label_new( dev->info->phys );
  gtk_misc_set_alignment( GTK_MISC(bindings_info_label_phys_c) , 0 , 0.5 );
  gtk_table_attach( GTK_TABLE(bindings_info_table) , bindings_info_label_phys_p , 0 , 1 , 2 , 3 ,
                    GTK_FILL , GTK_FILL | GTK_EXPAND , 0 , 0 );
  gtk_table_attach( GTK_TABLE(bindings_info_table) , bindings_info_label_phys_c , 1 , 2 , 2 , 3 ,
                    GTK_FILL , GTK_FILL | GTK_EXPAND , 0 , 0 );
  bindings_info_frame = gtk_frame_new( NULL );
  gtk_container_add( GTK_CONTAINER(bindings_info_frame) , bindings_info_table );
  gtk_box_pack_start( GTK_BOX(bindings_vbox) , bindings_info_frame , FALSE , FALSE , 0 );

  /* bindings boxlist */
  bindings_bind_table = gtk_table_new( 1 , BINDLIST_NUMCOLS , FALSE );
  cfg_bindbox_new_empty_row( GTK_TABLE(bindings_bind_table) , bindings_win , FALSE );
  cfg_bindbox_populate( bindings_bind_table , bindings_win , dev->info->bindings );
  bindings_bind_table_sw = gtk_scrolled_window_new( NULL , NULL );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(bindings_bind_table_sw) ,
                                  GTK_POLICY_NEVER , GTK_POLICY_ALWAYS );
  bindings_bind_frame = gtk_frame_new( NULL );
  gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW(bindings_bind_table_sw) ,
                                         bindings_bind_table );
  gtk_container_add( GTK_CONTAINER(bindings_bind_frame) , bindings_bind_table_sw );
  gtk_box_pack_start( GTK_BOX(bindings_vbox) , bindings_bind_frame , TRUE , TRUE , 0 );
  g_object_set_data( G_OBJECT(bindings_win) , "table" , bindings_bind_table );
  g_object_set_data( G_OBJECT(bindings_win) , "tablesw" , bindings_bind_table_sw );

  gtk_box_pack_start( GTK_BOX(bindings_vbox) , gtk_hseparator_new() , FALSE , FALSE , 0 );

  /* button bar */
  bindings_bbar_hbbox = gtk_hbutton_box_new();
  gtk_button_box_set_layout( GTK_BUTTON_BOX(bindings_bbar_hbbox) , GTK_BUTTONBOX_START );
  bindings_bbar_bt_cancel = gtk_button_new_from_stock( GTK_STOCK_CANCEL );
  bindings_bbar_bt_ok = gtk_button_new_from_stock( GTK_STOCK_OK );
  gtk_container_add( GTK_CONTAINER(bindings_bbar_hbbox) , bindings_bbar_bt_cancel );
  gtk_container_add( GTK_CONTAINER(bindings_bbar_hbbox) , bindings_bbar_bt_ok );
  gtk_button_box_set_child_secondary( GTK_BUTTON_BOX(bindings_bbar_hbbox) , bindings_bbar_bt_cancel , TRUE );
  gtk_button_box_set_child_secondary( GTK_BUTTON_BOX(bindings_bbar_hbbox) , bindings_bbar_bt_ok , TRUE );
  gtk_box_pack_start( GTK_BOX(bindings_vbox) , bindings_bbar_hbbox , FALSE , FALSE , 0 );

  /* activate device listening */
  dev->iochan_sid = g_io_add_watch( dev->iochan , G_IO_IN ,
    (GIOFunc)cfg_bindbox_assign_binding_input_func , bindings_win );

  /* signals */
  g_signal_connect( G_OBJECT(bindings_win) , "destroy" ,
                    G_CALLBACK(cfg_bindings_cb_destroy) , dev );
  g_signal_connect_swapped( G_OBJECT(bindings_bbar_bt_cancel) , "clicked" ,
                            G_CALLBACK(gtk_widget_destroy) , bindings_win );
  g_signal_connect_swapped( G_OBJECT(bindings_bbar_bt_ok) , "clicked" ,
                            G_CALLBACK(cfg_bindings_cb_commit) , bindings_win );

  gtk_widget_show_all( bindings_win );
}



/* about box */
void
ed_ui_about_show( void )
{
  static GtkWidget *about_win = NULL;
  GtkWidget *about_vbox;
  GtkWidget *logoandinfo_vbox;
  GtkWidget *info_tv, *info_tv_sw, *info_tv_frame;
  GtkWidget *bbar_bbox, *bbar_bt_ok;
  GtkTextBuffer *info_tb;
  GdkGeometry abount_win_hints;
  gchar *info_tb_content = NULL;

  if ( about_win != NULL )
  {
    gtk_window_present( GTK_WINDOW(about_win) );
    return;
  }

  about_win = gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_window_set_type_hint( GTK_WINDOW(about_win), GDK_WINDOW_TYPE_HINT_DIALOG );
  gtk_window_set_position( GTK_WINDOW(about_win), GTK_WIN_POS_CENTER );
  gtk_window_set_title( GTK_WINDOW(about_win), _("EvDev-Plug - about") );
  abount_win_hints.min_width = 420;
  abount_win_hints.min_height = 200;
  gtk_window_set_geometry_hints( GTK_WINDOW(about_win) , GTK_WIDGET(about_win) ,
                                 &abount_win_hints , GDK_HINT_MIN_SIZE );
  /* gtk_window_set_resizable( GTK_WINDOW(about_win) , FALSE ); */
  gtk_container_set_border_width( GTK_CONTAINER(about_win) , 10 );
  g_signal_connect( G_OBJECT(about_win) , "destroy" , G_CALLBACK(gtk_widget_destroyed) , &about_win );

  about_vbox = gtk_vbox_new( FALSE , 0 );
  gtk_container_add( GTK_CONTAINER(about_win) , about_vbox );

  logoandinfo_vbox = gtk_vbox_new( TRUE , 2 );

  /* TODO make a logo or make someone do it! :)
  logo_pixbuf = gdk_pixbuf_new_from_xpm_data( (const gchar **)evdev_plug_logo_xpm );
  logo_image = gtk_image_new_from_pixbuf( logo_pixbuf );
  g_object_unref( logo_pixbuf );

  logo_frame = gtk_frame_new( NULL );
  gtk_container_add( GTK_CONTAINER(logo_frame) , logo_image );
  gtk_box_pack_start( GTK_BOX(logoandinfo_vbox) , logo_frame , TRUE , TRUE , 0 );  */

  info_tv = gtk_text_view_new();
  info_tb = gtk_text_view_get_buffer( GTK_TEXT_VIEW(info_tv) );
  gtk_text_view_set_editable( GTK_TEXT_VIEW(info_tv) , FALSE );
  gtk_text_view_set_cursor_visible( GTK_TEXT_VIEW(info_tv) , FALSE );
  gtk_text_view_set_justification( GTK_TEXT_VIEW(info_tv) , GTK_JUSTIFY_LEFT );
  gtk_text_view_set_left_margin( GTK_TEXT_VIEW(info_tv) , 10 );

  info_tb_content = g_strjoin( NULL , "\nEvDev-Plug " , ED_VERSION_PLUGIN ,
                               _("\nplayer remote control via event devices\n"
                                 "http://www.develia.org/projects.php?p=audacious#evdevplug\n\n"
                                 "written by Giacomo Lozito\n") ,
                              "< james@develia.org >\n\n" , NULL );
  gtk_text_buffer_set_text( info_tb , info_tb_content , -1 );
  g_free( info_tb_content );

  info_tv_sw = gtk_scrolled_window_new( NULL , NULL );
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(info_tv_sw) ,
                                  GTK_POLICY_NEVER , GTK_POLICY_ALWAYS );
  gtk_container_add( GTK_CONTAINER(info_tv_sw) , info_tv );
  info_tv_frame = gtk_frame_new( NULL );
  gtk_container_add( GTK_CONTAINER(info_tv_frame) , info_tv_sw );
  gtk_box_pack_start( GTK_BOX(logoandinfo_vbox) , info_tv_frame , TRUE , TRUE , 0 );

  gtk_box_pack_start( GTK_BOX(about_vbox) , logoandinfo_vbox , TRUE , TRUE , 0 );

  /* horizontal separator and buttons */
  gtk_box_pack_start( GTK_BOX(about_vbox) , gtk_hseparator_new() , FALSE , FALSE , 4 );
  bbar_bbox = gtk_hbutton_box_new();
  gtk_button_box_set_layout( GTK_BUTTON_BOX(bbar_bbox) , GTK_BUTTONBOX_END );
  bbar_bt_ok = gtk_button_new_from_stock( GTK_STOCK_OK );
  g_signal_connect_swapped( G_OBJECT(bbar_bt_ok) , "clicked" ,
                            G_CALLBACK(gtk_widget_destroy) , about_win );
  gtk_container_add( GTK_CONTAINER(bbar_bbox) , bbar_bt_ok );
  gtk_box_pack_start( GTK_BOX(about_vbox) , bbar_bbox , FALSE , FALSE , 0 );

  gtk_widget_show_all( about_win );
}



/* message box */
void
ed_ui_message_show ( gchar * title , gchar * message , gpointer parent_win_gp )
{
  GtkWidget *message_win;
  GtkWindow *parent_win = NULL;

  if (( parent_win_gp != NULL ) && ( GTK_WIDGET_TOPLEVEL(GTK_WIDGET(parent_win_gp)) ))
    parent_win = GTK_WINDOW(parent_win_gp);

  message_win = gtk_message_dialog_new(
                  parent_win ,
                  ( parent_win != NULL ? GTK_DIALOG_DESTROY_WITH_PARENT : 0 ) ,
                  GTK_MESSAGE_INFO , GTK_BUTTONS_CLOSE , "%s", message );
  gtk_window_set_title( GTK_WINDOW(message_win) , title );

  gtk_dialog_run( GTK_DIALOG(message_win) );
  gtk_widget_destroy( message_win );
}
