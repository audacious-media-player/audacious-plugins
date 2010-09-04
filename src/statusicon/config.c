/*
* Status Icon Plugin for Audacious
*
* Copyright 2005-2007 Giacomo Lozito <james@develia.org>
* Copyright 2010 Michał Lipski <tallica@o2.pl>
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

#include <audacious/configdb.h>

#include "statusicon.h"

si_cfg_t si_cfg;

void si_cfg_load(void)
{
    mcs_handle_t *cfgfile = aud_cfg_db_open();

    if (!aud_cfg_db_get_int(cfgfile, "statusicon", "rclick_menu", &(si_cfg.rclick_menu)))
        si_cfg.rclick_menu = SI_CFG_RCLICK_MENU_SMALL1;

    if (!aud_cfg_db_get_int(cfgfile, "statusicon", "scroll_action", &(si_cfg.scroll_action)))
        si_cfg.scroll_action = SI_CFG_SCROLL_ACTION_VOLUME;

    if (!aud_cfg_db_get_int(cfgfile, "audacious", "mouse_wheel_change", &(si_cfg.volume_delta)))
        si_cfg.volume_delta = 5;

    if (!aud_cfg_db_get_bool(cfgfile, "statusicon", "disable_popup", &(si_cfg.disable_popup)))
        si_cfg.disable_popup = FALSE;

    aud_cfg_db_close(cfgfile);
}


void si_cfg_save(void)
{
    mcs_handle_t *cfgfile = aud_cfg_db_open();

    aud_cfg_db_set_int(cfgfile, "statusicon", "rclick_menu", si_cfg.rclick_menu);
    aud_cfg_db_set_int(cfgfile, "statusicon", "scroll_action", si_cfg.scroll_action);
    aud_cfg_db_set_bool(cfgfile, "statusicon", "disable_popup", si_cfg.disable_popup);
    aud_cfg_db_close(cfgfile);
}
