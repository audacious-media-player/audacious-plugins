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

#include <pthread.h>
#include <string.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>

#include "../i_configure.h"
#include "b-fluidsynth.h"

/* sequencer instance */
static sequencer_client_t sc;
/* options */
static amidiplug_cfg_fsyn_t * fsyn_cfg;

static pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t timer_cond = PTHREAD_COND_INITIALIZER;
static gint64 timer; /* microseconds */

int backend_info_get (char ** name, char ** longname, char ** desc, int * ppos)
{
    if (name != NULL)
        *name = g_strdup ("fluidsynth");

    if (longname != NULL)
        *longname = g_strdup (_("FluidSynth Backend "));

    if (desc != NULL)
        *desc = g_strdup (_("This backend produces audio by sending MIDI events "
                             "to FluidSynth, a real-time software synthesizer based "
                             "on the SoundFont2 specification (www.fluidsynth.org).\n"
                             "Produced audio can be manipulated via player effect "
                             "plugins and is processed by chosen output plugin.\n"
                             "Backend written by Giacomo Lozito."));

    if (ppos != NULL)
        *ppos = 2; /* preferred position in backend list */

    return 1;
}


int backend_init (amidiplug_cfg_backend_t * cfg)
{
    fsyn_cfg = cfg->fsyn;

    sc.soundfont_ids = g_array_new (FALSE, FALSE, sizeof (int));
    sc.sample_rate = fsyn_cfg->fsyn_synth_samplerate;
    sc.settings = new_fluid_settings();

    fluid_settings_setnum (sc.settings, "synth.sample-rate", fsyn_cfg->fsyn_synth_samplerate);

    if (fsyn_cfg->fsyn_synth_gain != -1)
        fluid_settings_setnum (sc.settings, "synth.gain", (gdouble) fsyn_cfg->fsyn_synth_gain / 10);

    if (fsyn_cfg->fsyn_synth_polyphony != -1)
        fluid_settings_setint (sc.settings, "synth.polyphony", fsyn_cfg->fsyn_synth_polyphony);

    if (fsyn_cfg->fsyn_synth_reverb == 1)
        fluid_settings_setstr (sc.settings, "synth.reverb.active", "yes");
    else if (fsyn_cfg->fsyn_synth_reverb == 0)
        fluid_settings_setstr (sc.settings, "synth.reverb.active", "no");

    if (fsyn_cfg->fsyn_synth_chorus == 1)
        fluid_settings_setstr (sc.settings, "synth.chorus.active", "yes");
    else if (fsyn_cfg->fsyn_synth_chorus == 0)
        fluid_settings_setstr (sc.settings, "synth.chorus.active", "no");

    sc.synth = new_fluid_synth (sc.settings);

    /* soundfont loader, check if we should load soundfont on backend init */
    if (fsyn_cfg->fsyn_soundfont_load == 0)
        i_soundfont_load();

    return 1;
}


int backend_cleanup (void)
{
    if (sc.soundfont_ids->len > 0)
    {
        /* unload soundfonts */
        int i = 0;

        for (i = 0 ; i < sc.soundfont_ids->len ; i++)
            fluid_synth_sfunload (sc.synth, g_array_index (sc.soundfont_ids, int, i), 0);
    }

    g_array_free (sc.soundfont_ids, TRUE);
    delete_fluid_synth (sc.synth);
    delete_fluid_settings (sc.settings);

    return 1;
}


int sequencer_get_port_count (void)
{
    /* always return a single port here */
    return 1;
}


int sequencer_start (char * midi_fname)
{
    /* soundfont loader, check if we should load soundfont on first midifile play */
    if ((fsyn_cfg->fsyn_soundfont_load == 1) && (sc.soundfont_ids->len == 0))
        i_soundfont_load();

    return 1; /* success */
}


int sequencer_stop (void)
{
    return 1; /* success */
}


/* activate sequencer client */
int sequencer_on (void)
{
    sc.tick_offset = 0;

    return 1; /* success */
}


/* shutdown sequencer client */
int sequencer_off (void)
{
    return 1; /* success */
}


/* queue set tempo */
int sequencer_queue_tempo (int tempo, int ppq)
{
    sc.ppq = ppq;
    /* sc.cur_tick_per_sec = (gdouble)( ppq * 1000000 ) / (gdouble)tempo; */
    sc.cur_microsec_per_tick = (gdouble) tempo / (gdouble) ppq;
    return 1;
}


int sequencer_queue_start (void)
{
    pthread_mutex_lock (& timer_mutex);
    timer = 0;
    pthread_mutex_unlock (& timer_mutex);

    return 1;
}


int sequencer_queue_stop (void)
{
    return 1;
}


int sequencer_event_init (void)
{
    /* common settings for all our events */
    return 1;
}


int sequencer_event_noteon (midievent_t * event)
{
    i_sleep (event->tick_real);
    fluid_synth_noteon (sc.synth,
                        event->data.d[0],
                        event->data.d[1],
                        event->data.d[2]);
    return 1;
}


int sequencer_event_noteoff (midievent_t * event)
{
    i_sleep (event->tick_real);
    fluid_synth_noteoff (sc.synth,
                         event->data.d[0],
                         event->data.d[1]);
    return 1;
}


int sequencer_event_keypress (midievent_t * event)
{
    /* KEY PRESSURE events are not handled by FluidSynth sequencer? */
    DEBUGMSG ("KEYPRESS EVENT with FluidSynth backend (unhandled)\n");
    i_sleep (event->tick_real);
    return 1;
}


