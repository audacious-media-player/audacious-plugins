/*
 * Copyright (c) 2021 Ariadne Conill <ariadne@dereferenced.org>
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

#include <cmath>

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

static const char * const bitcrusher_defaults[] = {
 "depth", "32",
 "downsample", "1.0",
 nullptr};

static const PreferencesWidget bitcrusher_widgets[] = {
    WidgetLabel (N_("<b>Bitcrusher</b>")),
    WidgetSpin (N_("Bit Depth:"),
        WidgetFloat ("bitcrusher", "depth"),
        {2, 32, 0.1}),
    WidgetSpin (N_("Downsample ratio:"),
        WidgetFloat ("bitcrusher", "downsample"),
        {0.02, 1.0, 0.02}),
};

static const PluginPreferences bitcrusher_prefs = {{bitcrusher_widgets}};

class Bitcrusher : public EffectPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Bitcrusher"),
        PACKAGE,
        nullptr,
        & bitcrusher_prefs
    };

    constexpr Bitcrusher () : EffectPlugin (info, 0, true) {}

    bool init ();
    void cleanup ();

    void start (int & channels, int & rate);
    Index<float> & process (Index<float> & data);
    bool flush (bool force);

private:
    float m_accumulator = 0.0;
    int m_channels = 0;
    Index<float> m_hold;
};

EXPORT Bitcrusher aud_plugin_instance;

bool
Bitcrusher::init ()
{
    aud_config_set_defaults ("bitcrusher", bitcrusher_defaults);
    return true;
}

void
Bitcrusher::cleanup ()
{
    m_hold.clear ();
}

void
Bitcrusher::start (int & channels, int & rate)
{
    m_accumulator = 0.0f;
    m_channels = channels;

    m_hold.resize (m_channels);
    m_hold.erase (0, m_channels);
}

Index<float> &
Bitcrusher::process (Index<float> & data)
{
    float downsample_ratio = aud_get_double ("bitcrusher", "downsample");
    float bit_depth = aud_get_double ("bitcrusher", "depth");

    float scale = pow (2., bit_depth) / 2.;
    float gain = (33. - bit_depth) / 8.;

    float * f = data.begin ();
    float * end = data.end ();

    while (f < end)
    {
        m_accumulator += downsample_ratio;

        for (int channel = 0; channel < m_channels; channel ++)
        {
            float current = * f;

            if (m_accumulator >= 1.0)
                m_hold [channel] = floorf ((current * gain) * scale + 0.5) / scale / gain;

            * f ++ = m_hold [channel];
        }

        if (m_accumulator >= 1.0)
            m_accumulator -= 1.0;
    }

    return data;
}

bool
Bitcrusher::flush (bool force)
{
    m_hold.erase (0, m_channels);
    return true;
}
