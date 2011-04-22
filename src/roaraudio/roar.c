/*
 *      Copyright (C) Philipp 'ph3-der-loewe' Schafft      - 2008-2010,
 *                    Daniel Duntemann <dauxx@dauxx.org>   - 2009,
 *                    William Pitcock <nenolod@atheme.org> - 2010.
 *
 *  This file is part of the Audacious RoarAudio output plugin a part of RoarAudio,
 *  a cross-platform sound system for both, home and professional use.
 *  See README for details.
 *
 *  This file is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3
 *  as published by the Free Software Foundation.
 *
 *  RoarAudio is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

/*
 * NOTE: Do not spam with requests.  You WILL get kicked out on Linux.
 */

#include "all.h"

AUD_OUTPUT_PLUGIN
(
	.name = "RoarAudio Output",
	.init = aud_roar_init,
	.cleanup = NULL,
	.get_volume = aud_roar_get_volume,
	.set_volume = aud_roar_set_volume,
	.open_audio = aud_roar_open,
	.write_audio = aud_roar_write,
	.close_audio = aud_roar_close,
	.flush = aud_roar_flush,
	.pause = aud_roar_pause,
	.output_time = aud_roar_get_output_time,
	.written_time = aud_roar_get_written_time,
	.drain = aud_roar_drain,
	.buffer_free = aud_roar_buffer_get_size,
//	.period_wait = aud_roar_period_wait,
)

gboolean aud_roar_init(void)
{
	mcs_handle_t *cfgfile;

	cfgfile = aud_cfg_db_open();

	g_inst.state = 0;
	g_inst.server = NULL;
	g_inst.mixer[0] = g_inst.mixer[1] = 100;

	aud_cfg_db_close(cfgfile);

	if (!(g_inst.state & STATE_CONNECTED))
	{
		if (roar_simple_connect(&(g_inst.con), NULL, "Audacious") == -1)
			return FALSE;

		g_inst.state |= STATE_CONNECTED;
	}

	return TRUE;
}

void aud_roar_write(void *ptr, int length)
{
	int r;

	while (length)
	{
		if ((r = roar_vio_write(&(g_inst.vio), ptr, length >= g_inst.block_size ? g_inst.block_size : length)) != -1)
		{
			g_inst.written += r;
			ptr += r;
			length -= r;
		}
		else
		{
			return;
		}
	}
}

gboolean aud_roar_initialize_stream(struct roar_vio_calls *calls, struct roar_connection *con, struct roar_stream *stream, int rate, int nch, int bits, int codec, int dir)
{
	struct roar_stream_info info;

	if (roar_vio_simple_new_stream_obj(&(g_inst.vio), &(g_inst.con), &(g_inst.stream), rate, nch, bits, codec, ROAR_DIR_PLAY) == -1)
		return FALSE;

	g_inst.bits = bits;
	g_inst.nch = nch;
	g_inst.rate = rate;
	g_inst.codec = codec;
	g_inst.written = 0;
	g_inst.pause = 0;
	g_inst.sampleoff = 0;
	g_inst.state |= STATE_PLAYING;
	g_inst.block_size = 0;

	if (roar_stream_get_info(&(g_inst.con), &(g_inst.stream), &info) != -1)
	{
		/* XXX: this is wrong. */
		g_inst.block_size = info.block_size * 2;
		AUDDBG("setting block size to %d\n", g_inst.block_size);
	}

	roar_stream_set_role(&(g_inst.con), &(g_inst.stream), ROAR_ROLE_MUSIC);
	aud_roar_set_volume(g_inst.mixer[0], g_inst.mixer[1]);

	return TRUE;
}

