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

#include "i_configure.h"

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/preferences.h>

#include "i_configure-fluidsynth.h"

bool backend_settings_changed = false;  /* atomic */

static bool override_gain = false;
static double gain_setting = 0.2;
static bool override_polyphony = false;
static int polyphony_setting = 256;
static bool override_reverb = false;
static bool reverb_setting = true;
static bool override_chorus = false;
static bool chorus_setting = true;

static void get_values ()
{
    int gain = aud_get_int ("amidiplug", "fsyn_synth_gain");
    int polyphony = aud_get_int ("amidiplug", "fsyn_synth_polyphony");
    int reverb = aud_get_int ("amidiplug", "fsyn_synth_reverb");
    int chorus = aud_get_int ("amidiplug", "fsyn_synth_chorus");

    if (gain != -1)
    {
        override_gain = true;
        gain_setting = gain / 10.0;
    }

    if (polyphony != -1)
    {
        override_polyphony = true;
        polyphony_setting = polyphony;
    }

    if (reverb != -1)
    {
        override_reverb = true;
        reverb_setting = reverb;
    }

    if (chorus != -1)
    {
        override_chorus = true;
        chorus_setting = chorus;
    }
}

static void set_values ()
{
    int gain = override_gain ? (int) (gain_setting * 10 + 0.5) : -1;
    int polyphony = override_polyphony ? polyphony_setting : -1;
    int reverb = override_reverb ? reverb_setting : -1;
    int chorus = override_chorus ? chorus_setting : -1;

    aud_set_int ("amidiplug", "fsyn_synth_gain", gain);
    aud_set_int ("amidiplug", "fsyn_synth_polyphony", polyphony);
    aud_set_int ("amidiplug", "fsyn_synth_reverb", reverb);
    aud_set_int ("amidiplug", "fsyn_synth_chorus", chorus);
}

static void backend_change ()
{
    set_values ();

    /* reset backend at beginning of next song to apply changes */
    __sync_bool_compare_and_swap (& backend_settings_changed, false, true);
}

static const PreferencesWidget gain_widgets[] = {
    WidgetCheck (N_("Override default gain:"),
        WidgetBool (override_gain, backend_change)),
    WidgetSpin (0, WidgetFloat (gain_setting, backend_change),
        {0, 10, 0.1},
        WIDGET_CHILD)
};

static const PreferencesWidget polyphony_widgets[] = {
    WidgetCheck (N_("Override default polyphony:"),
        WidgetBool (override_polyphony, backend_change)),
    WidgetSpin (0, WidgetInt (polyphony_setting, backend_change),
        {16, 4096, 1},
        WIDGET_CHILD)
};

static const PreferencesWidget reverb_widgets[] = {
    WidgetCheck (N_("Override default reverb:"),
        WidgetBool (override_reverb, backend_change)),
    WidgetCheck (N_("On"),
        WidgetBool (reverb_setting, backend_change),
        WIDGET_CHILD)
};

static const PreferencesWidget chorus_widgets[] = {
    WidgetCheck (N_("Override default chorus:"),
        WidgetBool (override_chorus, backend_change)),
    WidgetCheck (N_("On"),
        WidgetBool (chorus_setting, backend_change),
        WIDGET_CHILD)
};

static const PreferencesWidget amidiplug_widgets[] = {

    /* global settings */
    WidgetLabel (N_("<b>Playback</b>")),
    WidgetSpin (N_("Transpose:"),
        WidgetInt ("amidiplug", "ap_opts_transpose_value"),
        {-20, 20, 1, N_("semitones")}),
    WidgetSpin (N_("Drum shift:"),
        WidgetInt ("amidiplug", "ap_opts_drumshift_value"),
        {0, 127, 1, N_("note numbers")}),
    WidgetCheck (N_("Skip leading silence"),
        WidgetBool ("amidiplug", "skip_leading")),
    WidgetCheck (N_("Skip trailing silence"),
        WidgetBool ("amidiplug", "skip_trailing")),

    /* backend settings */
    WidgetLabel (N_("<b>SoundFont</b>")),
#ifdef USE_GTK
    WidgetCustomGTK (create_soundfont_list),
#endif
    WidgetLabel (N_("<b>Synthesizer</b>")),
    WidgetBox ({{gain_widgets}, true}),
    WidgetBox ({{polyphony_widgets}, true}),
    WidgetBox ({{reverb_widgets}, true}),
    WidgetBox ({{chorus_widgets}, true}),
    WidgetSpin (N_("Sample rate:"),
        WidgetInt ("amidiplug", "fsyn_synth_samplerate", backend_change),
        {22050, 96000, 1, N_("Hz")})
};

const PluginPreferences amidiplug_prefs = {
    {amidiplug_widgets},
    get_values
};
