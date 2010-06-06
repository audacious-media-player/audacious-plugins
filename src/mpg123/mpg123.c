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

#undef DEBUG

#ifdef DEBUG_MPG123_IO
# define MPG123_IODBG(...)	AUDDBG(__VA_ARGS__)
#else
# define MPG123_IODBG(...)	do { } while (0)
#endif

#include <audacious/plugin.h>
#include <audacious/audtag.h>

/*
 * disable compile-time sanity checks.  we're just using the decoder part,
 * so it doesn't matter.  --nenolod
 */
#define MPG123_NO_CONFIGURE
#include <mpg123.h>

static GMutex *ctrl_mutex = NULL;
static GCond *ctrl_cond = NULL;
static gboolean pause_flag;

/** utility functions **/
static gboolean mpg123_prefill (mpg123_handle * decoder, VFSFile * handle,
 gint * _result)
{
	guchar buffer[16384];
	gsize length;
	gint result;

	do
	{
		if ((length = aud_vfs_fread (buffer, 1, 16384, handle)) <= 0)
			return FALSE;

		result = mpg123_decode (decoder, buffer, length, NULL, 0, NULL);
	}
	while (result == MPG123_NEED_MORE);

	* _result = result;
	return TRUE;
}

static gint
mpg123_get_length(VFSFile *fd)
{
	mpg123_handle *decoder;
	mpg123_pars *params;
	const glong *rates;
	gsize num_rates;
	gint ret;
	gint i;
	glong rate;
	gint channels, encoding;
	gint samples;

	g_return_val_if_fail(fd != NULL, -2);

	AUDDBG("starting probe of %s\n", fd->uri);

	mpg123_rates(&rates, &num_rates);

	params = mpg123_new_pars(&ret);
	g_return_val_if_fail(params != NULL, -2);

	mpg123_par(params, MPG123_ADD_FLAGS, MPG123_QUIET, 0);
	mpg123_par(params, MPG123_ADD_FLAGS, MPG123_GAPLESS, 0);
	mpg123_par(params, MPG123_ADD_FLAGS, MPG123_SEEKBUFFER, 0);
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

	if (! mpg123_prefill (decoder, fd, & ret) || ret != MPG123_NEW_FORMAT)
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

	if (samples <= 0)
	{
		off_t filesize = vfs_fsize(fd);
		mpg123_set_filesize(decoder, filesize);
		samples = mpg123_length(decoder);
	}

	mpg123_delete(decoder);
	mpg123_delete_pars(params);

	AUDDBG("song has %d samples; length %ld seconds\n", samples, (samples / rate));

	return samples * (gint64) 1000 / rate;
}

/** plugin glue **/
static void
aud_mpg123_init(void)
{
	AUDDBG("initializing mpg123 library\n");
	mpg123_init();

	AUDDBG("initializing control mutex\n");
	ctrl_mutex = g_mutex_new();
	ctrl_cond = g_cond_new();
}

static void
aud_mpg123_deinit(void)
{
	AUDDBG("deinitializing mpg123 library\n");
	mpg123_exit();

	AUDDBG("deinitializing control mutex\n");
	g_mutex_free(ctrl_mutex);
	g_cond_free(ctrl_cond);
}

static gboolean
mpg123_probe_for_fd(const gchar *filename, VFSFile *fd)
{
	aud_vfs_fseek(fd, 0, SEEK_SET);
	return (mpg123_get_length(fd) >= -1);
}

static gboolean mpg123_get_info (VFSFile * handle, struct mpg123_frameinfo *
 info)
{
	mpg123_handle * decoder = mpg123_new (NULL, NULL);
	gint result;

	g_return_val_if_fail (decoder != NULL, FALSE);

	/* Turn off annoying messages. */
	mpg123_param (decoder, MPG123_ADD_FLAGS, MPG123_QUIET, 0);

	if (mpg123_open_feed (decoder) < 0)
		goto ERROR_FREE;

	if (! mpg123_prefill (decoder, handle, & result) || result !=
	 MPG123_NEW_FORMAT)
		goto ERROR_FREE;

	if (mpg123_info (decoder, info) < 0)
		goto ERROR_FREE;

	mpg123_delete (decoder);
	return TRUE;

ERROR_FREE:
	mpg123_delete (decoder);
	return FALSE;
}

