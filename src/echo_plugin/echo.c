#include <glib.h>
#include <string.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <audacious/preferences.h>

#include "config.h"
#include "echo.h"

#define MAX_SRATE 50000
#define MAX_CHANNELS 2
#define BYTES_PS sizeof(gfloat)
#define BUFFER_SAMPLES (MAX_SRATE * MAX_DELAY / 1000)
#define BUFFER_SHORTS (BUFFER_SAMPLES * MAX_CHANNELS)
#define BUFFER_BYTES (BUFFER_SHORTS * BYTES_PS)

static const gchar * const echo_defaults[] = {
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
 .n_widgets = G_N_ELEMENTS (echo_widgets)};

static gfloat *buffer = NULL;
static int w_ofs;

static gboolean init (void)
{
	aud_config_set_defaults ("echo_plugin", echo_defaults);
	return TRUE;
}

static void cleanup(void)
{
	g_free(buffer);
	buffer = NULL;
}

static gint echo_channels = 0;
static gint echo_rate = 0;

static void echo_start(gint *channels, gint *rate)
{
	static gint old_srate, old_nch;

	if (buffer == NULL)
		buffer = g_malloc0(BUFFER_BYTES + sizeof(gfloat));

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

static void echo_process(gfloat **d, gint *samples)
{
	gint delay = aud_get_int ("echo_plugin", "delay");
	gint feedback = aud_get_int ("echo_plugin", "feedback");
	gint volume = aud_get_int ("echo_plugin", "volume");

	gfloat in, out, buf;
	gint r_ofs;
	gfloat *data = *d;
	gfloat *end = *d + *samples;

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

static void echo_finish(gfloat **d, gint *samples)
{
	echo_process(d, samples);
}

AUD_EFFECT_PLUGIN
(
	.name = N_("Echo"),
	.domain = PACKAGE,
	.prefs = & echo_prefs,
	.init = init,
	.cleanup = cleanup,
	.about = echo_about,
	.start = echo_start,
	.process = echo_process,
	.finish = echo_finish,
	.preserves_format = TRUE
)
