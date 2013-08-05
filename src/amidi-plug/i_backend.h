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

#ifndef _I_BACKEND_H
#define _I_BACKEND_H 1

#include <libaudcore/core.h>

struct amidiplug_cfg_backend_s;
struct midievent_s;

typedef struct
{
    int (*init) (struct amidiplug_cfg_backend_s *);
    int (*cleanup) (void);
    void (* prepare) (void);
    void (* reset) (void);
    int (*audio_info_get) (int *, int *, int *);
    int (*seq_event_noteon) (struct midievent_s *);
    int (*seq_event_noteoff) (struct midievent_s *);
    int (*seq_event_allnoteoff) (int);
    int (*seq_event_keypress) (struct midievent_s *);
    int (*seq_event_controller) (struct midievent_s *);
    int (*seq_event_pgmchange) (struct midievent_s *);
    int (*seq_event_chanpress) (struct midievent_s *);
    int (*seq_event_pitchbend) (struct midievent_s *);
    int (*seq_event_sysex) (struct midievent_s *);
    int (*seq_event_tempo) (struct midievent_s *);
    int (*seq_event_other) (struct midievent_s *);
    void (* generate_audio) (void * buf, int bufsize);
}
amidiplug_sequencer_backend_t;

amidiplug_sequencer_backend_t * i_backend_load (void);
void i_backend_unload (amidiplug_sequencer_backend_t * backend);

#endif /* !_I_BACKEND_H */