static Tuple *
mpg123_probe_for_tuple(const gchar *filename, VFSFile *fd)
{
	struct mpg123_frameinfo info;
	Tuple *tu;
	gint len;
	gsize size;

	AUDDBG("starting probe of %p\n", fd);

	aud_vfs_fseek(fd, 0, SEEK_SET);
	len = mpg123_get_length(fd);

	if (len == -2)
		return NULL;

	tu = aud_tuple_new_from_filename(filename);

	aud_vfs_fseek (fd, 0, SEEK_SET);

	if (mpg123_get_info (fd, & info))
	{
		static const gchar * versions[] = {"1", "2", "2.5"};
		gchar format[32];

		snprintf (format, sizeof format, "MPEG-%s layer %d",
		 versions[info.version], info.layer);
		tuple_associate_string (tu, FIELD_CODEC, NULL, format);
	}

	aud_vfs_fseek(fd, 0, SEEK_SET);
	tag_tuple_read(tu, fd);

	if (tuple_get_int(tu, FIELD_LENGTH, NULL) == 0)
		tuple_associate_int (tu, FIELD_LENGTH, NULL, len);

	size = aud_vfs_fsize (fd);

	if (size > 0 && len > 0)
		tuple_associate_int (tu, FIELD_BITRATE, NULL, 8 * size / len);

	AUDDBG("returning tuple %p for file %p\n", tu, fd);
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
	gboolean stream;
	Tuple *tu;
} MPG123PlaybackContext;

static gchar *
get_stream_metadata(VFSFile *file, const gchar *name)
{
	gchar *raw = aud_vfs_get_metadata(file, name);
	gchar *converted = (raw != NULL && raw[0]) ? aud_str_to_utf8(raw) : NULL;

	g_free(raw);
	return converted;
}

static gboolean
update_stream_metadata(VFSFile *file, const gchar *name, Tuple *tuple, gint item)
{
	const gchar *old = aud_tuple_get_string(tuple, item, NULL);
	gchar *new = get_stream_metadata(file, name);
	gboolean changed = (new != NULL && (old == NULL || strcmp(old, new)));

	if (changed)
		aud_tuple_associate_string(tuple, item, NULL, new);

	g_free(new);
	return changed;
}

static void
mpg123_playback_worker(InputPlayback *data)
{
	MPG123PlaybackContext ctx;
	gint ret;
	gint i;
	const glong *rates;
	gsize num_rates;
	gint bitrate = 0, bitrate_sum = 0, bitrate_count = 0;
	gint bitrate_updated = -1000; /* >= a second away from any position */
	gboolean paused = FALSE;
	struct mpg123_frameinfo fi;

	memset(&ctx, 0, sizeof(MPG123PlaybackContext));
	memset(&fi, 0, sizeof(struct mpg123_frameinfo));

	AUDDBG("playback worker started for %s\n", data->filename);

	ctx.seek = -1;
	data->data = &ctx;

	AUDDBG("decoder setup\n");
	mpg123_rates(&rates, &num_rates);

	ctx.params = mpg123_new_pars(&ret);
	mpg123_par(ctx.params, MPG123_ADD_FLAGS, MPG123_QUIET, 0);
	mpg123_par(ctx.params, MPG123_ADD_FLAGS, MPG123_GAPLESS, 0);
	mpg123_par(ctx.params, MPG123_ADD_FLAGS, MPG123_SEEKBUFFER, 0);
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
		mpg123_format (ctx.decoder, rates[i], (MPG123_MONO | MPG123_STEREO),
		 MPG123_ENC_FLOAT_32);

	ctx.fd = aud_vfs_fopen(data->filename, "r");
	AUDDBG("opened stream transport @%p\n", ctx.fd);

	AUDDBG("checking if stream @%p is seekable\n", ctx.fd);
	if (aud_vfs_is_streaming(ctx.fd))
	{
		AUDDBG(" ... it's not.\n");
		ctx.stream = TRUE;
	}
	else
		AUDDBG(" ... it is.\n");

	AUDDBG("decoder format identification\n");
	if (! mpg123_prefill (ctx.decoder, ctx.fd, & ret) || ret !=
	 MPG123_NEW_FORMAT)
		goto cleanup;

	mpg123_getformat(ctx.decoder, &ctx.rate, &ctx.channels, &ctx.encoding);

	AUDDBG("stream identified as MPEG; %ldHz %d channels, encoding %d\n",
		ctx.rate, ctx.channels, ctx.encoding);

	AUDDBG("opening audio\n");
	if (! data->output->open_audio (FMT_FLOAT, ctx.rate, ctx.channels))
		goto cleanup;

	data->set_gain_from_playlist (data);

	g_mutex_lock(ctrl_mutex);

	AUDDBG("starting decode\n");
	data->playing = TRUE;
	data->set_pb_ready(data);
	pause_flag = FALSE;

	g_mutex_unlock(ctrl_mutex);

	while (data->playing == TRUE)
	{
		gfloat outbuf[ctx.channels * (ctx.rate / 100)];
		gsize outbuf_size;

		mpg123_info(ctx.decoder, &fi);
		bitrate_sum += fi.bitrate;
		bitrate_count ++;

		if (bitrate_sum / bitrate_count != bitrate && abs
		 (data->output->written_time () - bitrate_updated) >= 1000)
		{
			data->set_params(data, NULL, 0, bitrate_sum / bitrate_count * 1000,
			 ctx.rate, ctx.channels);
			bitrate = bitrate_sum / bitrate_count;
			bitrate_sum = 0;
			bitrate_count = 0;
			bitrate_updated = data->output->written_time ();
		}

		/* deal with shoutcast titles nonsense */
		if (ctx.stream)
		{
			gboolean changed = FALSE;

			if (!ctx.tu)
				ctx.tu = aud_tuple_new_from_filename(data->filename);

			changed = changed || update_stream_metadata(ctx.fd, "track-name", ctx.tu, FIELD_TITLE);
			changed = changed || update_stream_metadata(ctx.fd, "stream-name", ctx.tu, FIELD_ALBUM);

			if (changed)
			{
				mowgli_object_ref(ctx.tu);
				data->set_tuple(data, ctx.tu);
			}
		}

		do
		{
			guchar buf[16384];
			gsize len = 0;

			if (ret == MPG123_NEED_MORE)
			{
				MPG123_IODBG("mpg123 requested more data\n");

				len = aud_vfs_fread(buf, 1, 16384, ctx.fd);
				if (len <= 0)
				{
					if (len == 0)
					{
						MPG123_IODBG("stream EOF (well, read failed)\n");
						mpg123_decode (ctx.decoder, NULL, 0, (guchar *) outbuf,
						 sizeof outbuf, & outbuf_size);

						MPG123_IODBG("passing %ld bytes of audio\n", outbuf_size);
						data->output->write_audio (outbuf, outbuf_size);
						data->eof = TRUE;
						goto decode_cleanup;
					}
					else
						goto decode_cleanup;
				}

				MPG123_IODBG("got %ld bytes for mpg123\n", len);
			}

			ret = mpg123_decode (ctx.decoder, buf, len, (guchar *) outbuf,
			 sizeof outbuf, & outbuf_size);
			data->output->write_audio (outbuf, outbuf_size);
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

			g_cond_signal(ctrl_cond);
		}

		if (pause_flag != paused)
		{
			data->output->pause(pause_flag);
			paused = pause_flag;
			g_cond_signal(ctrl_cond);
		}

		if (paused)
		{
			g_cond_wait(ctrl_cond, ctrl_mutex);
			g_mutex_unlock(ctrl_mutex);
			continue;
		}

		g_mutex_unlock(ctrl_mutex);
	}

decode_cleanup:
	AUDDBG("eof reached\n");
	while (data->playing && data->output->buffer_playing())
		g_usleep(10000);

	AUDDBG("decode complete\n");
	data->output->close_audio();

cleanup:
	mpg123_delete(ctx.decoder);
	mpg123_delete_pars(ctx.params);
	aud_vfs_fclose(ctx.fd);

	data->playing = FALSE;
}

