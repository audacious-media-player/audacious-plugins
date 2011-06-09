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

#ifndef _I_AOSD_H
#define _I_AOSD_H 1

#undef PACKAGE
#define PACKAGE "audacious-plugins"

#include "aosd_common.h"
#include <glib.h>
#include <audacious/plugin.h>

gboolean aosd_init (void);
void aosd_cleanup ( void );
void aosd_configure ( void );
void aosd_about ( void );

#endif /* !_I_AOSD_H */
