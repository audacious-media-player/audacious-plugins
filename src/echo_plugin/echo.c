#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>

#include <audacious/configdb.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>

#include "echo.h"


static gboolean init (void);
static void cleanup(void);

#define MAX_SRATE 50000
#define MAX_CHANNELS 2
#define BYTES_PS sizeof(gfloat)
#define BUFFER_SAMPLES (MAX_SRATE * MAX_DELAY / 1000)
#define BUFFER_SHORTS (BUFFER_SAMPLES * MAX_CHANNELS)
#define BUFFER_BYTES (BUFFER_SHORTS * BYTES_PS)

static gfloat *buffer = NULL;
gint echo_delay = 500, echo_feedback = 50, echo_volume = 50;
static int w_ofs;

static gboolean init (void)
{
	mcs_handle_t *cfg;

	cfg = aud_cfg_db_open();
	aud_cfg_db_get_int(cfg, "echo_plugin", "delay", &echo_delay);
	aud_cfg_db_get_int(cfg, "echo_plugin", "feedback", &echo_feedback);
	aud_cfg_db_get_int(cfg, "echo_plugin", "volume", &echo_volume);
	aud_cfg_db_close(cfg);

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

static void echo_flush(void)
{

}

static gint echo_decoder_to_output_time(gint time)
{
	return time;
}

static gint echo_output_to_decoder_time(gint time)
{
	return time;
}

static void echo_process(gfloat **d, gint *samples)
{
	gfloat in, out, buf;
	gint r_ofs;
	gfloat *data = *d;
	gfloat *end = *d + *samples;

	r_ofs = w_ofs - (echo_rate * echo_delay / 1000) * echo_channels;
	if (r_ofs < 0)
		r_ofs += BUFFER_SHORTS;

	for (; data < end; data++)
	{
		in = *data;

		buf = buffer[r_ofs];
		out = in + buf * echo_volume / 100;
		buf = in + buf * echo_feedback / 100;
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

EffectPlugin echo_ep =
{
	.description = "Echo Plugin", /* Description */
	.init = init,
	.cleanup = cleanup,
	.about = echo_about,
	.configure = echo_configure,
	.start = echo_start,
	.process = echo_process,
	.flush = echo_flush,
	.finish = echo_finish,
	.decoder_to_output_time = echo_decoder_to_output_time,
	.output_to_decoder_time = echo_output_to_decoder_time
};

EffectPlugin *echo_eplist[] = { &echo_ep, NULL };

SIMPLE_EFFECT_PLUGIN(echo, echo_eplist);
