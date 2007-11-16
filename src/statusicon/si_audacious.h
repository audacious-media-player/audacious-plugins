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

#ifndef _I_SI_AUDACIOUS_H
#define _I_SI_AUDACIOUS_H 1

#include "si_common.h"
#include <glib.h>


#define SI_AUDACIOUS_PLAYBACK_CTRL_PREV	0
#define SI_AUDACIOUS_PLAYBACK_CTRL_PLAY	1
#define SI_AUDACIOUS_PLAYBACK_CTRL_PAUSE	2
#define SI_AUDACIOUS_PLAYBACK_CTRL_STOP	3
#define SI_AUDACIOUS_PLAYBACK_CTRL_NEXT	4
#define SI_AUDACIOUS_PLAYBACK_CTRL_EJECT	5

void si_audacious_toggle_visibility ( void );
void si_audacious_volume_change ( gint );
void si_audacious_playback_skip ( gint );
void si_audacious_playback_ctrl ( gpointer );
void si_audacious_quit ( void );
void si_audacious_toggle_playback ( void );


#endif /* !_I_SI_AUDACIOUS_H */
