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

#include "ed_types.h"
#include "ed_internals.h"
#include "ed_actions.h"
#include "ed_bindings_store.h"
#include "ed_common.h"

#include <stdint.h>
#include <stdio.h>
#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <regex.h>
/* for variadic */
#include <stdarg.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>

static gboolean ed_device_giofunc ( GIOChannel * , GIOCondition , gpointer );

static gint ed_util_get_data_from_keyfile( GKeyFile * , gchar * , ... );
static gpointer ed_util_get_bindings_from_keyfile( GKeyFile * , gchar * );

extern GList *ed_device_listening_list;


/* ***************** */
/*     internals     */

ed_device_info_t *
ed_device_info_new ( gchar * device_name , gchar * device_filename ,
                     gchar * device_phys , gint device_is_custom )
{
  ed_device_info_t *info = g_malloc(sizeof(ed_device_info_t));
  info->name = g_strdup(device_name);
  info->filename = g_strdup(device_filename);
  info->phys = g_strdup(device_phys);
  info->is_custom = device_is_custom;
  info->is_active = FALSE;
  info->bindings = NULL;
  info->reg = 0;
  return info;
}


gint
ed_device_info_delete ( ed_device_info_t * info )
{
  g_free( info->phys );
  g_free( info->filename );
  g_free( info->name );
  g_free( info );
  return 0;
}


ed_device_t *
ed_device_new ( gchar * device_name , gchar * device_filename ,
                gchar * device_phys , gint device_is_custom )
{
  ed_device_t *event_device;
  GIOChannel *iochan;
  gint fd;

  fd = g_open( device_filename , O_RDONLY , 0 );
  if ( fd < 0 )
  {
    /* an error occurred */
    g_warning( _("event-device-plugin: unable to open device file %s , skipping this device; check that "
               "the file exists and that you have read permission for it\n") , device_filename );
    return NULL;
  }

  iochan = g_io_channel_unix_new( fd );
  if ( iochan == NULL )
  {
    /* an error occurred */
    g_warning( _("event-device-plugin: unable to create a io_channel for device file %s ,"
               "skipping this device\n") , device_filename );
    close( fd );
    return NULL;
  }
  g_io_channel_set_encoding( iochan , NULL , NULL ); /* binary data */

  event_device = g_malloc(sizeof(ed_device_t));
  event_device->fd = fd;
  event_device->iochan = iochan;
  event_device->is_listening = FALSE;
  event_device->info = ed_device_info_new(
    device_name , device_filename , device_phys , device_is_custom );

  return event_device;
}


gint
ed_device_delete ( ed_device_t * event_device )
{
  if ( event_device->is_listening )
    ed_device_stop_listening( event_device );

  g_io_channel_shutdown( event_device->iochan , TRUE , NULL );
  g_io_channel_unref( event_device->iochan );
  close( event_device->fd );

  ed_device_info_delete( event_device->info );
  g_free( event_device );

  return 0;
}


gboolean
ed_inputevent_check_equality( ed_inputevent_t *iev1 , ed_inputevent_t *iev2 )
{
  if (( iev1 == NULL ) || ( iev2 == NULL ))
  {
    if (( iev1 == NULL ) && ( iev2 == NULL ))
      return TRUE;
    else
      return FALSE;
  }

  if ( ( iev1->code == iev2->code ) &&
       ( iev1->type == iev2->type ) &&
       ( iev1->value == iev2->value ) )
    return TRUE;
  else
    return FALSE;
}


gboolean
ed_device_info_check_equality( ed_device_info_t *info1 , ed_device_info_t *info2 )
{
  if (( info1 == NULL ) || ( info2 == NULL ))
  {
    if (( info1 == NULL ) && ( info2 == NULL ))
      return TRUE;
    else
      return FALSE;
  }

  if ( ( strcmp(info1->name,info2->name) == 0 ) &&
       ( strcmp(info1->filename,info2->filename) == 0 ) &&
       ( strcmp(info1->phys,info2->phys) == 0 ) &&
       ( info1->is_custom == info2->is_custom ) )
    return TRUE;
  else
    return FALSE;
}


