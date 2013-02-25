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

#include "config.h"

#include <audacious/i18n.h>
#include <audacious/misc.h>

#include "../i_configure.h"
#include "b-alsa.h"

/* sequencer instance */
static sequencer_client_t sc;
/* options */
static amidiplug_cfg_alsa_t * alsa_cfg;

int backend_info_get (char ** name, char ** longname, char ** desc, int * ppos)
{
    if (name != NULL)
        *name = g_strdup ("alsa");

    if (longname != NULL)
        *longname = g_strdup (_("ALSA Backend "));

    if (desc != NULL)
        *desc = g_strdup (_("This backend sends MIDI events to a group of user-chosen "
                             "ALSA sequencer ports. The ALSA sequencer interface is very "
                             "versatile, it can provide ports for audio cards hardware "
                             "synthesizers (i.e. emu10k1) but also for software synths, "
                             "external devices, etc.\n"
                             "This backend does not produce audio, MIDI events are handled "
                             "directly from devices/programs behind the ALSA ports; in example, "
                             "MIDI events sent to the hardware synth will be directly played.\n"
                             "Backend written by Giacomo Lozito."));

    if (ppos != NULL)
        *ppos = 1; /* preferred position in backend list */

    return 1;
}


int backend_init (amidiplug_cfg_backend_t * cfg)
{
    alsa_cfg = cfg->alsa;

    sc.seq = NULL;
    sc.client_port = 0;
    sc.queue = 0;
    sc.dest_port = NULL;
    sc.dest_port_num = 0;
    sc.queue_tempo = NULL;
    sc.is_start = FALSE;

    return 1;
}


int backend_cleanup (void)
{
    return 1;
}


int sequencer_get_port_count (void)
{
    return i_util_str_count (alsa_cfg->alsa_seq_wports, ':');
}


int sequencer_start (char * midi_fname)
{
    sc.is_start = TRUE;
    return 1; /* success */
}


int sequencer_stop (void)
{
    return 1; /* success */
}


/* activate sequencer client */
int sequencer_on (void)
{
    char * wports_str = alsa_cfg->alsa_seq_wports;

    if (!i_seq_open())
    {
        sc.seq = NULL;
        return 0;
    }

    if (!i_seq_port_create())
    {
        i_seq_close();
        sc.seq = NULL;
        return 0;
    }

    if (!i_seq_queue_create())
    {
        i_seq_close();
        sc.seq = NULL;
        return 0;
    }

    if ((sc.is_start == TRUE) && (wports_str))
    {
        sc.is_start = FALSE;
        i_seq_port_wparse (wports_str);
    }

    if (!i_seq_port_connect())
    {
        i_seq_queue_free();
        i_seq_close();
        sc.seq = NULL;
        return 0;
    }

    /* success */
    return 1;
}


/* shutdown sequencer client */
int sequencer_off (void)
{
    if (sc.seq)
    {
        i_seq_port_disconnect();
        i_seq_queue_free();
        i_seq_close();
        sc.seq = NULL;
        /* return 1 here */
        return 1;
    }

    /* return 2 if it was already freed */
    return 2;
}


/* queue set tempo */
int sequencer_queue_tempo (int tempo, int ppq)
{
    /* interpret and set tempo */
    snd_seq_queue_tempo_alloca (&sc.queue_tempo);
    snd_seq_queue_tempo_set_tempo (sc.queue_tempo, tempo);
    snd_seq_queue_tempo_set_ppq (sc.queue_tempo, ppq);

    if (snd_seq_set_queue_tempo (sc.seq, sc.queue, sc.queue_tempo) < 0)
    {
        g_warning ("Cannot set queue tempo (%u/%i)\n",
                   snd_seq_queue_tempo_get_tempo (sc.queue_tempo),
                   snd_seq_queue_tempo_get_ppq (sc.queue_tempo));
        return 0;
    }

    return 1;
}


int sequencer_queue_start (void)
{
    return snd_seq_start_queue (sc.seq, sc.queue, NULL);
}


int sequencer_queue_stop (void)
{
    return snd_seq_stop_queue (sc.seq, sc.queue, NULL);
}


int sequencer_event_init (void)
{
    /* common settings for all our events */
    snd_seq_ev_clear (&sc.ev);
    sc.ev.queue = sc.queue;
    sc.ev.source.port = 0;
    sc.ev.flags = SND_SEQ_TIME_STAMP_TICK;
    return 1;
}


