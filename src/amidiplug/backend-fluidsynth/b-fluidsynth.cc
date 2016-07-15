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

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/index.h>
#include <libaudcore/runtime.h>

#include "../i_backend.h"
#include "../i_configure.h"
#include "../i_midievent.h"

typedef struct
{
    fluid_settings_t * settings;
    fluid_synth_t * synth;

    Index<int> soundfont_ids;
}
sequencer_client_t;

/* sequencer instance */
static sequencer_client_t sc;
/* options */

static void i_soundfont_load ();

void backend_init ()
{
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

    /* load soundfonts */
    i_soundfont_load();
}


void backend_cleanup ()
{
    /* unload soundfonts */
    for (int id : sc.soundfont_ids)
        fluid_synth_sfunload (sc.synth, id, 0);

    sc.soundfont_ids.clear ();
    delete_fluid_synth (sc.synth);
    delete_fluid_settings (sc.settings);
}


void backend_reset ()
{
    fluid_synth_system_reset (sc.synth);  /* all notes off and channels reset */
}


void seq_event_noteon (midievent_t * event)
{
    fluid_synth_noteon (sc.synth,
                        event->d[0],
                        event->d[1],
                        event->d[2]);
}


void seq_event_noteoff (midievent_t * event)
{
    fluid_synth_noteoff (sc.synth,
                         event->d[0],
                         event->d[1]);
}


void seq_event_keypress (midievent_t * event)
{
    /* KEY PRESSURE events are not handled by FluidSynth sequencer? */
    AUDDBG ("KEYPRESS EVENT with FluidSynth backend (unhandled)\n");
}


void seq_event_controller (midievent_t * event)
{
    fluid_synth_cc (sc.synth,
                    event->d[0],
                    event->d[1],
                    event->d[2]);
}


void seq_event_pgmchange (midievent_t * event)
{
    fluid_synth_program_change (sc.synth,
                                event->d[0],
                                event->d[1]);
}


void seq_event_chanpress (midievent_t * event)
{
    /* CHANNEL PRESSURE events are not handled by FluidSynth sequencer? */
    AUDDBG ("CHANPRESS EVENT with FluidSynth backend (unhandled)\n");
}


void seq_event_pitchbend (midievent_t * event)
{
    int pb_value = (( (event->d[2]) & 0x7f) << 7) | ((event->d[1]) & 0x7f);
    fluid_synth_pitch_bend (sc.synth,
                            event->d[0],
                            pb_value);
}


void seq_event_sysex (midievent_t * event)
{
    AUDDBG ("SYSEX EVENT with FluidSynth backend (unhandled)\n");
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

static void i_soundfont_load ()
{
    String soundfont_file = aud_get_str ("amidiplug", "fsyn_soundfont_file");

    if (soundfont_file[0])
    {
        Index<String> sffiles = str_list_to_index (soundfont_file, ";");

        for (const char * sffile : sffiles)
        {
            AUDDBG ("loading soundfont %s\n", sffile);
            int sf_id = fluid_synth_sfload (sc.synth, sffile, 0);

            if (sf_id == -1)
                AUDWARN ("unable to load SoundFont file %s\n", sffile);
            else
            {
                AUDDBG ("soundfont %s successfully loaded\n", sffile);
                sc.soundfont_ids.append (sf_id);
            }
        }

        fluid_synth_system_reset (sc.synth);
    }
    else
        AUDWARN ("FluidSynth backend was selected, but no SoundFont has been specified\n");
}