gint
ed_device_start_listening ( ed_device_t * event_device )
{
  if ( g_list_find( ed_device_listening_list , event_device ) != NULL )
  {
    DEBUGMSG( "called start listening for device \"%s\" ( %s - %s ) but device listening is already active!\n" ,
               event_device->info->name , event_device->info->filename , event_device->info->phys );
    return -1; /* device listening is already active, do nothing */
  }
  else
  {
    DEBUGMSG( "start listening for device \"%s\" ( %s - %s )\n" ,
               event_device->info->name , event_device->info->filename , event_device->info->phys );
    /* add a watch that checks if there's data to read */
    event_device->iochan_sid = g_io_add_watch( event_device->iochan , G_IO_IN ,
      (GIOFunc)ed_device_giofunc , event_device );

    /* add event_device to the list */
    ed_device_listening_list = g_list_append( ed_device_listening_list , event_device );
    event_device->is_listening = TRUE;
    return 0;
  }
}


gint
ed_device_stop_listening ( ed_device_t * event_device )
{
  if ( ( g_list_find( ed_device_listening_list , event_device ) != NULL ) &&
       ( event_device->is_listening == TRUE ) )
  {
    DEBUGMSG( "stop listening for device \"%s\" ( %s - %s )\n" ,
               event_device->info->name , event_device->info->filename , event_device->info->phys );
    g_source_remove( event_device->iochan_sid );
    ed_device_listening_list = g_list_remove( ed_device_listening_list , event_device );
    event_device->is_listening = FALSE;
    return 0;
  }
  else
  {
    DEBUGMSG( "called stop listening for device \"%s\" ( %s - %s ) but device listening is not active!\n" ,
               event_device->info->name , event_device->info->filename , event_device->info->phys );
    return -1;
  }
}


gint
ed_device_stop_listening_from_info ( ed_device_info_t * info )
{
  GList *list_iter = ed_device_listening_list;
  while ( list_iter != NULL )
  {
    ed_device_t *dev = list_iter->data;
    if ( ed_device_info_check_equality( dev->info , info ) == TRUE )
    {
      ed_device_stop_listening( dev );
      return 0;
    }
    list_iter = g_list_next( list_iter );
  }
  return -1;
}


void
ed_device_stop_listening_all ( gboolean delete_bindings )
{
  /* convenience function that stops listening for all
     devices and also deletes bindings if requested */
  GList *list_iter = ed_device_listening_list;
  while ( list_iter != NULL )
  {
    ed_device_t *dev = list_iter->data;

    if (( delete_bindings == TRUE ) && ( dev->info->bindings != NULL ))
      ed_bindings_store_delete( dev->info->bindings );

    ed_device_delete( dev );

    list_iter = g_list_next( list_iter );
  }
}


gboolean
ed_device_check_listening_from_info ( ed_device_info_t * info )
{
  /* note: this must not alter the reg parameter of info */
  GList *list_iter = ed_device_listening_list;
  while ( list_iter != NULL )
  {
    ed_device_t *dev = list_iter->data;
    if ( ed_device_info_check_equality( dev->info , info ) == TRUE )
      return TRUE;
    list_iter = g_list_next( list_iter );
  }
  return FALSE;
}


static gboolean
ed_device_giofunc ( GIOChannel * iochan , GIOCondition cond , gpointer event_device )
{
  switch ( cond )
  {
    case G_IO_IN:
    {
      gsize rb = 0;
      struct input_event inputev;

      if ( g_io_channel_read_chars( iochan , (gchar*)&inputev ,
             sizeof(struct input_event) , &rb , NULL ) == G_IO_STATUS_NORMAL )
      {
        if ( rb == sizeof(struct input_event) )
        {
          gint action_code = -1;
          ed_device_t *dev = event_device;

          DEBUGMSG( "event (%d,%d,%d) intercepted for device \"%s\" ( %s - %s )\n" ,
            inputev.type , inputev.code , inputev.value ,
            dev->info->name , dev->info->filename , dev->info->phys );

          if ( dev->info->bindings != NULL )
          {
            ed_inputevent_t ev;
            ev.type = inputev.type;
            ev.code = inputev.code;
            ev.value = inputev.value;

            /* lookup event type/code/value in the binding tree for this device */
            if ( ed_bindings_store_lookup( dev->info->bindings , &ev , &action_code ) == TRUE )
            {
              /* this has been binded to an action, call the corresponding action */
              DEBUGMSG( "found action code %i for event (%d,%d,%d)\n" ,
                action_code , inputev.type , inputev.code , inputev.value );
              ed_action_call( action_code , NULL );
            }
          }
        }
      }
      break;
    }
    default:
      ;
  }

  return TRUE;
}