int sequencer_event_noteon (midievent_t * event)
{
    i_seq_event_common_init (event);
    snd_seq_ev_set_fixed (&sc.ev);
    sc.ev.data.note.channel = event->data.d[0];
    sc.ev.data.note.note = event->data.d[1];
    sc.ev.data.note.velocity = event->data.d[2];
    return 1;
}


int sequencer_event_noteoff (midievent_t * event)
{
    i_seq_event_common_init (event);
    snd_seq_ev_set_fixed (&sc.ev);
    sc.ev.data.note.channel = event->data.d[0];
    sc.ev.data.note.note = event->data.d[1];
    sc.ev.data.note.velocity = event->data.d[2];
    return 1;
}


int sequencer_event_keypress (midievent_t * event)
{
    i_seq_event_common_init (event);
    snd_seq_ev_set_fixed (&sc.ev);
    sc.ev.data.note.channel = event->data.d[0];
    sc.ev.data.note.note = event->data.d[1];
    sc.ev.data.note.velocity = event->data.d[2];
    return 1;
}


int sequencer_event_controller (midievent_t * event)
{
    i_seq_event_common_init (event);
    snd_seq_ev_set_fixed (&sc.ev);
    sc.ev.data.control.channel = event->data.d[0];
    sc.ev.data.control.param = event->data.d[1];
    sc.ev.data.control.value = event->data.d[2];
    return 1;
}


int sequencer_event_pgmchange (midievent_t * event)
{
    i_seq_event_common_init (event);
    snd_seq_ev_set_fixed (&sc.ev);
    sc.ev.data.control.channel = event->data.d[0];
    sc.ev.data.control.value = event->data.d[1];
    return 1;
}


int sequencer_event_chanpress (midievent_t * event)
{
    i_seq_event_common_init (event);
    snd_seq_ev_set_fixed (&sc.ev);
    sc.ev.data.control.channel = event->data.d[0];
    sc.ev.data.control.value = event->data.d[1];
    return 1;
}


int sequencer_event_pitchbend (midievent_t * event)
{
    i_seq_event_common_init (event);
    snd_seq_ev_set_fixed (&sc.ev);
    sc.ev.data.control.channel = event->data.d[0];
    sc.ev.data.control.value = ((event->data.d[1]) | ((event->data.d[2]) << 7)) - 0x2000;
    return 1;
}


int sequencer_event_sysex (midievent_t * event)
{
    i_seq_event_common_init (event);
    snd_seq_ev_set_variable (&sc.ev, event->data.length, event->sysex);
    return 1;
}


int sequencer_event_tempo (midievent_t * event)
{
    i_seq_event_common_init (event);
    snd_seq_ev_set_fixed (&sc.ev);
    sc.ev.dest.client = SND_SEQ_CLIENT_SYSTEM;
    sc.ev.dest.port = SND_SEQ_PORT_SYSTEM_TIMER;
    sc.ev.data.queue.queue = sc.queue;
    sc.ev.data.queue.param.value = event->data.tempo;
    return 1;
}


int sequencer_event_other (midievent_t * event)
{
    /* unhandled */
    return 1;
}


int sequencer_event_allnoteoff (int unused)
{
    int i = 0, c = 0;
    /* send "ALL SOUNDS OFF" to all channels on all ports */
    sc.ev.type = SND_SEQ_EVENT_CONTROLLER;
    sc.ev.time.tick = 0;
    snd_seq_ev_set_fixed (&sc.ev);
    sc.ev.data.control.param = MIDI_CTL_ALL_SOUNDS_OFF;
    sc.ev.data.control.value = 0;

    for (i = 0 ; i < sc.dest_port_num ; i++)
    {
        sc.ev.queue = sc.queue;
        sc.ev.dest = sc.dest_port[i];

        for (c = 0 ; c < 16 ; c++)
        {
            sc.ev.data.control.channel = c;
            snd_seq_event_output (sc.seq, &sc.ev);
            snd_seq_drain_output (sc.seq);
        }
    }

    return 1;
}


int sequencer_output (void * * buffer, int * len)
{
    snd_seq_event_output (sc.seq, &sc.ev);
    snd_seq_drain_output (sc.seq);
    snd_seq_sync_output_queue (sc.seq);
    return 0;
}


