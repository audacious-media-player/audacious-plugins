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

#include "i_backend.h"

/* GCC does not like casting func * * to void * *. */
static void * get_symbol (GModule * mod, const gchar * name)
{
    void * sym = NULL;
    g_module_symbol (mod, name, & sym);
    return sym;
}

gboolean i_str_has_pref_and_suff( const gchar *str , gchar *pref , gchar *suff )
{
  if ( (g_str_has_prefix( str , pref )) &&
       (g_str_has_suffix( str , suff )) )
    return TRUE;
  else
    return FALSE;
}

GSList * i_backend_list_lookup( void )
{
  GDir * backend_directory;
  GSList * backend_list = NULL;

  backend_directory = g_dir_open( AMIDIPLUGBACKENDDIR , 0 , NULL );
  if ( backend_directory != NULL )
  {
    const gchar * backend_directory_entry = g_dir_read_name( backend_directory );
    while ( backend_directory_entry != NULL )
    {
      /* simple filename checking */
      if ( i_str_has_pref_and_suff( backend_directory_entry , "ap-" , ".so" ) == TRUE )
      {
        GModule * module;
        gchar * (*getapmoduleinfo)( gchar ** , gchar ** , gchar ** , gint * );
        gchar * module_pathfilename = g_strjoin( "" , AMIDIPLUGBACKENDDIR , "/" ,
                                                 backend_directory_entry , NULL );
        /* seems to be a backend for amidi-plug , try to load it */
        module = g_module_open( module_pathfilename , G_MODULE_BIND_LOCAL );
        if ( module == NULL )
          g_warning( "Error loading module %s - %s\n" , module_pathfilename , g_module_error() );
        else
        {
          /* try to get the module name */
          if ((getapmoduleinfo = get_symbol (module , "backend_info_get")))
          {
            /* module name found, ok! add its name and filename to the list */
            amidiplug_sequencer_backend_name_t * mn = g_malloc(sizeof(amidiplug_sequencer_backend_name_t));
            /* name and desc dinamically allocated */
            getapmoduleinfo( &mn->name , &mn->longname , &mn->desc , &mn->ppos );
            mn->filename = g_strdup(module_pathfilename); /* dinamically allocated */
            DEBUGMSG( "Backend found and added in list, filename: %s and lname: %s\n" ,
                      mn->filename, mn->longname );
            backend_list = g_slist_append( backend_list , mn );
          }
          else
          {
            /* module name not found, this is not a backend for amidi-plug */
            g_warning( "File %s is not a backend for amidi-plug!\n" , module_pathfilename );
          }
          g_module_close( module );
        }
      }
      backend_directory_entry = g_dir_read_name( backend_directory );
    }
    g_dir_close( backend_directory );
  }
  else
    g_warning( "Unable to open the backend directory %s\n" , AMIDIPLUGBACKENDDIR );

  return backend_list;
}


void i_backend_list_free( GSList * backend_list )
{
  while ( backend_list != NULL )
  {
    amidiplug_sequencer_backend_name_t * mn = backend_list->data;
    g_free( mn->desc );
    g_free( mn->name );
    g_free( mn->longname );
    g_free( mn->filename );
    g_free( mn );
    backend_list = backend_list->next;
  }
  return;
}


gint i_backend_load( gchar * module_name )
{
  gchar * module_pathfilename = NULL;

  if (module_name && module_name[0])
  {
    module_pathfilename = g_strjoin ("", AMIDIPLUGBACKENDDIR, "/ap-", module_name, ".so", NULL);
    DEBUGMSG ("loading backend '%s'\n", module_pathfilename);
    backend.gmodule = g_module_open (module_pathfilename, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
  }
  else
    backend.gmodule = NULL;

  if ( backend.gmodule != NULL )
  {
    gchar * (*getapmoduleinfo)( gchar ** , gchar ** , gchar ** , gint * );
    gboolean (*checkautonomousaudio)( void );

    backend.init = get_symbol (backend.gmodule, "backend_init");
    backend.cleanup = get_symbol (backend.gmodule, "backend_cleanup");
    backend.audio_info_get = get_symbol (backend.gmodule, "audio_info_get");
    backend.audio_volume_get = get_symbol (backend.gmodule, "audio_volume_get");
    backend.audio_volume_set = get_symbol (backend.gmodule, "audio_volume_set");
    backend.seq_start = get_symbol (backend.gmodule, "sequencer_start");
    backend.seq_stop = get_symbol (backend.gmodule, "sequencer_stop");
    backend.seq_on = get_symbol (backend.gmodule, "sequencer_on");
    backend.seq_off = get_symbol (backend.gmodule, "sequencer_off");
    backend.seq_queue_tempo = get_symbol (backend.gmodule, "sequencer_queue_tempo");
    backend.seq_queue_start = get_symbol (backend.gmodule, "sequencer_queue_start");
    backend.seq_queue_stop = get_symbol (backend.gmodule, "sequencer_queue_stop");
    backend.seq_event_init = get_symbol (backend.gmodule, "sequencer_event_init");
    backend.seq_event_noteon = get_symbol (backend.gmodule, "sequencer_event_noteon");
    backend.seq_event_noteoff = get_symbol (backend.gmodule, "sequencer_event_noteoff");
    backend.seq_event_allnoteoff = get_symbol (backend.gmodule, "sequencer_event_allnoteoff");
    backend.seq_event_keypress = get_symbol (backend.gmodule, "sequencer_event_keypress");
    backend.seq_event_controller = get_symbol (backend.gmodule, "sequencer_event_controller");
    backend.seq_event_pgmchange = get_symbol (backend.gmodule, "sequencer_event_pgmchange");
    backend.seq_event_chanpress = get_symbol (backend.gmodule, "sequencer_event_chanpress");
    backend.seq_event_pitchbend = get_symbol (backend.gmodule, "sequencer_event_pitchbend");
    backend.seq_event_sysex = get_symbol (backend.gmodule, "sequencer_event_sysex");
    backend.seq_event_tempo = get_symbol (backend.gmodule, "sequencer_event_tempo");
    backend.seq_event_other = get_symbol (backend.gmodule, "sequencer_event_other");
    backend.seq_output = get_symbol (backend.gmodule, "sequencer_output");
    backend.seq_output_shut = get_symbol (backend.gmodule, "sequencer_output_shut");
    backend.seq_get_port_count = get_symbol (backend.gmodule, "sequencer_get_port_count");

    getapmoduleinfo = get_symbol (backend.gmodule, "backend_info_get");
    checkautonomousaudio = get_symbol (backend.gmodule, "audio_check_autonomous");

    getapmoduleinfo( &backend.name , NULL , NULL , NULL );
    backend.autonomous_audio = checkautonomousaudio();
    DEBUGMSG( "backend %s (name '%s') successfully loaded\n" , module_pathfilename , backend.name );
    backend.init( i_configure_cfg_get_file );
    g_free( module_pathfilename );
    return 1;
  }
  else
  {
    if (module_pathfilename)
    {
      g_warning ("unable to load backend '%s'\n", module_pathfilename);
      g_free (module_pathfilename);
    }

    backend.name = NULL;
    return 0;
  }
}


gint i_backend_unload( void )
{
  if ( backend.gmodule != NULL )
  {
    DEBUGMSG( "unloading backend '%s'\n" , backend.name );
    backend.cleanup();
    g_module_close( backend.gmodule );
    DEBUGMSG( "backend '%s' unloaded\n" , backend.name );
    g_free( backend.name );
    backend.gmodule = NULL;
    return 1;
  }
  else
    return 0;
}
