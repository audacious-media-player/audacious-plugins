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

#include <glib.h>

#include <libaudcore/drct.h>
#include <libaudgui/libaudgui.h>

#include "statusicon.h"

void si_volume_change(gint value)
{
    gint vl, vr;
    aud_drct_get_volume(&vl, &vr);
    aud_drct_set_volume(CLAMP(vl + value, 0, 100), CLAMP(vr + value, 0, 100));
}
