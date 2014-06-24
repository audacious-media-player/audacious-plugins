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

static const char * const echo_defaults[] = {
 "delay", "500",
 "feedback", "50",
 "volume", "50",
 nullptr};

static const PreferencesWidget echo_widgets[] = {
    WidgetLabel (N_("<b>Echo</b>")),
    WidgetSpin (N_("Delay:"),
        {VALUE_INT, 0, "echo_plugin", "delay"},
        {0, MAX_DELAY, 10, N_("ms")}),
    WidgetSpin (N_("Feedback:"),
        {VALUE_INT, 0, "echo_plugin", "feedback"},
        {0, 100, 1, "%"}),
    WidgetSpin (N_("Volume:"),
        {VALUE_INT, 0, "echo_plugin", "volume"},
        {0, 100, 1, "%"})
};

static const PluginPreferences echo_prefs = {
    echo_widgets,
    ARRAY_LEN (echo_widgets)
};

static float *buffer = nullptr;
static int w_ofs;

static bool init (void)
{
    aud_config_set_defaults ("echo_plugin", echo_defaults);
    return true;
}

static void cleanup(void)
{
    g_free(buffer);
    buffer = nullptr;
}

static int echo_channels = 0;
static int echo_rate = 0;

static void echo_start(int *channels, int *rate)
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

static void echo_process(float **d, int *samples)
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

static void echo_finish(float **d, int *samples)
{
    echo_process(d, samples);
}

static const char echo_about[] =
 N_("Echo Plugin\n"
    "By Johan Levin, 1999\n\n"
    "Surround echo by Carl van Schaik, 1999");

#define AUD_PLUGIN_NAME        N_("Echo")
#define AUD_PLUGIN_ABOUT       echo_about
#define AUD_PLUGIN_PREFS       & echo_prefs
#define AUD_PLUGIN_INIT        init
#define AUD_PLUGIN_CLEANUP     cleanup
#define AUD_EFFECT_START       echo_start
#define AUD_EFFECT_PROCESS     echo_process
#define AUD_EFFECT_FINISH      echo_finish
#define AUD_EFFECT_SAME_FMT    true

#define AUD_DECLARE_EFFECT
#include <libaudcore/plugin-declare.h>
