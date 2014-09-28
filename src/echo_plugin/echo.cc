#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>

#define MAX_DELAY 1000
#define MAX_SRATE 50000
#define BYTES_PS sizeof(float)
#define BUFFER_SAMPLES (MAX_SRATE * MAX_DELAY / 1000)
#define BUFFER_SHORTS (BUFFER_SAMPLES * AUD_MAX_CHANNELS)
#define BUFFER_BYTES (BUFFER_SHORTS * BYTES_PS)

static const char echo_about[] =
 N_("Echo Plugin\n"
    "By Johan Levin, 1999\n\n"
    "Surround echo by Carl van Schaik, 1999");

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

    void start (int * channels, int * rate);
    void process (float * * data, int * samples);
};

EXPORT EchoPlugin aud_plugin_instance;

static float *buffer = nullptr;
static int w_ofs;

bool EchoPlugin::init ()
{
    aud_config_set_defaults ("echo_plugin", echo_defaults);
    return true;
}

void EchoPlugin::cleanup ()
{
    g_free(buffer);
    buffer = nullptr;
}

static int echo_channels = 0;
static int echo_rate = 0;

void EchoPlugin::start (int * channels, int * rate)
{
    static int old_srate, old_nch;

    if (buffer == nullptr)
        buffer = (float *) g_malloc (BUFFER_BYTES);

    echo_channels = *channels;
    echo_rate = *rate;

    if (echo_channels != old_nch || echo_rate != old_srate)
    {
        memset(buffer, 0, BUFFER_BYTES);
        w_ofs = 0;
        old_nch = echo_channels;
        old_srate = echo_rate;
    }
}

void EchoPlugin::process (float * * d, int * samples)
{
    int delay = aud_get_int ("echo_plugin", "delay");
    int feedback = aud_get_int ("echo_plugin", "feedback");
    int volume = aud_get_int ("echo_plugin", "volume");

    float in, out, buf;
    int r_ofs;
    float *data = *d;
    float *end = *d + *samples;

    r_ofs = w_ofs - (echo_rate * delay / 1000) * echo_channels;
    if (r_ofs < 0)
        r_ofs += BUFFER_SHORTS;

    for (; data < end; data++)
    {
        in = *data;

        buf = buffer[r_ofs];
        out = in + buf * volume / 100;
        buf = in + buf * feedback / 100;
        buffer[w_ofs] = buf;
        *data = out;

        if (++r_ofs >= BUFFER_SHORTS)
            r_ofs -= BUFFER_SHORTS;
        if (++w_ofs >= BUFFER_SHORTS)
            w_ofs -= BUFFER_SHORTS;
    }
}
