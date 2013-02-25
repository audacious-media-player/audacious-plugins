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

#ifndef _B_FLUIDSYNTH_H
#define _B_FLUIDSYNTH_H 1

#include <glib.h>
#include <fluidsynth.h>

#include "../i_common.h"
#include "../i_midievent.h"


typedef struct
{
    fluid_settings_t * settings;
    fluid_synth_t * synth;

    GArray * soundfont_ids;

    int ppq;
    gdouble cur_microsec_per_tick;
    unsigned tick_offset;

    unsigned sample_rate;
}
sequencer_client_t;


void i_sleep (unsigned);
void i_soundfont_load (void);

#endif /* !_B_FLUIDSYNTH_H */
