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

struct midievent_t;

void backend_init ();
void backend_cleanup ();
void backend_reset ();

void backend_audio_info (int *, int *, int *);
void backend_generate_audio (void * buf, int bufsize);

void seq_event_noteon (midievent_t *);
void seq_event_noteoff (midievent_t *);
void seq_event_allnoteoff (int);
void seq_event_keypress (midievent_t *);
void seq_event_controller (midievent_t *);
void seq_event_pgmchange (midievent_t *);
void seq_event_chanpress (midievent_t *);
void seq_event_pitchbend (midievent_t *);
void seq_event_sysex (midievent_t *);
void seq_event_tempo (midievent_t *);
void seq_event_other (midievent_t *);

#endif /* !_I_BACKEND_H */
