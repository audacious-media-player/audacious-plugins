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

#ifndef _I_SI_UI_H
#define _I_SI_UI_H 1

#include <glib.h>


void si_ui_statusicon_show( void );
void si_ui_statusicon_hide( void );
void si_ui_about_show( void );
void si_ui_statusicon_enable ( gboolean );
void si_ui_prefs_show ( void );


#endif /* !_I_SI_UI_H */
