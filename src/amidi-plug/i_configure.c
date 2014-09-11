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

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/preferences.h>

#include "i_configure-fluidsynth.h"

int backend_settings_changed = FALSE;  /* atomic */

static bool_t override_gain = FALSE;
static float gain_setting = 0.2;
static bool_t override_polyphony = FALSE;
static int polyphony_setting = 256;
static bool_t override_reverb = FALSE;
static bool_t reverb_setting = TRUE;
static bool_t override_chorus = FALSE;
static bool_t chorus_setting = TRUE;

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
 {WIDGET_CHK_BTN, N_("Override default gain:"),
  .cfg_type = VALUE_BOOLEAN, .cfg = & override_gain, .callback = backend_change},
 {WIDGET_SPIN_BTN, .child = TRUE, .data = {.spin_btn = {0, 10, 0.1}},
  .cfg_type = VALUE_FLOAT, .cfg = & gain_setting, .callback = backend_change}};

static const PreferencesWidget polyphony_widgets[] = {
 {WIDGET_CHK_BTN, N_("Override default polyphony:"),
  .cfg_type = VALUE_BOOLEAN, .cfg = & override_polyphony, .callback = backend_change},
 {WIDGET_SPIN_BTN, .child = TRUE, .data = {.spin_btn = {16, 4096, 1}},
  .cfg_type = VALUE_INT, .cfg = & polyphony_setting, .callback = backend_change}};

static const PreferencesWidget reverb_widgets[] = {
 {WIDGET_CHK_BTN, N_("Override default reverb:"),
  .cfg_type = VALUE_BOOLEAN, .cfg = & override_reverb, .callback = backend_change},
 {WIDGET_CHK_BTN, N_("On"), .child = TRUE,
  .cfg_type = VALUE_BOOLEAN, .cfg = & reverb_setting, .callback = backend_change}};

static const PreferencesWidget chorus_widgets[] = {
 {WIDGET_CHK_BTN, N_("Override default chorus:"),
  .cfg_type = VALUE_BOOLEAN, .cfg = & override_chorus, .callback = backend_change},
 {WIDGET_CHK_BTN, N_("On"), .child = TRUE,
  .cfg_type = VALUE_BOOLEAN, .cfg = & chorus_setting, .callback = backend_change}};

static const PreferencesWidget amidiplug_widgets[] = {

 /* global settings */
 {WIDGET_LABEL, N_("<b>Playback</b>")},
 {WIDGET_SPIN_BTN, N_("Transpose:"), .data = {.spin_btn = {-20, 20, 1}},
  .cfg_type = VALUE_INT, .csect = "amidiplug", .cname = "ap_opts_transpose_value"},
 {WIDGET_SPIN_BTN, N_("Drum shift:"), .data = {.spin_btn = {0, 127, 1}},
  .cfg_type = VALUE_INT, .csect = "amidiplug", .cname = "ap_opts_drumshift_value"},

 /* backend settings */
 {WIDGET_LABEL, N_("<b>SoundFont</b>")},
 {WIDGET_CUSTOM, .data = {.populate = create_soundfont_list}},
 {WIDGET_LABEL, N_("<b>Synthesizer</b>")},
 {WIDGET_BOX, .data = {.box = {gain_widgets, ARRAY_LEN (gain_widgets), TRUE}}},
 {WIDGET_BOX, .data = {.box = {polyphony_widgets, ARRAY_LEN (polyphony_widgets), TRUE}}},
 {WIDGET_BOX, .data = {.box = {reverb_widgets, ARRAY_LEN (reverb_widgets), TRUE}}},
 {WIDGET_BOX, .data = {.box = {chorus_widgets, ARRAY_LEN (chorus_widgets), TRUE}}},
 {WIDGET_SPIN_BTN, N_("Sampling rate:"), .data = {.spin_btn = {22050, 96000, 1}},
  .cfg_type = VALUE_INT, .csect = "amidiplug", .cname = "fsyn_synth_samplerate",
  .callback = backend_change}};

const PluginPreferences amidiplug_prefs = {
 .init = get_values,
 .widgets = amidiplug_widgets,
 .n_widgets = ARRAY_LEN (amidiplug_widgets)};
