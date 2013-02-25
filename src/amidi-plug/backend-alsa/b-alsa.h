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

#ifndef _B_ALSA_H
#define _B_ALSA_H 1

#include <glib.h>
#include <alsa/asoundlib.h>
#include <libaudcore/core.h>

#include "../i_common.h"
#include "../i_midievent.h"


typedef struct
{
    snd_seq_t * seq;
    int client_port;
    int queue;

    snd_seq_addr_t * dest_port;
    int dest_port_num;

    snd_seq_queue_tempo_t * queue_tempo;

    snd_seq_event_t ev;

    bool_t is_start;
}
sequencer_client_t;

int i_seq_open (void);
int i_seq_close (void);
int i_seq_queue_create (void);
int i_seq_queue_free (void);
int i_seq_port_create (void);
int i_seq_port_connect (void);
int i_seq_port_disconnect (void);
int i_seq_port_wparse (char *);
int i_seq_event_common_init (midievent_t *);
GSList * i_seq_mixctl_get_list (int);
void i_seq_mixctl_free_list (GSList *);
int i_seq_mixer_find_selem (snd_mixer_t *, char *, char *, int, snd_mixer_elem_t **);
int i_util_str_count (char *, char);

#endif /* !_B_ALSA_H */
