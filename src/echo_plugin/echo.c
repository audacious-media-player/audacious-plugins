#include <stdlib.h>
#include <string.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <audacious/preferences.h>

#define MAX_DELAY 1000
#define MAX_SRATE 50000
#define MAX_CHANNELS 2
#define BYTES_PS sizeof(float)
#define BUFFER_SAMPLES (MAX_SRATE * MAX_DELAY / 1000)
#define BUFFER_SHORTS (BUFFER_SAMPLES * MAX_CHANNELS)
#define BUFFER_BYTES (BUFFER_SHORTS * BYTES_PS)

static const char * const echo_defaults[] = {
 "delay", "500",
 "feedback", "50",
 "volume", "50",
 NULL};

static const PreferencesWidget echo_widgets[] = {
 {WIDGET_LABEL, N_("<b>Echo</b>")},
 {WIDGET_SPIN_BTN, N_("Delay:"),
  .cfg_type = VALUE_INT, .csect = "echo_plugin", .cname = "delay",
  .data = {.spin_btn = {0, MAX_DELAY, 10, N_("ms")}}},
 {WIDGET_SPIN_BTN, N_("Feedback:"),
  .cfg_type = VALUE_INT, .csect = "echo_plugin", .cname = "feedback",
  .data = {.spin_btn = {0, 100, 1, "%"}}},
 {WIDGET_SPIN_BTN, N_("Volume:"),
  .cfg_type = VALUE_INT, .csect = "echo_plugin", .cname = "volume",
  .data = {.spin_btn = {0, 100, 1, "%"}}}};

static const PluginPreferences echo_prefs = {
 .widgets = echo_widgets,
 .n_widgets = ARRAY_LEN (echo_widgets)};

static float *buffer = NULL;
static int w_ofs;

static bool_t init (void)
{
    aud_config_set_defaults ("echo_plugin", echo_defaults);
    return TRUE;
}

static void cleanup(void)
{
    free(buffer);
    buffer = NULL;
}

static int echo_channels = 0;
static int echo_rate = 0;

static void echo_start(int *channels, int *rate)
{
    static int old_srate, old_nch;

    if (buffer == NULL)
        buffer = malloc (BUFFER_BYTES);

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

AUD_EFFECT_PLUGIN
(
    .name = N_("Echo"),
    .domain = PACKAGE,
    .about_text = echo_about,
    .prefs = & echo_prefs,
    .init = init,
    .cleanup = cleanup,
    .start = echo_start,
    .process = echo_process,
    .finish = echo_finish,
    .preserves_format = TRUE
)
