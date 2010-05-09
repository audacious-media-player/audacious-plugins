/*
 * Copyright (c) 2010 William Pitcock <nenolod@dereferenced.org>.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define DEBUG
#include <audacious/plugin.h>
#include <audacious/audtag.h>
#include <mpg123.h>

static GMutex *ctrl_mutex = NULL;

/** utility functions **/
static gint
mpg123_get_length(VFSFile *fd)
{
	mpg123_handle *decoder;
	mpg123_pars *params;
	const glong *rates;
	gsize num_rates;
	gsize len;
	gint ret;
	gint i;
	glong rate;
	gint channels, encoding;
	gint samples;

	g_return_val_if_fail(fd != NULL, -2);

	AUDDBG("starting probe\n");

	mpg123_rates(&rates, &num_rates);

	params = mpg123_new_pars(&ret);
	g_return_val_if_fail(params != NULL, -2);

	mpg123_par(params, MPG123_ADD_FLAGS, MPG123_QUIET, 0);
	mpg123_par(params, MPG123_ADD_FLAGS, MPG123_GAPLESS, 0);
	mpg123_par(params, MPG123_RVA, MPG123_RVA_OFF, 0);

	decoder = mpg123_parnew(params, NULL, &ret);
	if (decoder == NULL)
	{
		AUDDBG("mpg123 error: %s", mpg123_plain_strerror(ret));
		mpg123_delete_pars(params);
		return -2;
	}

	if ((ret = mpg123_open_feed(decoder)) != MPG123_OK)
	{
		AUDDBG("mpg123 error: %s", mpg123_plain_strerror(ret));
		mpg123_delete(decoder);
		mpg123_delete_pars(params);
		return -2;
	}

	mpg123_format_none(decoder);
	for (i = 0; i < num_rates; i++)
		mpg123_format(decoder, rates[i], (MPG123_MONO | MPG123_STEREO), MPG123_ENC_SIGNED_16);

	do
	{
		guchar buf[16384];

		len = aud_vfs_fread(buf, 1, 16384, fd);
		if (len <= 0)
			break;

		ret = mpg123_decode(decoder, buf, len, NULL, 0, NULL);
	} while (ret == MPG123_NEED_MORE);

	if (ret != MPG123_NEW_FORMAT)
	{
		mpg123_delete(decoder);
		mpg123_delete_pars(params);
		return -2;
	}

	mpg123_getformat(decoder, &rate, &channels, &encoding);

	AUDDBG("stream identified as MPEG; %ldHz %d channels, encoding %d\n",
		rate, channels, encoding);

	if (aud_vfs_is_streaming(fd))
	{
		mpg123_delete(decoder);
		mpg123_delete_pars(params);
		return -1;
	}

	mpg123_scan(decoder);
	samples = mpg123_length(decoder);

	mpg123_delete(decoder);
	mpg123_delete_pars(params);

	AUDDBG("song has %ld samples; length %ld seconds\n", samples, (samples / rate));

	return (samples / rate);
}

/** plugin glue **/
static void
aud_mpg123_init(void)
{
	AUDDBG("initializing mpg123 library\n");
	mpg123_init();

	AUDDBG("initializing control mutex\n");
	ctrl_mutex = g_mutex_new();
}

static void
aud_mpg123_deinit(void)
{
	AUDDBG("deinitializing mpg123 library\n");
	mpg123_exit();

	AUDDBG("deinitializing control mutex\n");
	g_mutex_free(ctrl_mutex);
}

static gboolean
mpg123_probe_for_fd(const gchar *filename, VFSFile *fd)
{
	aud_vfs_fseek(fd, 0, SEEK_SET);
	return (mpg123_get_length(fd) >= -1);
}