GList *
ed_device_get_list_from_system ( void )
{
  GIOChannel *iochan;
  gchar *buffer;
  gsize buffer_len;
  gint fd = -1;

  fd = g_open( "/proc/bus/input/devices" , O_RDONLY , 0 );
  if ( fd < 0 )
  {
    /* an error occurred */
    g_warning( _("event-device-plugin: unable to open /proc/bus/input/devices , automatic "
               "detection of event devices won't work.\n") );
    return NULL;
  }

  iochan = g_io_channel_unix_new( fd );
  if ( iochan == NULL )
  {
    /* an error occurred */
    g_warning( _("event-device-plugin: unable to open a io_channel for /proc/bus/input/devices , "
               "automatic detection of event devices won't work.\n") );
    close( fd );
    return NULL;
  }
  g_io_channel_set_encoding( iochan , "UTF-8" , NULL ); /* utf-8 text */

  if ( g_io_channel_read_to_end( iochan , &buffer , &buffer_len , NULL ) != G_IO_STATUS_NORMAL )
  {
    /* an error occurred */
    g_warning( _("event-device-plugin: an error occurred while reading /proc/bus/input/devices , "
               "automatic detection of event devices won't work.\n") );
    g_io_channel_shutdown( iochan , TRUE , NULL );
    g_io_channel_unref( iochan );
    close( fd );
    return NULL;
  }
  else
  {
    regex_t preg;
    gint search_offset = 0;
    GList *system_devices_list = NULL;

    /* we don't need these anymore */
    g_io_channel_shutdown( iochan , TRUE , NULL );
    g_io_channel_unref( iochan );
    close( fd );

    /* parse content of /proc/bus/input/devices */
    regcomp( &preg,
      "I:[^\n]*\nN: Name=\"([^\n]*)\"\nP: Phys=([^\n]*)\n([^\n]+\n)*H: Handlers=[^\n]*(event[0-9]+)[^\n]*\n" ,
      REG_ICASE | REG_EXTENDED );

    while ( search_offset > -1 )
    {
      size_t nmatch = 5;
      regmatch_t submatch[5];

      if ( regexec( &preg , &buffer[search_offset] , nmatch , submatch , 0 ) == 0 )
      {
        GString *device_name = NULL;
        GString *device_phys = NULL;
        GString *device_file = NULL;

        if ( submatch[1].rm_so != -1 ) /* check validity of name sub-expression */
        {
          device_name = g_string_new( "" );
          g_string_append_len( device_name ,
            &buffer[(search_offset + submatch[1].rm_so)] ,
            submatch[1].rm_eo - submatch[1].rm_so );
        }

        if ( submatch[2].rm_so != -1 ) /* check validity of physicalport sub-expression */
        {
          device_phys = g_string_new( "" );
          g_string_append_len( device_phys ,
            &buffer[(search_offset + submatch[2].rm_so)] ,
            submatch[2].rm_eo - submatch[2].rm_so );
        }

        if ( submatch[4].rm_so != -1 ) /* check validity of filename sub-expression */
        {
          device_file = g_string_new( "" );
          GString *device_test = g_string_new( "" );
          g_string_append_len( device_file ,
            &buffer[(search_offset + submatch[4].rm_so)] ,
            submatch[4].rm_eo - submatch[4].rm_so );

          /* let's check if the filename actually exists in /dev */
          g_string_printf( device_test , "/dev/input/%s" , (char*)device_file->str );
          if ( !g_file_test( device_test->str , G_FILE_TEST_EXISTS ) )
          {
            /* it doesn't exist, mark as invalid device by nullifying device_file*/
            g_warning( _("event-device-plugin: device %s not found in /dev/input , skipping.\n") , (char*)device_file );
            g_string_free( device_file , TRUE );
            device_file = NULL;
          }
          else
          {
            /* it does exist, mark as valid device by using the full path in device_file*/
            g_string_assign( device_file , device_test->str );
          }
          g_string_free( device_test , TRUE );
        }

        if (( device_name != NULL ) && ( device_phys != NULL ) && ( device_file != NULL ))
        {
          /* add item to the list */
          ed_device_info_t *info = ed_device_info_new(
            device_name->str , device_file->str , device_phys->str , 0 );
          info->reg = 0;
          DEBUGMSG( "device found, name:\"%s\" , file \"%s\" , phys \"%s\"\n" ,
            info->name , info->filename , info->phys );
          system_devices_list = g_list_append( system_devices_list , info );
        }

        if ( device_name != NULL )
          g_string_free( device_name , TRUE );
        if ( device_phys != NULL )
          g_string_free( device_phys , TRUE );
        if ( device_file != NULL )
          g_string_free( device_file , TRUE );

        search_offset += submatch[0].rm_eo; /* update offset for further search */
      }
      else
      {
        /* no more valid devices found */
        search_offset = -1;
      }
    }
    regfree( &preg );
    return system_devices_list;
  }
}


