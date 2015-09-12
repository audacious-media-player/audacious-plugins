/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   Configuration dialog

   Programmed and designed by Matti 'ccr' Hamalainen <ccr@tnsp.org>
   (C) Copyright 1999-2009 Tecnic Software productions (TNSP)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "xmms-sid.h"
#include "xs_config.h"

#include <libaudcore/i18n.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

/*
 * Configuration specific stuff
 */
struct xs_cfg_t xs_cfg;

static constexpr const char * const defaults[] = {
    "audioChannels", aud::numeric_string<XS_CHN_STEREO>::str,
    "audioFrequency", aud::numeric_string<XS_AUDIO_FREQ>::str,
    "mos8580", "FALSE",
    "forceModel", "FALSE",
    "emulateFilters", "TRUE",
    "clockSpeed", aud::numeric_string<XS_CLOCK_PAL>::str,
    "forceSpeed", "FALSE",
    "playMaxTimeEnable", "FALSE",
    "playMaxTimeUnknown", "FALSE",
    "playMaxTime", "150",
    "playMinTimeEnable", "FALSE",
    "playMinTime", "15",
    "subAutoEnable", "TRUE",
    "subAutoMinOnly", "TRUE",
    "subAutoMinTime", "15",
    nullptr
};

static constexpr ComboItem clock_speeds[] = {
    {"NTSC", XS_CLOCK_NTSC},
    {"PAL", XS_CLOCK_PAL}
};

static constexpr PreferencesWidget widgets[] = {
    WidgetLabel(N_("<b>Output</b>")),
    WidgetSpin(N_("Channels:"),
        WidgetInt("sid", "audioChannels"),
        {1, 2, 1}),
    WidgetSpin(N_("Sample rate:"),
        WidgetInt("sid", "audioFrequency"),
        {8000, 96000, 25, N_("Hz")}),
    WidgetLabel(N_("<b>Emulation</b>")),
    WidgetCheck(N_("Emulate MOS 8580 (default: MOS 6581)"),
        WidgetBool("sid", "mos8580")),
    WidgetCheck(N_("Do not automatically select chip model"),
        WidgetBool("sid", "forceModel")),
    WidgetCheck(N_("Emulate filter"),
        WidgetBool("sid", "emulateFilters")),
    WidgetCombo(N_("Clock speed:"),
        WidgetInt("sid", "clockSpeed"),
        {{clock_speeds}}),
    WidgetCheck(N_("Do not automatically select clock speed"),
        WidgetBool("sid", "forceSpeed")),
    WidgetLabel(N_("<b>Playback time</b>")),
    WidgetCheck(N_("Set maximum playback time:"),
        WidgetBool("sid", "playMaxTimeEnable")),
    WidgetSpin(nullptr,
        WidgetInt("sid", "playMaxTime"),
        {5, 3600, 5, N_("seconds")},
        WIDGET_CHILD),
    WidgetCheck(N_("Use only when song length is unknown"),
        WidgetBool("sid", "playMaxTimeUnknown"),
        WIDGET_CHILD),
    WidgetCheck(N_("Set minimum playback time:"),
        WidgetBool("sid", "playMinTimeEnable")),
    WidgetSpin(nullptr,
        WidgetInt("sid", "playMinTime"),
        {5, 3600, 5, N_("seconds")},
        WIDGET_CHILD),
    WidgetLabel(N_("<b>Subtunes</b>")),
    WidgetCheck(N_("Enable subtunes"),
        WidgetBool("sid", "subAutoEnable")),
    WidgetCheck(N_("Ignore subtunes shorter than:"),
        WidgetBool("sid", "subAutoMinOnly")),
    WidgetSpin(nullptr,
        WidgetInt("sid", "subAutoMinTime"),
        {5, 3600, 5, N_("seconds")},
        WIDGET_CHILD),
    WidgetLabel(N_("<b>Note</b>")),
    WidgetLabel(N_("These settings will take effect when Audacious is restarted."))
};

const PluginPreferences sid_prefs = {{widgets}};

/* Reset/initialize the configuration
 */
void xs_init_configuration()
{
    aud_config_set_defaults("sid", defaults);

    /* Initialize values with sensible defaults */
    xs_cfg.audioChannels = aud_get_int("sid", "audioChannels");
    xs_cfg.audioFrequency = aud_get_int("sid", "audioFrequency");

    xs_cfg.mos8580 = aud_get_bool("sid", "mos8580");
    xs_cfg.forceModel = aud_get_bool("sid", "forceModel");

    /* Filter values */
    xs_cfg.emulateFilters = aud_get_bool("sid", "emulateFilters");

    xs_cfg.clockSpeed = aud_get_int("sid", "clockSpeed");
    xs_cfg.forceSpeed = aud_get_bool("sid", "forceSpeed");

    xs_cfg.playMaxTimeEnable = aud_get_bool("sid", "playMaxTimeEnable");
    xs_cfg.playMaxTimeUnknown = aud_get_bool("sid", "playMaxTimeUnknown");
    xs_cfg.playMaxTime = aud_get_int("sid", "playMaxTime");

    xs_cfg.playMinTimeEnable = aud_get_bool("sid", "playMinTimeEnable");
    xs_cfg.playMinTime = aud_get_int("sid", "playMinTime");

    xs_cfg.subAutoEnable = aud_get_bool("sid", "subAutoEnable");
    xs_cfg.subAutoMinOnly = aud_get_bool("sid", "subAutoMinOnly");
    xs_cfg.subAutoMinTime = aud_get_int("sid", "subAutoMinTime");
}