static Tuple *
mpg123_probe_for_tuple(const gchar *filename, VFSFile *fd)
{
	Tuple *tu;
	gint len;

	AUDDBG("starting probe of %p\n", fd);

	aud_vfs_fseek(fd, 0, SEEK_SET);
	len = mpg123_get_length(fd);

	if (len == -2)
		return NULL;

	tu = aud_tuple_new_from_filename(filename);

	aud_vfs_fseek(fd, 0, SEEK_SET);
	tag_tuple_read(tu, fd);	

	AUDDBG("returning tuple %p for file %p\n", tu, fd);
	aud_tuple_associate_int(tu, FIELD_LENGTH, NULL, len * 1000);

	return tu;	
}

typedef struct {
	VFSFile *fd;
	mpg123_handle *decoder;
	mpg123_pars *params;
	glong rate;
	gint channels;
	gint encoding;	
	gint64 seek;
} MPG123PlaybackContext;

static void
mpg123_playback_worker(InputPlayback *data)
{
	MPG123PlaybackContext ctx = {};
	gint ret;
	gint i;
	const glong *rates;
	gsize num_rates;
	gint bitrate = 0;
	struct mpg123_frameinfo fi = {};

	AUDDBG("playback worker started for %s\n", data->filename);

	ctx.seek = -1;
	data->data = &ctx;

	AUDDBG("decoder setup\n");
	mpg123_rates(&rates, &num_rates);

	ctx.params = mpg123_new_pars(&ret);
	mpg123_par(ctx.params, MPG123_ADD_FLAGS, MPG123_QUIET, 0);
	mpg123_par(ctx.params, MPG123_ADD_FLAGS, MPG123_GAPLESS, 0);
	mpg123_par(ctx.params, MPG123_RVA, MPG123_RVA_OFF, 0);

	ctx.decoder = mpg123_parnew(ctx.params, NULL, &ret);
	if (ctx.decoder == NULL)
	{
		AUDDBG("mpg123 error: %s", mpg123_plain_strerror(ret));
		mpg123_delete_pars(ctx.params);
		return;
	}

	if ((ret = mpg123_open_feed(ctx.decoder)) != MPG123_OK)
	{
		AUDDBG("mpg123 error: %s", mpg123_plain_strerror(ret));
		mpg123_delete(ctx.decoder);
		mpg123_delete_pars(ctx.params);
		return;
	}

	AUDDBG("decoder format configuration\n");
	mpg123_format_none(ctx.decoder);
	for (i = 0; i < num_rates; i++)
		mpg123_format(ctx.decoder, rates[i], (MPG123_MONO | MPG123_STEREO), MPG123_ENC_SIGNED_16);

	ctx.fd = aud_vfs_fopen(data->filename, "r");
	AUDDBG("opened stream transport @%p\n", ctx.fd);

	AUDDBG("decoder format identification\n");
	do
	{
		guchar buf[16384];
		gsize len;

		len = aud_vfs_fread(buf, 1, 16384, ctx.fd);
		if (len <= 0)
			goto cleanup;

		ret = mpg123_decode(ctx.decoder, buf, len, NULL, 0, NULL);
	} while (ret == MPG123_NEED_MORE);

	if (ret != MPG123_NEW_FORMAT)
		goto cleanup;

	mpg123_getformat(ctx.decoder, &ctx.rate, &ctx.channels, &ctx.encoding);

	AUDDBG("stream identified as MPEG; %ldHz %d channels, encoding %d\n",
		ctx.rate, ctx.channels, ctx.encoding);

	AUDDBG("opening audio\n");
	if (!data->output->open_audio(FMT_S16_NE, ctx.rate, ctx.channels))
		goto cleanup;

	AUDDBG("starting decode\n");
	data->playing = TRUE;
	data->set_pb_ready(data);

	while (data->playing == TRUE)
	{
		guchar buf[16384];
		gsize len = 0;
		guchar outbuf[2048];
		gsize outbuf_size;

		mpg123_info(ctx.decoder, &fi);
		if (fi.bitrate != bitrate)
		{
			data->set_params(data, NULL, 0, (fi.bitrate * 1000), ctx.rate, ctx.channels);
			bitrate = fi.bitrate;
		}

		do
		{
			if (ret == MPG123_NEED_MORE)
			{
				AUDDBG("mpg123 requested more data\n");

				len = aud_vfs_fread(buf, 1, 16384, ctx.fd);
				if (len <= 0)
				{
					if (len == 0)
					{
						AUDDBG("stream EOF (well, read failed)\n");
						mpg123_decode(ctx.decoder, buf, 0, outbuf, 16384, &outbuf_size);

						AUDDBG("passing %ld bytes of audio\n", outbuf_size);
						data->pass_audio(data, FMT_S16_NE, ctx.channels, outbuf_size, outbuf, NULL);
						goto decode_cleanup;
					}
					else
						goto decode_cleanup;
				}

				AUDDBG("got %ld bytes for mpg123\n", len);
			}

			ret = mpg123_decode(ctx.decoder, buf, len, outbuf, 2048, &outbuf_size);
			data->pass_audio(data, FMT_S16_NE, ctx.channels, outbuf_size, outbuf, NULL);
		} while (ret == MPG123_NEED_MORE);

		g_mutex_lock(ctrl_mutex);

		if (data->playing == FALSE)
		{
			g_mutex_unlock(ctrl_mutex);
			break;
		}

		if (ctx.seek != -1)
		{
			off_t byteoff, sampleoff;

			sampleoff = mpg123_feedseek(ctx.decoder, (ctx.seek * ctx.rate), SEEK_SET, &byteoff);
			if (sampleoff < 0)
			{
				AUDDBG("mpg123 error: %s", mpg123_strerror(ctx.decoder));
				ctx.seek = -1;

				g_mutex_unlock(ctrl_mutex);
				continue;
			}

			AUDDBG("seeking to %ld (byte %ld)\n", ctx.seek, byteoff);
			data->output->flush(ctx.seek * 1000);
			aud_vfs_fseek(ctx.fd, byteoff, SEEK_SET);

			ctx.seek = -1;
		}

		g_mutex_unlock(ctrl_mutex);
	}

decode_cleanup:
	AUDDBG("decode complete\n");
	data->output->close_audio();

cleanup:
	mpg123_delete(ctx.decoder);
	mpg123_delete_pars(ctx.params);
	aud_vfs_fclose(ctx.fd);
}

