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

#ifndef _I_SI_CFG_H
#define _I_SI_CFG_H 1

#include "si_common.h"
#include <glib.h>


#define SI_CFG_RCLICK_MENU_AUD	0
#define SI_CFG_RCLICK_MENU_SMALL1	1
#define SI_CFG_RCLICK_MENU_SMALL2	2

#define SI_CFG_SCROLL_ACTION_VOLUME 0
#define SI_CFG_SCROLL_ACTION_SKIP 1

typedef struct
{
  gint rclick_menu;
  gint scroll_action;
  gint volume_delta;
}
si_cfg_t;

void si_cfg_load ( void );
void si_cfg_save ( void );


#endif /* !_I_SI_CFG_H */