int sequencer_output_shut (unsigned max_tick, int skip_offset)
{
    g_return_val_if_fail (sc.seq != NULL, 0);

    int i = 0, c = 0;
    /* time to shutdown playback! */
    /* send "ALL SOUNDS OFF" to all channels on all ports */
    sc.ev.type = SND_SEQ_EVENT_CONTROLLER;
    sc.ev.time.tick = 0;
    snd_seq_ev_set_fixed (&sc.ev);
    sc.ev.data.control.param = MIDI_CTL_ALL_SOUNDS_OFF;
    sc.ev.data.control.value = 0;

    for (i = 0 ; i < sc.dest_port_num ; i++)
    {
        sc.ev.queue = sc.queue;
        sc.ev.dest = sc.dest_port[i];

        for (c = 0 ; c < 16 ; c++)
        {
            sc.ev.data.control.channel = c;
            snd_seq_event_output (sc.seq, &sc.ev);
            snd_seq_drain_output (sc.seq);
        }
    }

    /* schedule queue stop at end of song */
    snd_seq_ev_clear (&sc.ev);
    sc.ev.queue = sc.queue;
    sc.ev.source.port = 0;
    sc.ev.flags = SND_SEQ_TIME_STAMP_TICK;

    snd_seq_ev_set_fixed (&sc.ev);
    sc.ev.type = SND_SEQ_EVENT_STOP;
    sc.ev.time.tick = max_tick - skip_offset;
    sc.ev.dest.client = SND_SEQ_CLIENT_SYSTEM;
    sc.ev.dest.port = SND_SEQ_PORT_SYSTEM_TIMER;
    sc.ev.data.queue.queue = sc.queue;
    snd_seq_event_output (sc.seq, &sc.ev);
    snd_seq_drain_output (sc.seq);
    /* snd_seq_sync_output_queue(sc.seq); */

    return 1;
}


int audio_volume_get (int * left_volume, int * right_volume)
{
    snd_mixer_t * mixer_h = NULL;
    snd_mixer_elem_t * mixer_elem = NULL;
    char mixer_card[10];
    snprintf (mixer_card, 8, "hw:%i", alsa_cfg->alsa_mixer_card_id);
    mixer_card[9] = '\0';

    if (snd_mixer_open (&mixer_h, 0) > -1)
        i_seq_mixer_find_selem (mixer_h, mixer_card,
                                alsa_cfg->alsa_mixer_ctl_name,
                                alsa_cfg->alsa_mixer_ctl_id,
                                &mixer_elem);
    else
        mixer_h = NULL;

    if ((mixer_elem) && (snd_mixer_selem_has_playback_volume (mixer_elem)))
    {
        glong pv_min, pv_max, pv_range;
        glong lc, rc;

        snd_mixer_selem_get_playback_volume_range (mixer_elem, &pv_min, &pv_max);
        pv_range = pv_max - pv_min;

        if (pv_range > 0)
        {
            if (snd_mixer_selem_is_playback_mono (mixer_elem))
            {
                snd_mixer_selem_get_playback_volume (mixer_elem, SND_MIXER_SCHN_MONO,
                                                     & lc);
                * left_volume = * right_volume = ((lc - pv_min) * 100 + pv_range / 2) /
                                                 pv_range;
            }
            else
            {
                snd_mixer_selem_get_playback_volume (mixer_elem, SND_MIXER_SCHN_FRONT_LEFT, &lc);
                * left_volume = ((lc - pv_min) * 100 + pv_range / 2) / pv_range;
                snd_mixer_selem_get_playback_volume (mixer_elem, SND_MIXER_SCHN_FRONT_RIGHT, &rc);
                * right_volume = ((rc - pv_min) * 100 + pv_range / 2) / pv_range;
            }
        }
    }

    if (mixer_h)
        snd_mixer_close (mixer_h);

    /* always return 1 here */
    return 1;
}


