/*
 * Copyright (c) 2008 William Pitcock <nenolod@nenolod.net>
 * Copyright (c) 2010-2012 John Lindgren <john.lindgren@tds.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

static const char * const cryst_defaults[] = {
 "intensity", "1",
 nullptr};

static const PreferencesWidget cryst_widgets[] = {
    WidgetLabel (N_("<b>Crystalizer</b>")),
    WidgetSpin (N_("Intensity:"),
        WidgetFloat ("crystalizer", "intensity"),
        {0, 10, 0.1})
};

static const PluginPreferences cryst_prefs = {{cryst_widgets}};

class Crystalizer : public EffectPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Crystalizer"),
        PACKAGE,
        nullptr,
        & cryst_prefs
    };

    constexpr Crystalizer () : EffectPlugin (info, 0, true) {}

    bool init ();
    void cleanup ();

    void start (int & channels, int & rate);
    Index<float> & process (Index<float> & data);
    bool flush (bool force);
};

EXPORT Crystalizer aud_plugin_instance;

static int cryst_channels;
static Index<float> cryst_prev;

bool Crystalizer::init ()
{
    aud_config_set_defaults ("crystalizer", cryst_defaults);
    return true;
}

void Crystalizer::cleanup ()
{
    cryst_prev.clear ();
}

void Crystalizer::start (int & channels, int & rate)
{
    cryst_channels = channels;
    cryst_prev.resize (cryst_channels);
    cryst_prev.erase (0, cryst_channels);
}

Index<float> & Crystalizer::process (Index<float> & data)
{
    float value = aud_get_double ("crystalizer", "intensity");
    float * f = data.begin ();
    float * end = data.end ();

    while (f < end)
    {
        for (int channel = 0; channel < cryst_channels; channel ++)
        {
            float current = * f;
            * f ++ = current + (current - cryst_prev[channel]) * value;
            cryst_prev[channel] = current;
        }
    }

    return data;
}

bool Crystalizer::flush (bool force)
{
    cryst_prev.erase (0, cryst_channels);
    return true;
}