static void mpg123_stop_playback_worker(InputPlayback *data)
{
	AUDDBG("signalling playback worker to die\n");	
	g_mutex_lock(ctrl_mutex);
	data->playing = FALSE;
	g_mutex_unlock(ctrl_mutex);

	AUDDBG("waiting for playback worker to die\n");
	g_thread_join(data->thread);
	data->thread = NULL;

	AUDDBG("playback worker died\n");
}

static void mpg123_seek_time(InputPlayback *data, gint time)
{
	g_mutex_lock(ctrl_mutex);

	if (data->playing)
	{
		MPG123PlaybackContext *ctx = data->data;
		ctx->seek = time;
	}

	g_mutex_unlock(ctrl_mutex);
}

/** plugin description header **/
static const gchar *mpg123_fmts[] = { "mp3", "mp2", "mp1", "bmu", NULL };

static InputPlugin mpg123_ip = {
	.init = aud_mpg123_init,
	.cleanup = aud_mpg123_deinit,
	.description = "MPG123",
	.vfs_extensions = (gchar **) mpg123_fmts,
	.is_our_file_from_vfs = mpg123_probe_for_fd,
	.probe_for_tuple = mpg123_probe_for_tuple,
	.play_file = mpg123_playback_worker,
	.stop = mpg123_stop_playback_worker,
	.seek = mpg123_seek_time,
};

static InputPlugin *mpg123_iplist[] = { &mpg123_ip, NULL };

SIMPLE_INPUT_PLUGIN(mpg123, mpg123_iplist);