int audio_volume_set (int left_volume, int right_volume)
{
    snd_mixer_t * mixer_h = NULL;
    snd_mixer_elem_t * mixer_elem = NULL;
    char mixer_card[10];
    snprintf (mixer_card, 8, "hw:%i", alsa_cfg->alsa_mixer_card_id);
    mixer_card[9] = '\0';

    if (snd_mixer_open (&mixer_h, 0) > -1)
        i_seq_mixer_find_selem (mixer_h, mixer_card,
                                alsa_cfg->alsa_mixer_ctl_name,
                                alsa_cfg->alsa_mixer_ctl_id,
                                &mixer_elem);
    else
        mixer_h = NULL;

    if ((mixer_elem) && (snd_mixer_selem_has_playback_volume (mixer_elem)))
    {
        glong pv_min, pv_max, pv_range;

        snd_mixer_selem_get_playback_volume_range (mixer_elem, &pv_min, &pv_max);
        pv_range = pv_max - pv_min;

        if (pv_range > 0)
        {
            if (snd_mixer_selem_has_playback_channel (mixer_elem, SND_MIXER_SCHN_FRONT_LEFT))
                snd_mixer_selem_set_playback_volume (mixer_elem, SND_MIXER_SCHN_FRONT_LEFT,
                                                     pv_min + (left_volume * pv_range + 50) / 100);

            if (snd_mixer_selem_has_playback_channel (mixer_elem, SND_MIXER_SCHN_FRONT_RIGHT))
                snd_mixer_selem_set_playback_volume (mixer_elem, SND_MIXER_SCHN_FRONT_RIGHT,
                                                     pv_min + (right_volume * pv_range + 50) / 100);
        }
    }

    if (mixer_h)
        snd_mixer_close (mixer_h);

    /* always return 1 here */
    return 1;
}


int audio_info_get (int * channels, int * bitdepth, int * samplerate)
{
    /* not applicable for ALSA backend */
    *channels = -1;
    *bitdepth = -1;
    *samplerate = -1;
    return 0; /* not valid information */
}


bool_t audio_check_autonomous (void)
{
    return TRUE; /* ALSA deals directly with audio production */
}



/* ******************************************************************
   *** EXTRA FUNCTIONS **********************************************
   ****************************************************************** */


/* get a list of writable ALSA MIDI ports
   use the data_bucket_t here...
   bint[0] = client id, bint[1] = port id
   bcharp[0] = client name, bcharp[1] = port name
   bpointer[0] = (not used), bpointer[1] = (not used) */
GSList * sequencer_port_get_list (void)
{
    int err;
    snd_seq_t * pseq;
    err = snd_seq_open (&pseq, "default", SND_SEQ_OPEN_DUPLEX, 0);

    if (err < 0)
        return NULL;

    GSList * wports = NULL;
    snd_seq_client_info_t * cinfo;
    snd_seq_port_info_t * pinfo;

    snd_seq_client_info_alloca (&cinfo);
    snd_seq_port_info_alloca (&pinfo);

    snd_seq_client_info_set_client (cinfo, -1);

    while (snd_seq_query_next_client (pseq, cinfo) >= 0)
    {
        int client = snd_seq_client_info_get_client (cinfo);
        snd_seq_port_info_set_client (pinfo, client);
        snd_seq_port_info_set_port (pinfo, -1);

        while (snd_seq_query_next_port (pseq, pinfo) >= 0)
        {
            if ((snd_seq_port_info_get_capability (pinfo)
                    & (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE))
                    == (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE))
            {
                data_bucket_t * portinfo = (data_bucket_t *) malloc (sizeof (data_bucket_t));
                portinfo->bint[0] = snd_seq_port_info_get_client (pinfo);
                portinfo->bint[1] = snd_seq_port_info_get_port (pinfo);
                portinfo->bcharp[0] = g_strdup (snd_seq_client_info_get_name (cinfo));
                portinfo->bcharp[1] = g_strdup (snd_seq_port_info_get_name (pinfo));
                wports = g_slist_append (wports, portinfo);
            }
        }
    }

    /* snd_seq_port_info_free( pinfo );
       snd_seq_client_info_free( cinfo ); */
    snd_seq_close (pseq);
    return wports;
}


void sequencer_port_free_list (GSList * wports)
{
    GSList * start = wports;

    while (wports != NULL)
    {
        data_bucket_t * portinfo = wports->data;
        free ((void *) portinfo->bcharp[0]);
        free ((void *) portinfo->bcharp[1]);
        free (portinfo);
        wports = wports->next;
    }

    g_slist_free (start);
    return;
}