int aud_roar_open(gint fmt, int rate, int nch)
{
	int codec = ROAR_CODEC_DEFAULT;
	int bits;

	bits = 16;
	switch (fmt)
	{
	  case FMT_S8:
		  bits = 8;
		  codec = ROAR_CODEC_DEFAULT;
		  break;
	  case FMT_U8:
		  bits = 8;
		  codec = ROAR_CODEC_PCM_U_LE;	// _LE, _BE, _PDP,... all the same for 8 bit output
		  break;
	  case FMT_U16_LE:
		  codec = ROAR_CODEC_PCM_U_LE;
		  break;
	  case FMT_U16_BE:
		  codec = ROAR_CODEC_PCM_U_BE;
		  break;
	  case FMT_S16_LE:
		  codec = ROAR_CODEC_PCM_S_LE;
		  break;
	  case FMT_S16_BE:
		  codec = ROAR_CODEC_PCM_S_BE;
		  break;
#if 0
/* RoarAudio don't use this stupid 24bit-in-lower-part-of-32bit-int format */
	  case FMT_U24_LE:	/* stored in lower 3 bytes of 32-bit value, highest byte must be 0 */
		  codec = ROAR_CODEC_PCM_U_LE;
		  bits = 24;
		  break;
	  case FMT_U24_BE:
		  codec = ROAR_CODEC_PCM_U_BE;
		  bits = 24;
		  break;
	  case FMT_S24_LE:
		  bits = 24;
		  codec = ROAR_CODEC_PCM_S_LE;
		  break;
	  case FMT_S24_BE:
		  bits = 24;
		  codec = ROAR_CODEC_PCM_S_BE;
		  break;
#endif
	  case FMT_U32_LE:	/* highest byte must be 0 */
		  codec = ROAR_CODEC_PCM_U_LE;
		  bits = 32;
		  break;
	  case FMT_U32_BE:
		  codec = ROAR_CODEC_PCM_U_BE;
		  bits = 32;
		  break;
	  case FMT_S32_LE:
		  bits = 32;
		  codec = ROAR_CODEC_PCM_S_LE;
		  break;
	  case FMT_S32_BE:
		  bits = 32;
		  codec = ROAR_CODEC_PCM_S_BE;
		  break;
	  case FMT_FLOAT:
	  default:
		  return FALSE;
		  break;
	}

	g_inst.bps = nch * rate * bits / 8;

	if (!aud_roar_initialize_stream(&(g_inst.vio), &(g_inst.con), &(g_inst.stream), rate, nch, bits, codec, ROAR_DIR_PLAY))
		return FALSE;

	aud_roar_update_metadata();

	return TRUE;
}

void aud_roar_close(void)
{
	gint id;

	id = roar_stream_get_id(&(g_inst.stream));
	roar_kick(&(g_inst.con), ROAR_OT_STREAM, id);
	roar_vio_close(&(g_inst.vio));

	g_inst.state &= ~STATE_PLAYING;
	g_inst.written = 0;
}

void aud_roar_pause(gboolean p)
{
	if (p)
		roar_stream_set_flags(&(g_inst.con), &(g_inst.stream), ROAR_FLAG_PAUSE, ROAR_SET_FLAG);
	else
		roar_stream_set_flags(&(g_inst.con), &(g_inst.stream), ROAR_FLAG_PAUSE, ROAR_RESET_FLAG);

	g_inst.pause = p;
}

void aud_roar_flush(int time)
{
	gint64 r = time;

	r *= g_inst.bps;
	r /= 1000;

	aud_roar_close();
	if (!aud_roar_initialize_stream(&(g_inst.vio), &(g_inst.con), &(g_inst.stream),
	    g_inst.rate, g_inst.nch, g_inst.bits, g_inst.codec, ROAR_DIR_PLAY))
		return;

	g_inst.written = r;
	g_inst.sampleoff = ((time / 1000) * g_inst.rate) * g_inst.nch;
}

gint64 aud_roar_get_adjusted_sample_count(void)
{
	gint id;
	gint64 samples;

	/* first update our stream record. */
	id = roar_stream_get_id(&(g_inst.stream));
	roar_get_stream(&(g_inst.con), &(g_inst.stream), id);

	/* calculate the time, given our sample count. */
	samples = g_inst.stream.pos;
	samples += g_inst.sampleoff;

	return samples;
}

int aud_roar_get_output_time(void)
{
	gint64 samples;

	samples = aud_roar_get_adjusted_sample_count();

	return (samples * 1000) / (g_inst.rate * g_inst.nch);
}

int aud_roar_get_written_time(void)
{
	gint64 r;

	if (!g_inst.bps)
		return 0;

	r = g_inst.written;
	r *= 1000;		// sec -> msec
	r /= g_inst.bps;

	return r;
}

