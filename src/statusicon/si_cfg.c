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

#include "si_cfg.h"
#include "si_common.h"
#include <audacious/plugin.h>
#include <audacious/configdb.h>

si_cfg_t si_cfg;


void
si_cfg_load ( void )
{
  ConfigDb *cfgfile = bmp_cfg_db_open();

  if ( !bmp_cfg_db_get_int( cfgfile , "statusicon" ,
       "rclick_menu" , &(si_cfg.rclick_menu) ) )
    si_cfg.rclick_menu = SI_CFG_RCLICK_MENU_AUD;

  if ( !bmp_cfg_db_get_int( cfgfile , "statusicon" ,
       "scroll_action" , &(si_cfg.scroll_action) ) )
    si_cfg.scroll_action = SI_CFG_SCROLL_ACTION_VOLUME;
    
  if ( !bmp_cfg_db_get_bool( cfgfile , "statusicon" ,
       "mw_visib_prevstatus" , &(si_cfg.mw_visib_prevstatus) ) )
    si_cfg.mw_visib_prevstatus = FALSE;
  if ( !bmp_cfg_db_get_bool( cfgfile , "statusicon" ,
       "pw_visib_prevstatus" , &(si_cfg.pw_visib_prevstatus) ) )
    si_cfg.pw_visib_prevstatus = FALSE;
  if ( !bmp_cfg_db_get_bool( cfgfile , "statusicon" ,
       "ew_visib_prevstatus" , &(si_cfg.ew_visib_prevstatus) ) )
    si_cfg.ew_visib_prevstatus = FALSE;

  bmp_cfg_db_close( cfgfile );
  return;
}


void
si_cfg_save ( void )
{
  ConfigDb *cfgfile = bmp_cfg_db_open();

  bmp_cfg_db_set_int( cfgfile , "statusicon" ,
    "rclick_menu" , si_cfg.rclick_menu );
  
  bmp_cfg_db_set_int( cfgfile , "statusicon" ,
    "scroll_action" , si_cfg.scroll_action );
  
  bmp_cfg_db_set_bool( cfgfile , "statusicon" ,
    "mw_visib_prevstatus" , si_cfg.mw_visib_prevstatus );
  bmp_cfg_db_set_bool( cfgfile , "statusicon" ,
    "pw_visib_prevstatus" , si_cfg.pw_visib_prevstatus );
  bmp_cfg_db_set_bool( cfgfile , "statusicon" ,
    "ew_visib_prevstatus" , si_cfg.ew_visib_prevstatus );

  bmp_cfg_db_close( cfgfile );
  return;
}