GList *
ed_device_get_list_from_config ( void )
{
  GKeyFile *keyfile = NULL;
  GList *config_devices_list = NULL;
  gboolean is_loaded = FALSE;
  gchar **device_names = NULL;
  gsize device_names_num = 0;
  gchar *config_pathfilename = NULL;
  gchar *config_datadir = NULL;
  gint i = 0;

  config_datadir = (gchar*)aud_util_get_localdir();
  config_pathfilename = g_build_filename( config_datadir , PLAYER_LOCALRC_FILE , NULL );
  g_free( config_datadir );
  keyfile = g_key_file_new();
  is_loaded = g_key_file_load_from_file( keyfile , config_pathfilename , G_KEY_FILE_NONE , NULL );
  g_free( config_pathfilename );

  if ( is_loaded != TRUE )
  {
    g_warning( _("event-device-plugin: unable to load config file %s , default settings will be used.\n") ,
               PLAYER_LOCALRC_FILE );
    g_key_file_free( keyfile );
    return NULL;
  }

  /* remove ___plugin___ group that contains plugin settings */
  g_key_file_remove_group( keyfile , "___plugin___" , NULL );

  /* the other groups are devices; check them and run active ones */
  device_names = g_key_file_get_groups( keyfile , &device_names_num );
  while ( device_names[i] != NULL )
  {
    gint device_is_custom = 0;
    gchar *device_file = NULL;
    gchar *device_phys = NULL;
    gboolean device_is_active = FALSE;
    gint result = 0;

    result = ed_util_get_data_from_keyfile(
               keyfile , device_names[i] ,
               ED_CONFIG_INFO_FILENAME , &device_file ,
               ED_CONFIG_INFO_PHYS , &device_phys ,
               ED_CONFIG_INFO_ISCUSTOM , &device_is_custom ,
               ED_CONFIG_INFO_ISACTIVE , &device_is_active ,
               ED_CONFIG_INFO_END );

    if ( result == 0 )
    {
      /* all information succesfully retrieved from config, create a ed_device_info_t */
      ed_device_info_t *info;

      info = ed_device_info_new( device_names[i] , device_file ,
                                 device_phys , device_is_custom );

      /* pick bindings for this device */
      info->bindings = ed_util_get_bindings_from_keyfile( keyfile , device_names[i] );
      info->is_active = device_is_active;

      /* add this device to the config list */
      config_devices_list = g_list_append( config_devices_list , info );
      /* free information from config file, info has its own copies */
      g_free( device_file ); g_free( device_phys );
    }
    else
    {
      g_warning( _("event-device-plugin: incomplete information in config file for device \"%s\""
                 " , skipping.\n") , device_names[i] );
    }

    i++; /* on with next */
  }

  g_strfreev( device_names );
  g_key_file_free( keyfile );
  return config_devices_list;
}


