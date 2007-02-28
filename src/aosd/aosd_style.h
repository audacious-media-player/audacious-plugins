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

#ifndef _I_AOSD_STYLE_H
#define _I_AOSD_STYLE_H 1

#undef PACKAGE
#define PACKAGE "audacious-plugins"

#include "aosd_common.h"
#include <glib.h>


/* decoration style public API */
void aosd_deco_style_get_codes_array ( gint ** , gint * );
void aosd_deco_style_get_padding ( gint , gint * , gint * , gint * , gint * );
const gchar * aosd_deco_style_get_desc ( gint );
gint aosd_deco_style_get_numcol ( gint );
void aosd_deco_style_render ( gint , gpointer , gpointer , gpointer ); /* opaque */

gint aosd_deco_style_get_first_code ( void );
gint aosd_deco_style_get_max_numcol ( void );


#endif /* !_I_AOSD_STYLE_H */