// META DATA:
int aud_roar_update_metadata(void)
{
	struct roar_meta meta;
	char empty = 0;
	const gchar *info = NULL;
	gint pos, playlist;
	gint i;
	static struct { int ac_metatype, ra_metatype; } metamap[] = {
		{FIELD_ARTIST,		ROAR_META_TYPE_ARTIST},
		{FIELD_TITLE,		ROAR_META_TYPE_TITLE},
		{FIELD_ALBUM,		ROAR_META_TYPE_ALBUM},
		{FIELD_COMMENT,		ROAR_META_TYPE_COMMENT},
		{FIELD_GENRE,		ROAR_META_TYPE_GENRE},
		{FIELD_PERFORMER,	ROAR_META_TYPE_PERFORMER},
		{FIELD_COPYRIGHT,	ROAR_META_TYPE_COPYRIGHT},
		{FIELD_DATE,		ROAR_META_TYPE_DATE},
	};

	playlist = aud_playlist_get_active();
	pos = aud_playlist_get_position(playlist);

	meta.value = &empty;
	meta.key[0] = 0;
	meta.type = ROAR_META_TYPE_NONE;

	roar_stream_meta_set(&(g_inst.con), &(g_inst.stream), ROAR_META_MODE_CLEAR, &meta);

	gchar * filename = aud_playlist_entry_get_filename (playlist, pos);
	if (filename)
	{
		if (! strncmp (filename, "http://", 7))
			meta.type = ROAR_META_TYPE_FILEURL;
		else
			meta.type = ROAR_META_TYPE_FILENAME;

		meta.value = filename;
		roar_stream_meta_set(&(g_inst.con), &(g_inst.stream), ROAR_META_MODE_SET, &meta);

		g_free (filename);
	}

	Tuple * songtuple = aud_playlist_entry_get_tuple (playlist, pos, TRUE);
	if (songtuple)
	{
		for (i = 0; i < sizeof(metamap)/sizeof(*metamap); i++)
		{
			if ((info = tuple_get_string(songtuple, metamap[i].ac_metatype, NULL)))
			{
				meta.type = metamap[i].ra_metatype;
				meta.value = g_strdup(info);

				roar_stream_meta_set(&(g_inst.con), &(g_inst.stream), ROAR_META_MODE_SET, &meta);

				free(meta.value);
			}
		}

		tuple_free (songtuple);
	}

	meta.value = &empty;
	meta.type = ROAR_META_TYPE_NONE;
	roar_stream_meta_set(&(g_inst.con), &(g_inst.stream), ROAR_META_MODE_FINALIZE, &meta);

	return 0;
}

// MIXER:
void aud_roar_get_volume(int *l, int *r)
{
	struct roar_mixer_settings mixer;
	int channels;
	float fs;

	if (!(g_inst.state & STATE_CONNECTED))
		return;

#ifdef NOTYET
	if (roar_get_vol(&(g_inst.con), g_inst.stream.id, &mixer, &channels) == -1)
		return;

	if (channels == 1)
		mixer.mixer[1] = mixer.mixer[0];

	fs = (float)mixer.scale/100.;

	*l = g_inst.mixer[0] = mixer.mixer[0] / fs;
	*r = g_inst.mixer[1] = mixer.mixer[1] / fs;
#else
	*l = g_inst.mixer[0];
	*r = g_inst.mixer[1];
#endif
}

void aud_roar_set_volume(int l, int r)
{
	struct roar_mixer_settings mixer;

	if (!(g_inst.state & STATE_CONNECTED))
		return;

	mixer.mixer[0] = g_inst.mixer[0] = l;
	mixer.mixer[1] = g_inst.mixer[1] = r;

	mixer.scale = 100;

	roar_set_vol(&(g_inst.con), g_inst.stream.id, &mixer, 2);
}

/* this really sucks. */
gboolean aud_roar_vio_is_writable(struct roar_vio_calls *vio)
{
	struct roar_vio_select vios[1];

	ROAR_VIO_SELECT_SETVIO(&vios[0], vio, ROAR_VIO_SELECT_WRITE);

	return roar_vio_select(vios, 1, &(struct roar_vio_selecttv){ .nsec = 1 }, NULL);
}

gint aud_roar_buffer_get_size(void)
{
	if (!(g_inst.state & STATE_CONNECTED))
		return 0;

	if (!(g_inst.state & STATE_PLAYING))
		return 0;

	if (!aud_roar_vio_is_writable(&(g_inst.vio)))
		return 0;

	if (g_inst.block_size != 0)
		return g_inst.block_size;

	return 0;
}

void aud_roar_period_wait(void)
{
	struct roar_vio_select vios[1];

	if (!(g_inst.state & STATE_PLAYING))
		return;

	ROAR_VIO_SELECT_SETVIO(&vios[0], &(g_inst.vio), ROAR_VIO_SELECT_WRITE);

	roar_vio_select(vios, 1, NULL, NULL);
}

void aud_roar_drain (void) {
	struct roar_event waits, triggered;

	memset(&waits, 0, sizeof(waits));
	waits.event             = ROAR_OE_STREAM_XRUN;
	waits.emitter           = -1;
	waits.target            = roar_stream_get_id(&(g_inst.stream));
	waits.target_type       = ROAR_OT_STREAM;

	// ignore errors as we are in void context anyway.

	roar_vio_sync(&(g_inst.vio));

	roar_wait(&(g_inst.con), &triggered, &waits, 1);
}