void
ed_device_free_list ( GList * system_devices_list )
{
  GList *list_iter = system_devices_list;
  while ( list_iter != NULL )
  {
    ed_device_info_delete( (ed_device_info_t*)list_iter->data );
    list_iter = g_list_next( list_iter );
  }
  g_list_free( system_devices_list );
  return;
}


void
ed_device_start_listening_from_config ( void )
{
  GKeyFile *keyfile = NULL;
  gboolean is_loaded = FALSE;
  gchar **device_names = NULL;
  gsize device_names_num = 0;
  gchar *config_pathfilename = NULL;
  gchar *config_datadir = NULL;
  GList *system_devices_list = NULL;
  gint i = 0;

  config_datadir = (gchar*)aud_util_get_localdir();
  config_pathfilename = g_build_filename( config_datadir , PLAYER_LOCALRC_FILE , NULL );
  g_free( config_datadir );
  keyfile = g_key_file_new();
  is_loaded = g_key_file_load_from_file( keyfile , config_pathfilename , G_KEY_FILE_NONE , NULL );
  g_free( config_pathfilename );

  if ( is_loaded != TRUE )
  {
    g_warning( _("event-device-plugin: unable to load config file %s , default settings will be used.\n") ,
               PLAYER_LOCALRC_FILE );
    g_key_file_free( keyfile );
    return;
  }

  system_devices_list = ed_device_get_list_from_system();

  /* remove ___plugin___ group that contains plugin settings */
  g_key_file_remove_group( keyfile , "___plugin___" , NULL );

  /* check available devices and run active ones */
  device_names = g_key_file_get_groups( keyfile , &device_names_num );
  while ( device_names[i] != NULL )
  {
    GError *gerr = NULL;
    gboolean is_active;

    is_active = g_key_file_get_boolean( keyfile , device_names[i] , "is_active" , &gerr );
    if ( gerr != NULL )
    {
      g_warning( _("event-device-plugin: configuration, unable to get is_active value for device \"%s\""
                 ", skipping it.\n") , device_names[i] );
      g_clear_error( &gerr );
    }

    if ( is_active == TRUE ) /* only care about active devices at this time, ignore others */
    {
      gint is_custom = 0;
      gchar *device_file = NULL;
      gchar *device_phys = NULL;
      gint result = 0;

      result = ed_util_get_data_from_keyfile(
                 keyfile , device_names[i] ,
                 ED_CONFIG_INFO_FILENAME , &device_file ,
                 ED_CONFIG_INFO_PHYS , &device_phys ,
                 ED_CONFIG_INFO_ISCUSTOM , &is_custom ,
                 ED_CONFIG_INFO_END );

      if ( result != 0 )
      {
        /* something wrong, skip this device */
        i++; continue;
      }

      /* unless this is a custom device, perform a device check */
      if ( is_custom != 1 )
      {
        /* not a custom device, check it against system_devices_list
           to see if its information should be updated or if it's not plugged at all */
        gint check_result = ed_device_check(
          system_devices_list , device_names[i] , &device_file , &device_phys );

        if ( check_result == ED_DEVCHECK_OK )
        {
          /* ok, we have an active not-custom device and it has been successfully
             checked too; create a ed_device_t item for it */
          ed_device_t *dev = ed_device_new ( device_names[i] , device_file , device_phys , 0 );
          g_free( device_file ); g_free( device_phys ); /* not needed anymore */
          if ( dev != NULL )
          {
            dev->info->bindings = ed_util_get_bindings_from_keyfile( keyfile , device_names[i] );
            ed_device_start_listening ( dev );
          }
        }

        /* note: if check_result == ED_DEVCHECK_ABSENT, we simply skip this device */
      }
      else
      {
        /* ok, we have an active custom device; create a ed_device_t item for it */
        ed_device_t *dev = ed_device_new ( device_names[i] , device_file , device_phys , 1 );
        g_free( device_file ); g_free( device_phys ); /* not needed anymore */
        if ( dev != NULL )
        {
          dev->info->bindings = ed_util_get_bindings_from_keyfile( keyfile , device_names[i] );
          ed_device_start_listening ( dev );
        }
      }
    }

    /* on with next device name */
    i++;
  }

  g_strfreev( device_names );
  ed_device_free_list( system_devices_list );
  g_key_file_free( keyfile );
  return;
}


