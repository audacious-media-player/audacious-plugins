/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2006
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

#include "i_backend.h"

#include <stdlib.h>

#include "i_common.h"
#include "i_configure.h"

extern amidiplug_sequencer_backend_t fluidsynth_backend;

amidiplug_sequencer_backend_t * i_backend_load (void)
{
    amidiplug_sequencer_backend_t * backend = & fluidsynth_backend;

    backend->init (amidiplug_cfg_backend);

    return backend;
}

void i_backend_unload (amidiplug_sequencer_backend_t * backend)
{
    backend->cleanup ();
}
