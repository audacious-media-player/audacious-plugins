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
#include "aosd_ui.h"
#include "aosd_osd.h"
#include "aosd_cfg.h"
#include "aosd_trigger.h"
#include <audacious/i18n.h>

AUD_GENERAL_PLUGIN
(
    .name = "Audacious OSD",
    .init = aosd_init,
    .about = aosd_about,
    .configure = aosd_configure,
    .cleanup = aosd_cleanup
)

aosd_cfg_t * global_config = NULL;
gboolean plugin_is_active = FALSE;


/* ***************** */
/* plug-in functions */

gboolean aosd_init (void)
{
  plugin_is_active = TRUE;
  g_log_set_handler( NULL , G_LOG_LEVEL_WARNING , g_log_default_handler , NULL );

  global_config = aosd_cfg_new();
  aosd_cfg_load( global_config );

  aosd_osd_init( global_config->osd->misc.transparency_mode );

  aosd_trigger_start( &global_config->osd->trigger );

  return TRUE;
}


void
aosd_cleanup ( void )
{
  if ( plugin_is_active == TRUE )
  {
    aosd_trigger_stop( &global_config->osd->trigger );

    aosd_osd_shutdown();
    aosd_osd_cleanup();

    if ( global_config != NULL )
    {
      aosd_cfg_delete( global_config );
      global_config = NULL;
    }

    plugin_is_active = FALSE;
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