/* this function checks that a given event device (with device_name,
   device_phys and device_file) exists in system_devices_list; device_phys
   and device_file must be dynamically-allocated string, they could
   be freed and reallocated if their information needs to be updated;
   it returns an integer, its value represents the performed operations */
gint
ed_device_check ( GList * system_devices_list ,
                  gchar * device_name ,
                  gchar ** device_file ,
                  gchar ** device_phys )
{
  /* first, search in the list for a device, named device_name,
     that has not been found in a previous ed_device_check
     made with the same system_devices_list (info->reg == 0) */
  GList *list_iter = system_devices_list;

  while ( list_iter != NULL )
  {
    ed_device_info_t *info = list_iter->data;

    if ( ( info->reg == 0 ) && ( strcmp( device_name , info->name ) == 0 ) )
    {
      /* found a device, check if it has the same physical address */
      if ( strcmp( *device_phys , info->phys ) == 0 )
      {
        /* good, same device name and same physical
           address; update device_file if necessary */
        if ( strcmp( *device_file , info->filename ) != 0 )
        {
          g_free( *device_file );
          *device_file = g_strdup( info->filename );
        }
        /* now mark it as "found" so it won't be searched in next
           ed_device_check made with the same system_devices_list*/
        info->reg = 1;
        /* everything done */
        return ED_DEVCHECK_OK;
      }
      else
      {
        /* device found, but physical address is not the one from *device_phys; try to
           search further in system_devices_list for a device with same name and address */
        GList *list_iter2 = g_list_next(list_iter);
        while ( list_iter2 != NULL )
        {
          ed_device_info_t *info2 = list_iter2->data;
          if ( ( info2->reg == 0 ) &&
               ( strcmp( device_name , info2->name ) == 0 ) &&
               ( strcmp( *device_phys , info2->phys ) == 0 ) )
          {
            /* found a device with the same name and address,
               so let's use it; update device_file if necessary */
            if ( strcmp( *device_file , info2->filename ) != 0 )
            {
              g_free( *device_file );
              *device_file = g_strdup( info2->filename );
            }
            /* now mark it as "found" so it won't be searched in next
               ed_device_check made with the same system_devices_list */
            info2->reg = 1;
            /* everything done */
            return ED_DEVCHECK_OK;
          }
          list_iter2 = g_list_next(list_iter2);
        }

        /* if we get to this point, it means that there isn't any device named
           device_name with physical address equal to *device_phys ; there is only
           one (or more) device named device_name but with different physical
           address; we'll use the first of those (alas the current content of info) */
        g_free( *device_phys ); /* free outdated device_phys */
        *device_phys = g_strdup( info->phys ); /* update it with the new one */

        /* update device_file if necessary */
        if ( strcmp( *device_file , info->filename ) != 0 )
        {
          g_free( *device_file );
          *device_file = g_strdup( info->filename );
        }

        /* now mark it as "found" so it won't be searched in next
           ed_device_check made with the same system_devices_list*/
        info->reg = 1;
        /* everything done */
        return ED_DEVCHECK_OK;
      }
    }

    list_iter = g_list_next(list_iter);
  }

  /* the entire system_devices_list was searched,
     but no device named device_name was found */
  return ED_DEVCHECK_ABSENT;
}



