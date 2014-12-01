/*
 * Audacious bs2b effect plugin
 * Copyright (C) 2009, Sebastian Pipping <sebastian@pipping.org>
 * Copyright (C) 2009, Tony Vroon <chainsaw@gentoo.org>
 * Copyright (C) 2010, John Lindgren <john.lindgren@tds.net>
 * Copyright (C) 2011, Michał Lipski <tallica@o2.pl>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

#include <bs2b.h>

class BS2BPlugin : public EffectPlugin
{
public:
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("Bauer Stereophonic-to-Binaural (BS2B)"),
        PACKAGE,
        nullptr,
        & prefs
    };

    constexpr BS2BPlugin () : EffectPlugin (info, 0, true) {}

    bool init ();
    void cleanup ();

    void start (int & channels, int & rate);
    Index<float> & process (Index<float> & data);
};

EXPORT BS2BPlugin aud_plugin_instance;

static t_bs2bdp bs2b = nullptr;
static int bs2b_channels;

const char * const BS2BPlugin::defaults[] = {
 "feed", "45",
 "fcut", "700",
 nullptr};

bool BS2BPlugin::init ()
{
    aud_config_set_defaults ("bs2b", defaults);
    bs2b = bs2b_open ();

    if (! bs2b)
        return false;

    bs2b_set_level_feed (bs2b, aud_get_int ("bs2b", "feed"));
    bs2b_set_level_fcut (bs2b, aud_get_int ("bs2b", "fcut"));

    return true;
}

void BS2BPlugin::cleanup ()
{
    bs2b_close (bs2b);
    bs2b = nullptr;
}

void BS2BPlugin::start (int & channels, int & rate)
{
    bs2b_channels = channels;
    bs2b_set_srate (bs2b, rate);
}

Index<float> & BS2BPlugin::process (Index<float> & data)
{
    if (bs2b_channels == 2)
        bs2b_cross_feed_f (bs2b, data.begin (), data.len () / 2);

    return data;
}

static void feed_value_changed ()
{
    bs2b_set_level_feed (bs2b, aud_get_int ("bs2b", "feed"));
}

static void fcut_value_changed ()
{
    bs2b_set_level_fcut (bs2b, aud_get_int ("bs2b", "fcut"));
}

static void set_preset (uint32_t preset)
{
    int feed = preset >> 16;
    int fcut = preset & 0xffff;

    aud_set_int ("bs2b", "feed", feed);
    aud_set_int ("bs2b", "fcut", fcut);

    bs2b_set_level_feed (bs2b, feed);
    bs2b_set_level_fcut (bs2b, fcut);

    hook_call ("bs2b preset loaded", nullptr);
}

static void set_default_preset ()
    { set_preset (BS2B_DEFAULT_CLEVEL); }
static void set_cmoy_preset ()
    { set_preset (BS2B_CMOY_CLEVEL); }
static void set_jmeier_preset ()
    { set_preset (BS2B_JMEIER_CLEVEL); }

static const PreferencesWidget preset_widgets[] = {
    WidgetLabel (N_("Presets:")),
    WidgetButton (N_("Default"), {set_default_preset}),
    WidgetButton ("C. Moy", {set_cmoy_preset}),
    WidgetButton ("J. Meier", {set_jmeier_preset})
};

const PreferencesWidget BS2BPlugin::widgets[] = {
    WidgetSpin (N_("Feed level:"),
        WidgetInt ("bs2b", "feed", feed_value_changed, "bs2b preset loaded"),
        {BS2B_MINFEED, BS2B_MAXFEED, 1, N_("x1/10 dB")}),
    WidgetSpin (N_("Cut frequency:"),
        WidgetInt ("bs2b", "fcut", fcut_value_changed, "bs2b preset loaded"),
        {BS2B_MINFCUT, BS2B_MAXFCUT, 1, N_("Hz")}),
    WidgetBox ({{preset_widgets}, true})
};

const PluginPreferences BS2BPlugin::prefs = {{widgets}};