/* get a list of available sound cards and relative mixer controls;
   use the data_bucket_t here...
   bint[0] = card id, bint[1] = (not used)
   bcharp[0] = card name, bcharp[1] = (not used)
   bpointer[0] = list (GSList) of mixer controls on the card, bpointer[1] = (not used) */
GSList * alsa_card_get_list (void)
{
    int soundcard_id = -1;
    GSList * scards = NULL;

    snd_card_next (&soundcard_id);

    while (soundcard_id > -1)
    {
        /* card container */
        data_bucket_t * cardinfo = (data_bucket_t *) malloc (sizeof (data_bucket_t));
        cardinfo->bint[0] = soundcard_id;
        /* snd_card_get_name calls strdup on its own */
        snd_card_get_name (soundcard_id, &cardinfo->bcharp[0]);
        /* for each sound card, get a list of available mixer controls */
        cardinfo->bpointer[0] = i_seq_mixctl_get_list (soundcard_id);

        scards = g_slist_append (scards, cardinfo);
        snd_card_next (&soundcard_id);
    }

    return scards;
}


void alsa_card_free_list (GSList * scards)
{
    GSList * start = scards;

    while (scards != NULL)
    {
        data_bucket_t * cardinfo = scards->data;
        /* free the list of mixer controls for the sound card */
        i_seq_mixctl_free_list ((GSList *) cardinfo->bpointer[0]);
        free ((void *) cardinfo->bcharp[0]);
        free (cardinfo);
        scards = scards->next;
    }

    g_slist_free (start);
    return;
}



/* ******************************************************************
   *** INTERNALS ****************************************************
   ****************************************************************** */


/* create sequencer client */
int i_seq_open (void)
{
    int err;
    err = snd_seq_open (&sc.seq, "default", SND_SEQ_OPEN_DUPLEX, 0);

    if (err < 0)
        return 0;

    snd_seq_set_client_name (sc.seq, "amidi-plug");
    return 1;
}


/* free sequencer client */
int i_seq_close (void)
{
    if (snd_seq_close (sc.seq) < 0)
        return 0; /* fail */
    else
        return 1; /* success */
}


/* create queue */
int i_seq_queue_create (void)
{
    sc.queue = snd_seq_alloc_named_queue (sc.seq, "AMIDI-Plug");

    if (sc.queue < 0)
        return 0; /* fail */
    else
        return 1; /* success */
}


/* free queue */
int i_seq_queue_free (void)
{
    if (snd_seq_free_queue (sc.seq, sc.queue) < 0)
        return 0; /* fail */
    else
        return 1; /* success */
}


/* create sequencer port */
int i_seq_port_create (void)
{
    sc.client_port = snd_seq_create_simple_port (sc.seq, "AMIDI-Plug", 0,
                     SND_SEQ_PORT_TYPE_MIDI_GENERIC |
                     SND_SEQ_PORT_TYPE_APPLICATION);

    if (sc.client_port < 0)
        return 0; /* fail */
    else
        return 1; /* success */
}


/* port connection */
int i_seq_port_connect (void)
{
    int i = 0, err = 0;

    for (i = 0 ; i < sc.dest_port_num ; i++)
    {
        if (snd_seq_connect_to (sc.seq, sc.client_port,
                                sc.dest_port[i].client,
                                sc.dest_port[i].port) < 0)
            ++err;
    }

    /* if these values are equal, it means
       that all port connections failed */
    if (err == i)
        return 0; /* fail */
    else
        return 1; /* success */
}


/* port disconnection */
int i_seq_port_disconnect (void)
{
    int i = 0, err = 0;

    for (i = 0 ; i < sc.dest_port_num ; i++)
    {
        if (snd_seq_disconnect_to (sc.seq, sc.client_port,
                                   sc.dest_port[i].client,
                                   sc.dest_port[i].port) < 0)
            ++err;
    }

    /* if these values are equal, it means
       that all port disconnections failed */
    if (err == i)
        return 0; /* fail */
    else
        return 1; /* success */
}