/* config */
static void
ed_config_save_from_list_bindings_foreach ( ed_inputevent_t * iev ,
                                            gint action_code ,
                                            gpointer keyfile ,
                                            gpointer info_gp )
{
  gint int_list[4];
  gchar *keyname;
  ed_device_info_t *info = info_gp;
  keyname = g_strdup_printf( "b%i" , info->reg );
  int_list[0] = action_code;
  int_list[1] = iev->type;
  int_list[2] = iev->code;
  int_list[3] = iev->value;
  g_key_file_set_integer_list( keyfile , info->name , keyname , int_list , 4 );
  g_free( keyname );
  info->reg++;
  return;
}

gint
ed_config_save_from_list ( GList * config_devices_list )
{
  GKeyFile *keyfile;
  GList *iter_list = NULL;
  gchar *keyfile_str = NULL;
  gsize keyfile_str_len = 0;
  GIOChannel *iochan;
  gchar *config_pathfilename = NULL;
  gchar *config_datadir = NULL;

  config_datadir = (gchar*)aud_util_get_localdir();
  config_pathfilename = g_build_filename( config_datadir , PLAYER_LOCALRC_FILE , NULL );

  keyfile = g_key_file_new();

  g_key_file_set_string( keyfile , "___plugin___" , "config_ver" , ED_VERSION_CONFIG );

  iter_list = config_devices_list;
  while ( iter_list != NULL )
  {
    ed_device_info_t *info = iter_list->data;
    g_key_file_set_string( keyfile , info->name , "filename" , info->filename );
    g_key_file_set_string( keyfile , info->name , "phys" , info->phys );
    g_key_file_set_boolean( keyfile , info->name , "is_active" , info->is_active );
    g_key_file_set_integer( keyfile , info->name , "is_custom" , info->is_custom );
    /* use the info->reg field as a counter to list actions */
    info->reg = 0; /* init the counter */
    if ( info->bindings != NULL )
      ed_bindings_store_foreach( info->bindings ,
         ed_config_save_from_list_bindings_foreach , keyfile , info );
    iter_list = g_list_next( iter_list );
  }

  keyfile_str = g_key_file_to_data( keyfile , &keyfile_str_len , NULL );
  if ( g_file_test( config_datadir , G_FILE_TEST_IS_DIR ) == TRUE )
  {
    iochan = g_io_channel_new_file( config_pathfilename , "w" , NULL );
    g_io_channel_set_encoding( iochan , "UTF-8" , NULL );
    g_io_channel_write_chars( iochan , keyfile_str , keyfile_str_len , NULL , NULL );
    g_io_channel_shutdown( iochan , TRUE , NULL );
    g_io_channel_unref( iochan );
  }
  else
  {
    g_warning( _("event-device-plugin: unable to access local directory %s , settings will not be saved.\n") ,
               config_datadir );
  }

  g_free( keyfile_str );
  g_free( config_datadir );
  g_key_file_free( keyfile );
  return 0;
}


/* utils */


/* this picks information from a keyfile, using device_name
   as group name; information must be requested by passing
   a ed_config_info_t value and a corresponding container;
   list of requested information must be terminated with
   ED_CONFIG_INFO_END; returns 0 if everything is found,
   returns negative values if some information is missing */
