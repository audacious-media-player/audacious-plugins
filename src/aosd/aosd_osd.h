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

#ifndef _I_AOSD_OSD_H
#define _I_AOSD_OSD_H 1

#include "aosd_cfg.h"

int aosd_osd_display ( char * markup_string , aosd_cfg_t * cfg_osd , bool copy_cfg );
void aosd_osd_shutdown ( void );
void aosd_osd_init ( int transparency_mode ); /* to be called before any OSD usage */
void aosd_osd_cleanup ( void ); /* to be called when done with OSD usage */

int aosd_osd_check_composite_ext ( void );
int aosd_osd_check_composite_mgr ( void );

#endif /* !_I_AOSD_OSD_H */
