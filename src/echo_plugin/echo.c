#include <audacious/plugin.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <audacious/i18n.h>
#include "audacious/configdb.h"
#include "echo.h"

#include "../../config.h"

static void init(void);
static void cleanup(void);
static int mod_samples(gpointer * d, gint length, AFormat afmt, gint srate, gint nch);

#define MAX_SRATE 50000
#define MAX_CHANNELS 2
#define BYTES_PS 2
#define BUFFER_SAMPLES (MAX_SRATE * MAX_DELAY / 1000)
#define BUFFER_SHORTS (BUFFER_SAMPLES * MAX_CHANNELS)
#define BUFFER_BYTES (BUFFER_SHORTS * BYTES_PS)

EffectPlugin echo_ep =
{
	NULL,
	NULL,
	"Echo Plugin", /* Description */
	init,
	cleanup,
	echo_about,
	echo_configure,
	mod_samples,
	NULL
};

static gint16 *buffer = NULL;
gint echo_delay = 500, echo_feedback = 50, echo_volume = 50;
gboolean echo_surround_enable = FALSE;
static int w_ofs;

EffectPlugin *echo_eplist[] = { &echo_ep, NULL };

DECLARE_PLUGIN(echo, NULL, NULL, NULL, NULL, echo_eplist, NULL, NULL, NULL);

static void init(void)
{
	ConfigDb *cfg;
	
	if (sizeof(short) != sizeof(gint16))
		abort();

	cfg = bmp_cfg_db_open();
	bmp_cfg_db_get_int(cfg, "echo_plugin", "delay", &echo_delay);
	bmp_cfg_db_get_int(cfg, "echo_plugin", "feedback", &echo_feedback);
	bmp_cfg_db_get_int(cfg, "echo_plugin", "volume", &echo_volume);
	bmp_cfg_db_get_bool(cfg, "echo_plugin", "enable_surround", &echo_surround_enable);
	bmp_cfg_db_close(cfg);
}

static void cleanup(void)
{
	g_free(buffer);
	buffer = NULL;	
}

static int mod_samples(gpointer * d, gint length, AFormat afmt, gint srate, gint nch)
{
	gint i, in, out, buf, r_ofs, fb_div;
	gint16 *data = (gint16 *) * d;
	static gint old_srate, old_nch;

	if (!(afmt == FMT_S16_NE ||
	      (afmt == FMT_S16_LE && G_BYTE_ORDER == G_LITTLE_ENDIAN) ||
	      (afmt == FMT_S16_BE && G_BYTE_ORDER == G_BIG_ENDIAN)))
		return length;

	if (!buffer)
		buffer = g_malloc0(BUFFER_BYTES + 2);

	if (nch != old_nch || srate != old_srate)
	{
		memset(buffer, 0, BUFFER_BYTES);
		w_ofs = 0;
		old_nch = nch;
		old_srate = srate;
	}

	if (echo_surround_enable && nch == 2)
		fb_div = 200;
	else
		fb_div = 100;

	r_ofs = w_ofs - (srate * echo_delay / 1000) * nch;
	if (r_ofs < 0)
		r_ofs += BUFFER_SHORTS;

	for (i = 0; i < length / BYTES_PS; i++)
	{
		in = data[i];
		buf = buffer[r_ofs];
		if (echo_surround_enable && nch == 2)
		{
			if (i & 1)
				buf -= buffer[r_ofs - 1];
			else
				buf -= buffer[r_ofs + 1];
		}
		out = in + buf * echo_volume / 100;
		buf = in + buf * echo_feedback / fb_div;
		out = CLAMP(out, -32768, 32767);
		buf = CLAMP(buf, -32768, 32767);
		buffer[w_ofs] = buf;
		data[i] = out;
		if (++r_ofs >= BUFFER_SHORTS)
			r_ofs -= BUFFER_SHORTS;
		if (++w_ofs >= BUFFER_SHORTS)
			w_ofs -= BUFFER_SHORTS;
	}

	return length;
}