static gint
ed_util_get_data_from_keyfile( GKeyFile * keyfile , gchar * device_name , ... )
{
  GError *gerr = NULL;
  gboolean is_failed = FALSE;
  ed_config_info_t info_code = ED_CONFIG_INFO_END;
  GList *temp_stringstore = NULL;
  va_list ap;

  /* when we get a string value from g_key_file_get_string, we temporarily
     store its container in temp_stringstore; if subsequent information
     requests in the iteraton fails, we free the information in previous
     container and nullify its content;
     this way, user will get complete information (return value 0) or
     absent information (all string containers nullified, return value -1) */

  va_start( ap, device_name );

  while ( ( is_failed == FALSE ) &&
          ( ( info_code = va_arg( ap , ed_config_info_t ) ) != ED_CONFIG_INFO_END ) )
  {
    switch ( info_code )
    {
      case ED_CONFIG_INFO_FILENAME:
      {
        gchar **device_file = va_arg( ap , gchar ** );
        *device_file = g_key_file_get_string( keyfile , device_name , "filename" , &gerr );
        if ( gerr != NULL )
        {
           g_clear_error( &gerr );
           g_warning( _("event-device-plugin: configuration, unable to get filename value for device \"%s\""
                      ", skipping it.\n") , device_name );
           is_failed = TRUE;
        }
        else
          temp_stringstore = g_list_append( temp_stringstore , device_file );
        break;
      }

      case ED_CONFIG_INFO_PHYS:
      {
        gchar **device_phys = va_arg( ap , gchar ** );
        *device_phys = g_key_file_get_string( keyfile , device_name , "phys" , &gerr );
        if ( gerr != NULL )
        {
           g_clear_error( &gerr );
           g_warning( _("event-device-plugin: configuration, unable to get phys value for device \"%s\""
                      ", skipping it.\n") , device_name );
           is_failed = TRUE;
        }
        else
          temp_stringstore = g_list_append( temp_stringstore , device_phys );
        break;
      }

      case ED_CONFIG_INFO_ISCUSTOM:
      {
        gint *is_custom = va_arg( ap , gint * );
        *is_custom = g_key_file_get_integer( keyfile , device_name , "is_custom" , &gerr );
        if ( gerr != NULL )
        {
           g_clear_error( &gerr );
           g_warning( _("event-device-plugin: configuration, unable to get is_custom value for device \"%s\""
                      ", skipping it.\n") , device_name );
           is_failed = TRUE;
        }
        break;
      }

      case ED_CONFIG_INFO_ISACTIVE:
      {
        gboolean *is_active = va_arg( ap , gboolean * );
        *is_active = g_key_file_get_boolean( keyfile , device_name , "is_active" , &gerr );
        if ( gerr != NULL )
        {
           g_clear_error( &gerr );
           g_warning( _("event-device-plugin: configuration, unable to get is_active value for device \"%s\""
                      ", skipping it.\n") , device_name );
           is_failed = TRUE;
        }
        break;
      }

      default:
      {
        /* unexpected value in info_code, skipping */
        g_warning( _("event-device-plugin: configuration, unexpected value for device \"%s\""
                   ", skipping it.\n") , device_name );
        is_failed = TRUE;
      }
    }
  }

  va_end( ap );

  if ( is_failed == FALSE )
  {
    /* temp_stringstore is not needed anymore,
       do not change pointed containers */
    g_list_free( temp_stringstore );
    return 0;
  }
  else
  {
    /* temp_stringstore is not needed anymore,
       nullify pointed containers and free content */
    GList *list_iter = temp_stringstore;
    while ( list_iter != NULL )
    {
      gchar **container = list_iter->data;
      g_free( *container );
      *container = NULL;
      list_iter = g_list_next( list_iter );
    }
    g_list_free( temp_stringstore );
    return -1;
  }
}


/* this does just what its name says :) */
static gpointer
ed_util_get_bindings_from_keyfile( GKeyFile * keyfile , gchar * device_name )
{
  ed_inputevent_t *iev = g_malloc(sizeof(ed_inputevent_t));
  gpointer bindings = ed_bindings_store_new();
  gchar **keys;
  gint j = 0;

  /* now get bindings for this device */
  keys = g_key_file_get_keys( keyfile , device_name , NULL , NULL );
  while ( keys[j] != NULL )
  {
    /* in the config file, only bindings start with the 'b' character */
    if ( keys[j][0] == 'b' )
    {
      gsize ilist_len = 0;
      gint *ilist;
      ilist = g_key_file_get_integer_list( keyfile ,
                device_name ,  keys[j] , &ilist_len , NULL );
      if ( ilist_len > 3 )
      {
        gint action_code = (gint)ilist[0];
        iev->type = (guint)ilist[1];
        iev->code = (guint)ilist[2];
        iev->value = (gint)ilist[3];
        ed_bindings_store_insert( bindings , iev , action_code );
      }
      g_free( ilist );
    }
    j++;
  }

  g_strfreev( keys );
  g_free( iev );

  if ( ed_bindings_store_size( bindings ) == 0 )
  {
    ed_bindings_store_delete( bindings );
    bindings = NULL;
  }

  return bindings;
}