/* parse writable ports */
int i_seq_port_wparse (char * wportlist)
{
    int i = 0, err = 0;
    char ** portstr = g_strsplit (wportlist, ",", 0);

    sc.dest_port_num = 0;

    /* fill sc.dest_port_num with the writable port number */
    while (portstr[sc.dest_port_num] != NULL)
        ++sc.dest_port_num;

    /* check if there is already an allocated array and free it */
    free (sc.dest_port);
    sc.dest_port = NULL;

    if (sc.dest_port_num > 0)
        /* allocate the array of writable ports */
        sc.dest_port = g_new0 (snd_seq_addr_t, sc.dest_port_num);

    for (i = 0 ; i < sc.dest_port_num ; i++)
    {
        if (snd_seq_parse_address (sc.seq, &sc.dest_port[i], portstr[i]) < 0)
            ++err;
    }

    g_strfreev (portstr);

    /* if these values are equal, it means
       that all port translations failed */
    if (err == i)
        return 0; /* fail */
    else
        return 1; /* success */
}


int i_seq_event_common_init (midievent_t * event)
{
    sc.ev.type = event->type;
    sc.ev.time.tick = event->tick_real;
    sc.ev.dest = sc.dest_port[event->port];
    return 1;
}


/* get a list of available mixer controls for a given sound card;
   use the data_bucket_t here...
   bint[0] = control id, bint[1] = (not used)
   bcharp[0] = control name, bcharp[1] = (not used)
   bpointer[0] = (not used), bpointer[1] = (not used) */
GSList * i_seq_mixctl_get_list (int soundcard_id)
{
    GSList * mixctls = NULL;
    snd_mixer_t * mixer_h;
    snd_mixer_selem_id_t * mixer_selem_id;
    snd_mixer_elem_t * mixer_elem;
    char card[10];

    snprintf (card, 8, "hw:%i", soundcard_id);
    card[9] = '\0';

    snd_mixer_selem_id_alloca (&mixer_selem_id);
    snd_mixer_open (&mixer_h, 0);
    snd_mixer_attach (mixer_h, card);
    snd_mixer_selem_register (mixer_h, NULL, NULL);
    snd_mixer_load (mixer_h);

    for (mixer_elem = snd_mixer_first_elem (mixer_h) ; mixer_elem ;
            mixer_elem = snd_mixer_elem_next (mixer_elem))
    {
        data_bucket_t * mixctlinfo = (data_bucket_t *) malloc (sizeof (data_bucket_t));
        snd_mixer_selem_get_id (mixer_elem, mixer_selem_id);
        mixctlinfo->bint[0] = snd_mixer_selem_id_get_index (mixer_selem_id);
        mixctlinfo->bcharp[0] = g_strdup (snd_mixer_selem_id_get_name (mixer_selem_id));
        mixctls = g_slist_append (mixctls, mixctlinfo);
    }

    snd_mixer_close (mixer_h);
    return mixctls;
}


void i_seq_mixctl_free_list (GSList * mixctls)
{
    GSList * start = mixctls;

    while (mixctls != NULL)
    {
        data_bucket_t * mixctlinfo = mixctls->data;
        free ((void *) mixctlinfo->bcharp[0]);
        free (mixctlinfo);
        mixctls = mixctls->next;
    }

    g_slist_free (start);
    return;
}


int i_seq_mixer_find_selem (snd_mixer_t * mixer_h, char * mixer_card,
                            char * mixer_control_name, int mixer_control_id,
                            snd_mixer_elem_t ** mixer_elem)
{
    snd_mixer_selem_id_t * mixer_selem_id = NULL;
    snd_mixer_selem_id_alloca (&mixer_selem_id);
    snd_mixer_selem_id_set_index (mixer_selem_id, mixer_control_id);
    snd_mixer_selem_id_set_name (mixer_selem_id, mixer_control_name);
    snd_mixer_attach (mixer_h, mixer_card);
    snd_mixer_selem_register (mixer_h, NULL, NULL);
    snd_mixer_load (mixer_h);
    /* assign the mixer element (can be NULL if there is no such element) */
    *mixer_elem = snd_mixer_find_selem (mixer_h, mixer_selem_id);
    /* always return 1 here */
    return 1;
}


/* count the number of occurrencies of a specific character 'c'
   in the string 'string' (it must be a null-terminated string) */
int i_util_str_count (char * string, char c)
{
    int i = 0, count = 0;

    while (string[i] != '\0')
    {
        if (string[i] == c)
            ++count;

        ++i;
    }

    return count;
}
