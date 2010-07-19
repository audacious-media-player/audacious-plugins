/*
 *      Copyright (C) Philipp 'ph3-der-loewe' Schafft      - 2008,
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

#include "all.h"

OutputPlugin roar_op = {
	.description = "RoarAudio Output Plugin",
	.init = roar_init,
	.cleanup = NULL,
	.about = roar_about,
	.configure = roar_configure,
	.get_volume = roar_get_volume,
	.set_volume = roar_set_volume,
	.open_audio = roar_open,
	.write_audio = roar_write,
	.close_audio = roar_close,
	.flush = roar_flush,
	.pause = roar_pause,
	.output_time = roar_get_output_time,
	.written_time = roar_get_written_time,
//	.buffer_playing = roar_buffer_is_playing,
//	.buffer_free = roar_buffer_get_size,
	.drain = roar_drain,
};

OutputPlugin *roar_oplist[] = { &roar_op, NULL };

SIMPLE_OUTPUT_PLUGIN(roaraudio, roar_oplist);

OutputPluginInitStatus roar_init(void)
{
	mcs_handle_t *cfgfile;

	cfgfile = aud_cfg_db_open();

	g_inst.state = 0;
	g_inst.server = NULL;
	g_inst.mixer[0] = g_inst.mixer[1] = 100;

	aud_cfg_db_get_string(cfgfile, "ROAR", "server", &g_inst.server);

	aud_cfg_db_get_string(cfgfile, "ROAR", "player_name", &g_inst.cfg.player_name);

	aud_cfg_db_close(cfgfile);

	if (g_inst.cfg.player_name == NULL)
		g_inst.cfg.player_name = "Audacious";

	if (!(g_inst.state & STATE_CONNECTED))
	{
		if (roar_simple_connect(&(g_inst.con), g_inst.server, g_inst.cfg.player_name) == -1)
			return OUTPUT_PLUGIN_INIT_FAIL;

		g_inst.state |= STATE_CONNECTED;
	}

	return OUTPUT_PLUGIN_INIT_FOUND_DEVICES;
}

void roar_write(void *ptr, int length)
{
	int r;

	while (length)
	{
		if ((r = roar_vio_write(&(g_inst.vio), ptr, length >= 17640 ? 17640 : length)) != -1)
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

gboolean roar_initialize_stream(struct roar_vio_calls *calls, struct roar_connection *con, struct roar_stream *stream, int rate, int nch, int bits, int codec, int dir)
{
	if (roar_vio_simple_new_stream_obj(&(g_inst.vio), &(g_inst.con), &(g_inst.stream), rate, nch, bits, codec, ROAR_DIR_PLAY) == -1)
	{
		roar_disconnect(&(g_inst.con));
		return FALSE;
	}

	g_inst.bits = bits;
	g_inst.nch = nch;
	g_inst.rate = rate;
	g_inst.codec = codec;
	g_inst.written = 0;
	g_inst.pause = 0;	
	g_inst.state |= STATE_PLAYING;

	roar_set_volume(g_inst.mixer[0], g_inst.mixer[1]);

	return TRUE;
}

int roar_open(gint fmt, int rate, int nch)
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
		  return FALSE;
		  break;
	}

	g_inst.bps = nch * rate * bits / 8;

	if (!roar_initialize_stream(&(g_inst.vio), &(g_inst.con), &(g_inst.stream), rate, nch, bits, codec, ROAR_DIR_PLAY))
		return FALSE;

	roar_update_metadata();

	return TRUE;
}

void roar_close(void)
{
	gint id;

	id = roar_stream_get_id(&(g_inst.stream));
	roar_kick(&(g_inst.con), ROAR_OT_STREAM, id);
	roar_vio_close(&(g_inst.vio));

	g_inst.state &= ~STATE_PLAYING;
	g_inst.written = 0;
}

void roar_pause(short p)
{
	if (p)
		roar_stream_set_flags(&(g_inst.con), &(g_inst.stream), ROAR_FLAG_PAUSE | ROAR_FLAG_MUTE, ROAR_SET_FLAG);
	else
		roar_stream_set_flags(&(g_inst.con), &(g_inst.stream), ROAR_FLAG_PAUSE | ROAR_FLAG_MUTE, ROAR_RESET_FLAG);

	g_inst.pause = p;
}

void roar_flush(int time)
{
	gint64 r = time;

	r *= g_inst.bps;
	r /= 1000;

	roar_close();
	if (!roar_initialize_stream(&(g_inst.vio), &(g_inst.con), &(g_inst.stream),
	    g_inst.rate, g_inst.nch, g_inst.bits, g_inst.codec, ROAR_DIR_PLAY))
		return;

	g_inst.written = r;
	g_inst.sampleoff = ((time / 1000) * g_inst.rate) * g_inst.nch;
}

gint64 roar_get_adjusted_sample_count(void)
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

int roar_get_output_time(void)
{
	gint64 samples;

	samples = roar_get_adjusted_sample_count();

	return (samples * 1000) / (g_inst.rate * g_inst.nch);
}

int roar_get_written_time(void)
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
int roar_update_metadata(void)
{
	struct roar_meta meta;
	char empty = 0;
	const gchar *info = NULL;
	gint pos, playlist;

	playlist = aud_playlist_get_active();
	pos = aud_playlist_get_position(playlist);

	meta.value = &empty;
	meta.key[0] = 0;
	meta.type = ROAR_META_TYPE_NONE;

	roar_stream_meta_set(&(g_inst.con), &(g_inst.stream), ROAR_META_MODE_CLEAR, &meta);

	info = aud_playlist_entry_get_filename(playlist, pos);
	if (info)
	{
		if (strncmp(info, "http:", 5) == 0)
			meta.type = ROAR_META_TYPE_FILEURL;
		else
			meta.type = ROAR_META_TYPE_FILENAME;

		meta.value = g_strdup(info);
		roar_stream_meta_set(&(g_inst.con), &(g_inst.stream), ROAR_META_MODE_SET, &meta);

		free(meta.value);
	}

	info = aud_playlist_entry_get_title(playlist, pos, TRUE);
	if (info)
	{
		meta.type = ROAR_META_TYPE_TITLE;

		meta.value = g_strdup(info);
		roar_stream_meta_set(&(g_inst.con), &(g_inst.stream), ROAR_META_MODE_SET, &meta);

		free(meta.value);
	}

	meta.value = &empty;
	meta.type = ROAR_META_TYPE_NONE;
	roar_stream_meta_set(&(g_inst.con), &(g_inst.stream), ROAR_META_MODE_FINALIZE, &meta);

	return 0;
}

// MIXER:
void roar_get_volume(int *l, int *r)
{
	if (!(g_inst.state & STATE_CONNECTED))
		return;

	*l = g_inst.mixer[0];
	*r = g_inst.mixer[1];
}

void roar_set_volume(int l, int r)
{
	struct roar_mixer_settings mixer;

	if (!(g_inst.state & STATE_CONNECTED))
		return;

	mixer.mixer[0] = g_inst.mixer[0] = l;
	mixer.mixer[1] = g_inst.mixer[1] = r;

	mixer.scale = 100;

	roar_set_vol(&(g_inst.con), g_inst.stream.id, &mixer, 2);
}

void roar_drain(void)
{
	gint id;

	if (!(g_inst.state & STATE_CONNECTED))
		return;

	if (!(g_inst.state & STATE_PLAYING))
		return;

	roar_vio_sync(&(g_inst.vio));

	/* first update our stream record. */
	id = roar_stream_get_id(&(g_inst.stream));
	roar_get_stream(&(g_inst.con), &(g_inst.stream), id);

	/* we're starting a new song, so update the sample offset so the timing is right. */
	g_inst.sampleoff = g_inst.stream.pos;
}

gboolean roar_buffer_is_playing(void)
{
	return g_inst.state & (STATE_PLAYING) ? TRUE : FALSE;
}

gint roar_buffer_get_size(void)
{
	gint64 samples, bytes;

	samples = roar_get_adjusted_sample_count();

	if (samples == 0)
		return 17640;

	bytes = (samples * (g_inst.bits / 8)) * g_inst.nch;

	return g_inst.written - bytes;
}
