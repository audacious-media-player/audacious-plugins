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

#ifndef _I_SI_H
#define _I_SI_H 1

#include "si_common.h"
#include <glib.h>
#include <audacious/plugin.h>

void si_init ( void );
void si_cleanup ( void );
void si_about ( void );

GeneralPlugin si_gp =
{
    NULL,					/* handle */
    NULL,					/* filename */
    -1,						/* session */
    "Status Icon " SI_VERSION_PLUGIN,		/* description */
    si_init,					/* init */
    si_about,					/* about */
    NULL,					/* configure */
    si_cleanup					/* cleanup */
};

#endif /* !_I_SI_H */
