/* Extra Stereo Plugin for Audacious
 * Written by Johan Levin, 1999
 * Modified by John Lindgren, 2009-2012 */

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <audacious/preferences.h>

static bool_t init (void);

static void stereo_start (int * channels, int * rate);
static void stereo_process (float * * data, int * samples);
static void stereo_finish (float * * data, int * samples);

static const char stereo_about[] =
 N_("Extra Stereo Plugin\n\n"
    "By Johan Levin, 1999");

static const char * const stereo_defaults[] = {
 "intensity", "2.5",
 NULL};

static const PreferencesWidget stereo_widgets[] = {
 {WIDGET_LABEL, N_("<b>Extra Stereo</b>")},
 {WIDGET_SPIN_BTN, N_("Intensity:"),
  .cfg_type = VALUE_FLOAT, .csect = "extra_stereo", .cname = "intensity",
  .data = {.spin_btn = {0, 10, 0.1}}}};

static const PluginPreferences stereo_prefs = {
 .widgets = stereo_widgets,
 .n_widgets = sizeof stereo_widgets / sizeof stereo_widgets[0]};

AUD_EFFECT_PLUGIN
(
    .name = N_("Extra Stereo"),
    .domain = PACKAGE,
    .about_text = stereo_about,
    .prefs = & stereo_prefs,
    .init = init,
    .start = stereo_start,
    .process = stereo_process,
    .finish = stereo_finish,
    .preserves_format = TRUE
)

static bool_t init (void)
{
    aud_config_set_defaults ("extra_stereo", stereo_defaults);
    return TRUE;
}

static int stereo_channels;

static void stereo_start (int * channels, int * rate)
{
    stereo_channels = * channels;
}

static void stereo_process (float * * data, int * samples)
{
    float value = aud_get_double ("extra_stereo", "intensity");
    float * f, * end;
    float center;

    if (stereo_channels != 2 || samples == 0)
        return;

    end = (* data) + (* samples);

    for (f = * data; f < end; f += 2)
    {
        center = (f[0] + f[1]) / 2;
        f[0] = center + (f[0] - center) * value;
        f[1] = center + (f[1] - center) * value;
    }
}

static void stereo_finish (float * * data, int * samples)
{
    stereo_process (data, samples);
}
