/* Extra Stereo Plugin for Audacious
 * Written by Johan Levin, 1999
 * Modified by John Lindgren, 2009-2012 */

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

class ExtraStereo : public EffectPlugin
{
public:
    static const char about[];
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("Extra Stereo"),
        PACKAGE,
        about,
        & prefs
    };

    constexpr ExtraStereo () : EffectPlugin (info, 0, true) {}

    bool init ();

    void start (int & channels, int & rate);
    Index<float> & process (Index<float> & data);
};

EXPORT ExtraStereo aud_plugin_instance;

const char ExtraStereo::about[] =
 N_("Extra Stereo Plugin\n\n"
    "By Johan Levin, 1999");

const char * const ExtraStereo::defaults[] = {
 "intensity", "2.5",
 nullptr};

const PreferencesWidget ExtraStereo::widgets[] = {
    WidgetLabel (N_("<b>Extra Stereo</b>")),
    WidgetSpin (N_("Intensity:"),
        WidgetFloat ("extra_stereo", "intensity"),
        {0, 10, 0.1})
};

const PluginPreferences ExtraStereo::prefs = {{widgets}};

bool ExtraStereo::init ()
{
    aud_config_set_defaults ("extra_stereo", defaults);
    return true;
}

static int stereo_channels;

void ExtraStereo::start (int & channels, int & rate)
{
    stereo_channels = channels;
}

Index<float> & ExtraStereo::process(Index<float> & data)
{
    float value = aud_get_double ("extra_stereo", "intensity");
    float * f, * end;
    float center;

    if (stereo_channels != 2)
        return data;

    end = data.end ();

    for (f = data.begin (); f < end; f += 2)
    {
        center = (f[0] + f[1]) / 2;
        f[0] = center + (f[0] - center) * value;
        f[1] = center + (f[1] - center) * value;
    }

    return data;
}
