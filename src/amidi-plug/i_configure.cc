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

#include <glib.h>

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/preferences.h>

#include "i_configure-fluidsynth.h"

int backend_settings_changed = FALSE;  /* atomic */

static gboolean override_gain = FALSE;
static float gain_setting = 0.2;
static gboolean override_polyphony = FALSE;
static int polyphony_setting = 256;
static gboolean override_reverb = FALSE;
static gboolean reverb_setting = TRUE;
static gboolean override_chorus = FALSE;
static gboolean chorus_setting = TRUE;

static void get_values (void)
{
    int gain = aud_get_int ("amidiplug", "fsyn_synth_gain");
    int polyphony = aud_get_int ("amidiplug", "fsyn_synth_polyphony");
    int reverb = aud_get_int ("amidiplug", "fsyn_synth_reverb");
    int chorus = aud_get_int ("amidiplug", "fsyn_synth_chorus");

    if (gain != -1)
    {
        override_gain = TRUE;
        gain_setting = gain / 10.0;
    }

    if (polyphony != -1)
    {
        override_polyphony = TRUE;
        polyphony_setting = polyphony;
    }

    if (reverb != -1)
    {
        override_reverb = TRUE;
        reverb_setting = reverb;
    }

    if (chorus != -1)
    {
        override_chorus = TRUE;
        chorus_setting = chorus;
    }
}

static void set_values (void)
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

static void backend_change (void)
{
    set_values ();

    /* reset backend at beginning of next song to apply changes */
    g_atomic_int_set (& backend_settings_changed, TRUE);
}

static const PreferencesWidget gain_widgets[] = {
    WidgetCheck (N_("Override default gain:"),
        {VALUE_BOOLEAN, & override_gain, 0, 0, backend_change}),
    WidgetSpin (0, {VALUE_FLOAT, & gain_setting, 0, 0, backend_change},
        {0, 10, 0.1},
        WIDGET_CHILD)
};

static const PreferencesWidget polyphony_widgets[] = {
    WidgetCheck (N_("Override default polyphony:"),
        {VALUE_BOOLEAN, & override_polyphony, 0, 0, backend_change}),
    WidgetSpin (0, {VALUE_INT, & polyphony_setting, 0, 0, backend_change},
        {16, 4096, 1},
        WIDGET_CHILD)
};

static const PreferencesWidget reverb_widgets[] = {
    WidgetCheck (N_("Override default reverb:"),
        {VALUE_BOOLEAN, & override_reverb, 0, 0, backend_change}),
    WidgetCheck (N_("On"),
        {VALUE_BOOLEAN, & reverb_setting, 0, 0, backend_change},
        WIDGET_CHILD)
};

static const PreferencesWidget chorus_widgets[] = {
    WidgetCheck (N_("Override default chorus:"),
        {VALUE_BOOLEAN, & override_chorus, 0, 0, backend_change}),
    WidgetCheck (N_("On"),
        {VALUE_BOOLEAN, & chorus_setting, 0, 0, backend_change},
        WIDGET_CHILD)
};

static const PreferencesWidget amidiplug_widgets[] = {

    /* global settings */
    WidgetLabel (N_("<b>Playback</b>")),
    WidgetSpin (N_("Transpose:"),
        {VALUE_INT, 0, "amidiplug", "ap_opts_transpose_value"},
        {-20, 20, 1}),
    WidgetSpin (N_("Drum shift:"),
        {VALUE_INT, 0, "amidiplug", "ap_opts_drumshift_value"},
        {0, 127, 1}),
    WidgetLabel (N_("<b>Advanced</b>")),
    WidgetCheck (N_("Extract comments from MIDI file"),
        {VALUE_BOOLEAN, 0, "amidiplug", "ap_opts_comments_extract"}),
    WidgetCheck (N_("Extract lyrics from MIDI file"),
        {VALUE_BOOLEAN, 0, "amidiplug", "ap_opts_lyrics_extract"}),

    /* backend settings */
    WidgetLabel (N_("<b>SoundFont</b>")),
    WidgetCustom (create_soundfont_list),
    WidgetLabel (N_("<b>Synthesizer</b>")),
    WidgetBox ({gain_widgets, ARRAY_LEN (gain_widgets), TRUE}),
    WidgetBox ({polyphony_widgets, ARRAY_LEN (polyphony_widgets), TRUE}),
    WidgetBox ({reverb_widgets, ARRAY_LEN (reverb_widgets), TRUE}),
    WidgetBox ({chorus_widgets, ARRAY_LEN (chorus_widgets), TRUE}),
    WidgetSpin (N_("Sampling rate:"),
        {VALUE_INT, 0, "amidiplug", "fsyn_synth_samplerate", backend_change},
        {22050, 96000, 1})
};

const PluginPreferences amidiplug_prefs = {
    amidiplug_widgets,
    ARRAY_LEN (amidiplug_widgets),
    get_values
};

