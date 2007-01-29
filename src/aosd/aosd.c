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

#include "aosd.h"
#include "aosd_osd.h"
#include "aosd_cfg.h"
#include <audacious/input.h>
#include <audacious/playlist.h>
#include <audacious/strings.h>


static guint timeout_sid = 0;
static gchar *prev_title = NULL;
static gboolean was_playing = FALSE;
aosd_cfg_t * global_config = NULL;


gboolean
aosd_check_pl_change ( gpointer data )
{
  if ( ip_data.playing )
  {
    Playlist *active = playlist_get_active();
    gint pos = playlist_get_position(active);
    gchar *title = playlist_get_songtitle(active, pos);

    if   ( ( title != NULL ) &&
         ( (( prev_title != NULL ) && ( strcmp(title,prev_title) )) ||
           ( was_playing == FALSE ) ) )
    {
      /* string formatting is done here a.t.m. - TODO - improve this area */
      gchar *utf8_title = str_to_utf8( title );
      gchar *utf8_title_markup = g_markup_printf_escaped(
        "<span font_desc='%s'>%s</span>" , global_config->osd->text.fonts_name[0] , utf8_title );
      aosd_display( utf8_title_markup , global_config->osd , FALSE );
      g_free( utf8_title_markup );
      g_free( utf8_title );
    }

    if ( prev_title != NULL )
      g_free(prev_title);
    prev_title = g_strdup(title);

    g_free( title );
  }
  else
  {
    if ( prev_title != NULL )
      { g_free(prev_title); prev_title = NULL; }
  }

  was_playing = ip_data.playing;
  return TRUE;
}


/* ***************** */
/* plug-in functions */

GeneralPlugin *get_gplugin_info()
{
   return &aosd_gp;
}


void
aosd_init ( void )
{
  g_log_set_handler( NULL , G_LOG_LEVEL_WARNING , g_log_default_handler , NULL );

  global_config = aosd_cfg_new();
  aosd_cfg_load( global_config );

  timeout_sid = g_timeout_add( 500 , (GSourceFunc)aosd_check_pl_change , NULL );
  return;
}


void
aosd_cleanup ( void )
{
  if ( timeout_sid > 0 )
    g_source_remove( timeout_sid );

  if ( prev_title != NULL )
  {
    g_free(prev_title);
    prev_title = NULL;
  }

  aosd_shutdown();

  if ( global_config != NULL )
  {
    aosd_cfg_delete( global_config );
    global_config = NULL;
  }

  return;
}


void
aosd_configure ( void )
{
  /* create a new configuration object */
  aosd_cfg_t *cfg = aosd_cfg_new();
  /* fill it with information from config file */
  aosd_cfg_load( cfg );
  /* call the configuration UI */
  aosd_ui_configure( cfg );
  /* delete configuration object */
  aosd_cfg_delete( cfg );
  return;
}


void
aosd_about ( void )
{
  aosd_ui_about();
  return;
}
