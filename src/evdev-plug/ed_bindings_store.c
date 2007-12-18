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

#include "ed_bindings_store.h"
#include "ed_common.h"
#include <stdlib.h>


/* currently, the bindings store is implemented as a string-based Hash Table;
   however, a change of implementation would only require to re-write the
   public API defined in this source file;
   the rest of the code in EvDev-Plug doesn't care about the
   implementation at all, and access it in a transparent fashion */


gpointer
ed_bindings_store_new ( void )
{
  GHashTable *hashtable = g_hash_table_new_full( g_str_hash , g_str_equal , g_free , NULL );
  return hashtable;
}


/* associate a triplet ev_type:ev_code:ev_value
   (identifying an input event) to an action code in our bindings store;
   bindings store should keep its own copy of ed_inputevent_t objects! */
gint
ed_bindings_store_insert ( gpointer hashtable_gp ,
                           ed_inputevent_t * inputev ,
                           gint action_code )
{
  gchar *input_str;
  input_str = g_strdup_printf( "%i:%i:%i" , inputev->type , inputev->code , inputev->value );
  g_hash_table_insert( (GHashTable*)hashtable_gp , input_str , GINT_TO_POINTER(action_code) );

  DEBUGMSG( "storing code %i -> event: %i:%i:%i\n" ,
    action_code , inputev->type, inputev->code, inputev->value );
  return 0;
}


typedef struct
{
  ed_bindings_store_foreach_func callback;
  gpointer data1;
  gpointer data2;
}
hashfunc_foreach_container_t;

static void
hashfunc_foreach( gpointer key , gpointer value , gpointer container_gp )
{
  hashfunc_foreach_container_t *container = container_gp;
  ed_inputevent_t inputev;
  gchar **toks;

  /* convert key (it's a string in the "type:code:value" form) back to ed_inputevent_t object */
  toks = g_strsplit( (gchar*)key , ":" , 3 );
  inputev.type = atoi( toks[0] );
  inputev.code = atoi( toks[1] );
  inputev.value = atoi( toks[2] );
  g_strfreev( toks );

  container->callback( &inputev , GPOINTER_TO_INT(value) , container->data1 , container->data2 );
  return;
}

void
ed_bindings_store_foreach ( gpointer hashtable_gp ,
                            ed_bindings_store_foreach_func callback ,
                            gpointer user_data1 ,
                            gpointer user_data2 )
{
  hashfunc_foreach_container_t *container = g_malloc(sizeof(hashfunc_foreach_container_t));
  container->callback = callback;
  container->data1 = user_data1;
  container->data2 = user_data2;
  g_hash_table_foreach( (GHashTable*)hashtable_gp , hashfunc_foreach , container );
  g_free( container );
}


guint
ed_bindings_store_size ( gpointer hashtable_gp )
{
  return g_hash_table_size( (GHashTable*)hashtable_gp );
}


gboolean
ed_bindings_store_lookup( gpointer hashtable_gp ,
                          ed_inputevent_t * inputev ,
                          gint * action_code )
{
  gpointer p;
  gchar *input_str;

  input_str = g_strdup_printf( "%i:%i:%i" , inputev->type , inputev->code , inputev->value );

  if ( g_hash_table_lookup_extended( (GHashTable*)hashtable_gp , input_str , NULL , &p ) == TRUE )
  {
    *action_code = GPOINTER_TO_INT(p);
    g_free( input_str );
    return TRUE;
  }
  else
  {
    g_free( input_str );
    return FALSE;
  }
}


gint
ed_bindings_store_delete ( gpointer hashtable_gp )
{
  if ( hashtable_gp != NULL )
    g_hash_table_destroy( hashtable_gp );
  return 0;
}
