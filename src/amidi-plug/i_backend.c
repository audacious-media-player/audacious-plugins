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

#include <stdlib.h>

#include "i_common.h"
#include "i_configure.h"

/* GCC does not like casting func * * to void * *. */
static void * get_symbol (GModule * mod, const char * name)
{
    void * sym = NULL;
    g_module_symbol (mod, name, & sym);
    return sym;
}

bool_t i_str_has_pref_and_suff( const char *str , char *pref , char *suff )
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
    const char * backend_directory_entry = g_dir_read_name( backend_directory );
    while ( backend_directory_entry != NULL )
    {
      /* simple filename checking */
      if ( i_str_has_pref_and_suff( backend_directory_entry , "ap-" , ".so" ) == TRUE )
      {
        GModule * module;
        char * (*getapmoduleinfo)( char ** , char ** , char ** , int * );
        char * module_pathfilename = g_strjoin( "" , AMIDIPLUGBACKENDDIR , "/" ,
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
            amidiplug_sequencer_backend_name_t * mn = malloc(sizeof(amidiplug_sequencer_backend_name_t));
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
    free( mn->desc );
    free( mn->name );
    free( mn->longname );
    free( mn->filename );
    free( mn );
    backend_list = backend_list->next;
  }
  return;
}


amidiplug_sequencer_backend_t * i_backend_load (const char * module_name)
{
  SPRINTF (path, AMIDIPLUGBACKENDDIR "/ap-%s.so", module_name);
  GModule * module = g_module_open (path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);

  if (! module)
  {
    fprintf (stderr, "amidiplug: Unable to load backend \"%s\"\n", path);
    return NULL;
  }

  amidiplug_sequencer_backend_t * backend = malloc (sizeof (amidiplug_sequencer_backend_t));

  backend->gmodule = module;

  backend->init = get_symbol (module, "backend_init");
  backend->cleanup = get_symbol (module, "backend_cleanup");
  backend->audio_info_get = get_symbol (module, "audio_info_get");
  backend->audio_volume_get = get_symbol (module, "audio_volume_get");
  backend->audio_volume_set = get_symbol (module, "audio_volume_set");
  backend->seq_start = get_symbol (module, "sequencer_start");
  backend->seq_stop = get_symbol (module, "sequencer_stop");
  backend->seq_on = get_symbol (module, "sequencer_on");
  backend->seq_off = get_symbol (module, "sequencer_off");
  backend->seq_queue_tempo = get_symbol (module, "sequencer_queue_tempo");
  backend->seq_queue_start = get_symbol (module, "sequencer_queue_start");
  backend->seq_queue_stop = get_symbol (module, "sequencer_queue_stop");
  backend->seq_event_init = get_symbol (module, "sequencer_event_init");
  backend->seq_event_noteon = get_symbol (module, "sequencer_event_noteon");
  backend->seq_event_noteoff = get_symbol (module, "sequencer_event_noteoff");
  backend->seq_event_allnoteoff = get_symbol (module, "sequencer_event_allnoteoff");
  backend->seq_event_keypress = get_symbol (module, "sequencer_event_keypress");
  backend->seq_event_controller = get_symbol (module, "sequencer_event_controller");
  backend->seq_event_pgmchange = get_symbol (module, "sequencer_event_pgmchange");
  backend->seq_event_chanpress = get_symbol (module, "sequencer_event_chanpress");
  backend->seq_event_pitchbend = get_symbol (module, "sequencer_event_pitchbend");
  backend->seq_event_sysex = get_symbol (module, "sequencer_event_sysex");
  backend->seq_event_tempo = get_symbol (module, "sequencer_event_tempo");
  backend->seq_event_other = get_symbol (module, "sequencer_event_other");
  backend->seq_output = get_symbol (module, "sequencer_output");
  backend->seq_output_shut = get_symbol (module, "sequencer_output_shut");
  backend->seq_get_port_count = get_symbol (module, "sequencer_get_port_count");

  bool_t (* checkautonomousaudio) (void) = get_symbol (module, "audio_check_autonomous");
  backend->autonomous_audio = checkautonomousaudio ();

  backend->init (amidiplug_cfg_backend);

  return backend;
}


void i_backend_unload (amidiplug_sequencer_backend_t * backend)
{
  backend->cleanup ();
  g_module_close (backend->gmodule);
  free (backend);
}