static void
mpg123_stop_playback_worker(InputPlayback *data)
{
	AUDDBG("signalling playback worker to die\n");
	g_mutex_lock(ctrl_mutex);
	data->playing = FALSE;
	g_cond_signal(ctrl_cond);
	g_mutex_unlock(ctrl_mutex);

	AUDDBG("waiting for playback worker to die\n");
	g_thread_join(data->thread);
	data->thread = NULL;

	AUDDBG("playback worker died\n");
}

static void
mpg123_pause_playback_worker(InputPlayback *data, gshort p)
{
	g_mutex_lock(ctrl_mutex);

	if (data->playing)
	{
		pause_flag = p;
		g_cond_signal(ctrl_cond);
		g_cond_wait(ctrl_cond, ctrl_mutex);
	}

	g_mutex_unlock(ctrl_mutex);
}

static void
mpg123_seek_time(InputPlayback *data, gint time)
{
	g_mutex_lock(ctrl_mutex);

	if (data->playing)
	{
		MPG123PlaybackContext *ctx = data->data;
		ctx->seek = time;
		g_cond_signal(ctrl_cond);
		g_cond_wait(ctrl_cond, ctrl_mutex);
	}

	g_mutex_unlock(ctrl_mutex);
}

static gboolean mpg123_write_tag (Tuple * tuple, VFSFile * handle)
{
	return tag_tuple_write (tuple, handle, TAG_TYPE_APE);
}

static gboolean mpg123_get_image (const gchar * filename, VFSFile * handle,
 void * * data, gint * length)
{
	if (handle == NULL)
		return FALSE;

	return tag_image_read (handle, data, length);
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
	.pause = mpg123_pause_playback_worker,
	.update_song_tuple = mpg123_write_tag,
	.get_song_image = mpg123_get_image,
};

static InputPlugin *mpg123_iplist[] = { &mpg123_ip, NULL };

SIMPLE_INPUT_PLUGIN(mpg123, mpg123_iplist);
