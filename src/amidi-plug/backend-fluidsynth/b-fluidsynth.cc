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

#include <stdlib.h>
#include <string.h>

#include <fluidsynth.h>
#include <glib.h>

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>

#include "../i_backend.h"
#include "../i_configure.h"
#include "../i_midievent.h"

/* #define DEBUGMSG(...) fprintf (stderr, __VA_ARGS__) */
#define DEBUGMSG(...)

typedef struct
{
    fluid_settings_t * settings;
    fluid_synth_t * synth;

    GArray * soundfont_ids;
}
sequencer_client_t;

/* sequencer instance */
static sequencer_client_t sc;
/* options */

static void i_soundfont_load (void);

void backend_init (void)
{
    sc.soundfont_ids = g_array_new (FALSE, FALSE, sizeof (int));
    sc.settings = new_fluid_settings();

    fluid_settings_setnum (sc.settings, "synth.sample-rate",
     aud_get_int ("amidiplug", "fsyn_synth_samplerate"));

    int gain = aud_get_int ("amidiplug", "fsyn_synth_gain");
    int polyphony = aud_get_int ("amidiplug", "fsyn_synth_polyphony");
    int reverb = aud_get_int ("amidiplug", "fsyn_synth_reverb");
    int chorus = aud_get_int ("amidiplug", "fsyn_synth_chorus");

    if (gain != -1)
        fluid_settings_setnum (sc.settings, "synth.gain", gain / 10.0);

    if (polyphony != -1)
        fluid_settings_setint (sc.settings, "synth.polyphony", polyphony);

    if (reverb == 1)
        fluid_settings_setstr (sc.settings, "synth.reverb.active", "yes");
    else if (reverb == 0)
        fluid_settings_setstr (sc.settings, "synth.reverb.active", "no");

    if (chorus == 1)
        fluid_settings_setstr (sc.settings, "synth.chorus.active", "yes");
    else if (chorus == 0)
        fluid_settings_setstr (sc.settings, "synth.chorus.active", "no");

    sc.synth = new_fluid_synth (sc.settings);
}


void backend_cleanup (void)
{
    /* unload soundfonts */
    for (unsigned i = 0; i < sc.soundfont_ids->len; i ++)
        fluid_synth_sfunload (sc.synth, g_array_index (sc.soundfont_ids, int, i), 0);

    g_array_free (sc.soundfont_ids, TRUE);
    delete_fluid_synth (sc.synth);
    delete_fluid_settings (sc.settings);
}


void backend_prepare (void)
{
    /* soundfont loader, check if we should load soundfont on first midifile play */
    if (! sc.soundfont_ids->len)
        i_soundfont_load();
}

void backend_reset (void)
{
    fluid_synth_system_reset (sc.synth);  /* all notes off and channels reset */
}


void seq_event_noteon (midievent_t * event)
{
    fluid_synth_noteon (sc.synth,
                        event->data.d[0],
                        event->data.d[1],
                        event->data.d[2]);
}


void seq_event_noteoff (midievent_t * event)
{
    fluid_synth_noteoff (sc.synth,
                         event->data.d[0],
                         event->data.d[1]);
}


void seq_event_keypress (midievent_t * event)
{
    /* KEY PRESSURE events are not handled by FluidSynth sequencer? */
    DEBUGMSG ("KEYPRESS EVENT with FluidSynth backend (unhandled)\n");
}


void seq_event_controller (midievent_t * event)
{
    fluid_synth_cc (sc.synth,
                    event->data.d[0],
                    event->data.d[1],
                    event->data.d[2]);
}


void seq_event_pgmchange (midievent_t * event)
{
    fluid_synth_program_change (sc.synth,
                                event->data.d[0],
                                event->data.d[1]);
}


void seq_event_chanpress (midievent_t * event)
{
    /* CHANNEL PRESSURE events are not handled by FluidSynth sequencer? */
    DEBUGMSG ("CHANPRESS EVENT with FluidSynth backend (unhandled)\n");
}


void seq_event_pitchbend (midievent_t * event)
{
    int pb_value = (( (event->data.d[2]) & 0x7f) << 7) | ((event->data.d[1]) & 0x7f);
    fluid_synth_pitch_bend (sc.synth,
                            event->data.d[0],
                            pb_value);
}


void seq_event_sysex (midievent_t * event)
{
    DEBUGMSG ("SYSEX EVENT with FluidSynth backend (unhandled)\n");
}


void seq_event_tempo (midievent_t * event)
{
    /* unhandled */
}


void seq_event_other (midievent_t * event)
{
    /* unhandled */
}


void seq_event_allnoteoff (int unused)
{
    int c = 0;

    for (c = 0 ; c < 16 ; c++)
    {
        fluid_synth_cc (sc.synth, c, 123 /* all notes off */, 0);
    }
}


void backend_generate_audio (void * buf, int bufsize)
{
    fluid_synth_write_s16 (sc.synth, bufsize / 4, buf, 0, 2, buf, 1, 2);
}


void backend_audio_info (int * channels, int * bitdepth, int * samplerate)
{
    *channels = 2;
    *bitdepth = 16; /* always 16 bit, we use fluid_synth_write_s16() */
    *samplerate = aud_get_int ("amidiplug", "fsyn_synth_samplerate");
}


/* ******************************************************************
   *** INTERNALS ****************************************************
   ****************************************************************** */

static void i_soundfont_load (void)
{
    char * soundfont_file = aud_get_str ("amidiplug", "fsyn_soundfont_file");

    if (soundfont_file[0])
    {
        char ** sffiles = g_strsplit (soundfont_file, ";", 0);
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

    str_unref (soundfont_file);
}
