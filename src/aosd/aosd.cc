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

#include <libaudcore/i18n.h>

#include "aosd.h"
#include "aosd_osd.h"
#include "aosd_cfg.h"
#include "aosd_trigger.h"

EXPORT AOSD aud_plugin_instance;

const char AOSD::about[] =
 N_("Audacious OSD\n"
    "http://www.develia.org/projects.php?p=audacious#aosd\n\n"
    "Written by Giacomo Lozito <james@develia.org>\n\n"
    "Based in part on Evan Martin's Ghosd library:\n"
    "http://neugierig.org/software/ghosd/");

aosd_cfg_t global_config = aosd_cfg_t ();


/* ***************** */
/* plug-in functions */

bool AOSD::init ()
{
  aosd_cfg_load (global_config);
  aosd_osd_init (global_config.misc.transparency_mode);
  aosd_trigger_start (global_config.trigger);
  return true;
}


void AOSD::cleanup ()
{
  aosd_trigger_stop (global_config.trigger);
  aosd_osd_shutdown ();
  aosd_osd_cleanup ();
  global_config = aosd_cfg_t ();
}
