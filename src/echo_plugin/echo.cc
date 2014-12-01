#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

#define MAX_DELAY 1000

static const char echo_about[] =
 N_("Echo Plugin\n"
    "By Johan Levin, 1999\n"
    "Surround echo by Carl van Schaik, 1999\n"
    "Updated for Audacious by William Pitcock and John Lindgren, 2010-2014");

static const char * const echo_defaults[] = {
 "delay", "500",
 "feedback", "50",
 "volume", "50",
 nullptr};

static const PreferencesWidget echo_widgets[] = {
    WidgetLabel (N_("<b>Echo</b>")),
    WidgetSpin (N_("Delay:"),
        WidgetInt ("echo_plugin", "delay"),
        {0, MAX_DELAY, 10, N_("ms")}),
    WidgetSpin (N_("Feedback:"),
        WidgetInt ("echo_plugin", "feedback"),
        {0, 100, 1, "%"}),
    WidgetSpin (N_("Volume:"),
        WidgetInt ("echo_plugin", "volume"),
        {0, 100, 1, "%"})
};

static const PluginPreferences echo_prefs = {{echo_widgets}};

class EchoPlugin : public EffectPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Echo"),
        PACKAGE,
        echo_about,
        & echo_prefs
    };

    constexpr EchoPlugin () : EffectPlugin (info, 0, true) {}

    bool init ();
    void cleanup ();

    void start (int & channels, int & rate);
    Index<float> & process (Index<float> & data);
};

EXPORT EchoPlugin aud_plugin_instance;

static Index<float> buffer;
static int w_ofs;

bool EchoPlugin::init ()
{
    aud_config_set_defaults ("echo_plugin", echo_defaults);
    return true;
}

void EchoPlugin::cleanup ()
{
    buffer.clear ();
}

static int echo_channels = 0;
static int echo_rate = 0;

void EchoPlugin::start (int & channels, int & rate)
{
    if (channels != echo_channels || rate != echo_rate)
    {
        echo_channels = channels;
        echo_rate = rate;

        buffer.resize (aud::rescale (MAX_DELAY, 1000, rate) * channels);
        buffer.erase (0, -1);

        w_ofs = 0;
    }
}

Index<float> & EchoPlugin::process (Index<float> & data)
{
    int delay = aud_get_int ("echo_plugin", "delay");
    float feedback = aud_get_int ("echo_plugin", "feedback") / 100.0f;
    float volume = aud_get_int ("echo_plugin", "volume") / 100.0f;

    int interval = aud::rescale (delay, 1000, echo_rate) * echo_channels;
    interval = aud::clamp (interval, 0, buffer.len ());  // sanity check

    int r_ofs = w_ofs - interval;
    if (r_ofs < 0)
        r_ofs += buffer.len ();

    float * end = data.end ();

    for (float * f = data.begin (); f < end; f++)
    {
        float in = * f;
        float buf = buffer[r_ofs];

        * f = in + buf * volume;
        buffer[w_ofs] = in + buf * feedback;

        r_ofs = (r_ofs + 1) % buffer.len ();
        w_ofs = (w_ofs + 1) % buffer.len ();
    }

    return data;
}