int sequencer_event_controller (midievent_t * event)
{
    i_sleep (event->tick_real);
    fluid_synth_cc (sc.synth,
                    event->data.d[0],
                    event->data.d[1],
                    event->data.d[2]);
    return 1;
}


int sequencer_event_pgmchange (midievent_t * event)
{
    i_sleep (event->tick_real);
    fluid_synth_program_change (sc.synth,
                                event->data.d[0],
                                event->data.d[1]);
    return 1;
}


int sequencer_event_chanpress (midievent_t * event)
{
    /* CHANNEL PRESSURE events are not handled by FluidSynth sequencer? */
    DEBUGMSG ("CHANPRESS EVENT with FluidSynth backend (unhandled)\n");
    i_sleep (event->tick_real);
    return 1;
}


int sequencer_event_pitchbend (midievent_t * event)
{
    int pb_value = (( (event->data.d[2]) & 0x7f) << 7) | ((event->data.d[1]) & 0x7f);
    i_sleep (event->tick_real);
    fluid_synth_pitch_bend (sc.synth,
                            event->data.d[0],
                            pb_value);
    return 1;
}


int sequencer_event_sysex (midievent_t * event)
{
    DEBUGMSG ("SYSEX EVENT with FluidSynth backend (unhandled)\n");
    i_sleep (event->tick_real);
    return 1;
}


int sequencer_event_tempo (midievent_t * event)
{
    i_sleep (event->tick_real);
    /* sc.cur_tick_per_sec = (gdouble)( sc.ppq * 1000000 ) / (gdouble)event->data.tempo; */
    sc.cur_microsec_per_tick = (gdouble) event->data.tempo / (gdouble) sc.ppq;
    sc.tick_offset = event->tick_real;

    pthread_mutex_lock (& timer_mutex);
    timer = 0;
    pthread_mutex_unlock (& timer_mutex);

    return 1;
}


int sequencer_event_other (midievent_t * event)
{
    /* unhandled */
    i_sleep (event->tick_real);
    return 1;
}


int sequencer_event_allnoteoff (int unused)
{
    int c = 0;

    for (c = 0 ; c < 16 ; c++)
    {
        fluid_synth_cc (sc.synth, c, 123 /* all notes off */, 0);
    }

    return 1;
}


int sequencer_output (void * * buffer, int * length)
{
    int frames = sc.sample_rate / 100;

    * buffer = g_realloc (* buffer, 4 * frames);
    * length = 4 * frames;
    fluid_synth_write_s16 (sc.synth, frames, * buffer, 0, 2, * buffer, 1, 2);

    pthread_mutex_lock (& timer_mutex);
    timer += 10000;
    pthread_cond_signal (& timer_cond);
    pthread_mutex_unlock (& timer_mutex);

    return 1;
}


int sequencer_output_shut (unsigned max_tick, int skip_offset)
{
    i_sleep (max_tick - skip_offset);
    fluid_synth_system_reset (sc.synth);  /* all notes off and channels reset */
    return 1;
}


/* unimplemented, for autonomous audio == FALSE volume is set by the
   output plugin mixer controls and is not handled by input plugins */
int audio_volume_get (int * left_volume, int * right_volume)
{
    return 0;
}
int audio_volume_set (int left_volume, int right_volume)
{
    return 0;
}


int audio_info_get (int * channels, int * bitdepth, int * samplerate)
{
    *channels = 2;
    *bitdepth = 16; /* always 16 bit, we use fluid_synth_write_s16() */
    *samplerate = fsyn_cfg->fsyn_synth_samplerate;
    return 1; /* valid information */
}


bool_t audio_check_autonomous (void)
{
    return FALSE; /* FluidSynth gives produced audio back to player */
}



/* ******************************************************************
   *** INTERNALS ****************************************************
   ****************************************************************** */


void i_sleep (unsigned tick)
{
    gdouble elapsed_tick_usecs = (gdouble) (tick - sc.tick_offset) * sc.cur_microsec_per_tick;

    pthread_mutex_lock (& timer_mutex);

    while (timer < elapsed_tick_usecs)
        pthread_cond_wait (& timer_cond, & timer_mutex);

    pthread_mutex_unlock (& timer_mutex);
}


void i_soundfont_load (void)
{
    if (strcmp (fsyn_cfg->fsyn_soundfont_file, ""))
    {
        char ** sffiles = g_strsplit (fsyn_cfg->fsyn_soundfont_file, ";", 0);
        int i = 0;

        while (sffiles[i] != NULL)
        {
            int sf_id = 0;
            DEBUGMSG ("loading soundfont %s\n", sffiles[i]);
            sf_id = fluid_synth_sfload (sc.synth, sffiles[i], 0);

            if (sf_id == -1)
            {
                g_warning ("unable to load SoundFont file %s\n", sffiles[i]);
            }
            else
            {
                DEBUGMSG ("soundfont %s successfully loaded\n", sffiles[i]);
                g_array_append_val (sc.soundfont_ids, sf_id);
            }

            i++;
        }

        g_strfreev (sffiles);

        fluid_synth_system_reset (sc.synth);
    }
    else
    {
        g_warning ("FluidSynth backend was selected, but no SoundFont has been specified\n");
    }
}


bool_t i_bounds_check (int value, int min, int max)
{
    if ((value >= min) && (value <= max))
        return TRUE;
    else
        return FALSE;
}
