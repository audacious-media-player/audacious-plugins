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

#include "libmpg123/mpg123.h"

/* Define to read all frame headers when calculating file length */
/* #define FULL_SCAN */

/* id3skip.c */
gint id3_header_size (const guchar * data, gint size);

static GMutex *ctrl_mutex = NULL;
static GCond *ctrl_cond = NULL;

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

	if (result < 0)
		AUDDBG ("mpg123 error: %s\n", mpg123_plain_strerror (result));

	* _result = result;
	return TRUE;
}

static ssize_t r_read (void * f, void * buf, size_t len)
{
	return aud_vfs_fread (buf, 1, len, f);
}

static off_t r_lseek (void * f, off_t to, int whence)
{
	if (aud_vfs_fseek (f, to, whence) < 0)
		return -1;
	return aud_vfs_ftell (f);
}

static gint mpg123_get_length (VFSFile * f)
{
	mpg123_handle * h = mpg123_new (NULL, NULL);
	gint r, chan, enc;
	glong rate;
	gint64 samp;

	g_return_val_if_fail (h != NULL, 0);
	mpg123_param (h, MPG123_ADD_FLAGS, MPG123_QUIET, 0);
	mpg123_replace_reader_handle (h, r_read, r_lseek, NULL);

	if ((r = mpg123_open_handle (h, f)) < 0)
		goto ERROR;
#ifdef FULL_SCAN
	if ((r = mpg123_scan (h)) < 0)
		goto ERROR;
#endif
	if ((r = mpg123_getformat (h, & rate, & chan, & enc)) < 0)
		goto ERROR;
	if ((samp = mpg123_length (h)) < 0)
		goto ERROR;

	mpg123_delete (h);
	return samp * 1000 / rate;

ERROR:
	fprintf (stderr, "mpg123 error: %s\n", mpg123_plain_strerror (r));
	mpg123_delete (h);
	return 0;
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

static gboolean mpg123_probe_for_fd (const gchar * filename, VFSFile * handle)
{
	mpg123_handle * decoder = mpg123_new (NULL, NULL);
	guchar buffer[8192];
	gint result;

	g_return_val_if_fail (decoder != NULL, FALSE);

	/* Turn off annoying messages. */
	mpg123_param (decoder, MPG123_ADD_FLAGS, MPG123_QUIET, 0);

	if (mpg123_open_feed (decoder) < 0)
		goto ERROR_FREE;

	if (aud_vfs_fread (buffer, 1, sizeof buffer, handle) != sizeof buffer)
		goto ERROR_FREE;

	if ((result = id3_header_size (buffer, sizeof buffer)) > 0)
	{
		AUDDBG ("Skip %d bytes of ID3 tag.\n", result);

		if (aud_vfs_fseek (handle, result, SEEK_SET))
			goto ERROR_FREE;

		if (aud_vfs_fread (buffer, 1, sizeof buffer, handle) != sizeof buffer)
			goto ERROR_FREE;
	}

	if ((result = mpg123_decode (decoder, buffer, sizeof buffer, NULL, 0, NULL))
	 != MPG123_NEW_FORMAT)
	{
		AUDDBG ("Probe error: %s.\n", mpg123_plain_strerror (result));
		goto ERROR_FREE;
	}

	AUDDBG ("Accepted as MP3: %s.\n", filename);
	mpg123_delete (decoder);
	return TRUE;

ERROR_FREE:
	mpg123_delete (decoder);
	return FALSE;
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
	gsize size;

	AUDDBG("starting probe of %p\n", fd);

	aud_vfs_fseek(fd, 0, SEEK_SET);

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
		tuple_associate_int (tu, FIELD_LENGTH, NULL, mpg123_get_length (fd));

	size = aud_vfs_fsize (fd);

	gint len = tuple_get_int (tu, FIELD_LENGTH, NULL);
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
	struct mpg123_frameinfo fi;
	gint error_count = 0;

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
		data->error = TRUE;
		return;
	}

	if ((ret = mpg123_open_feed(ctx.decoder)) != MPG123_OK)
	{
		AUDDBG("mpg123 error: %s", mpg123_plain_strerror(ret));
		mpg123_delete(ctx.decoder);
		mpg123_delete_pars(ctx.params);
		data->error = TRUE;
		return;
	}

	AUDDBG("decoder format configuration\n");
	mpg123_format_none(ctx.decoder);
	for (i = 0; i < num_rates; i++)
		mpg123_format (ctx.decoder, rates[i], (MPG123_MONO | MPG123_STEREO),
		 MPG123_ENC_SIGNED_16);

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
	{
		data->error = TRUE;
		goto cleanup;
	}

	mpg123_getformat(ctx.decoder, &ctx.rate, &ctx.channels, &ctx.encoding);

	AUDDBG("stream identified as MPEG; %ldHz %d channels, encoding %d\n",
		ctx.rate, ctx.channels, ctx.encoding);

	AUDDBG("opening audio\n");
	if (! data->output->open_audio (FMT_S16_NE, ctx.rate, ctx.channels))
	{
		data->error = TRUE;
		goto cleanup;
	}

	data->set_gain_from_playlist (data);

	g_mutex_lock(ctrl_mutex);

	AUDDBG("starting decode\n");
	data->playing = TRUE;
	data->set_pb_ready(data);

	g_mutex_unlock(ctrl_mutex);

	while (data->playing == TRUE)
	{
		gint16 outbuf[ctx.channels * (ctx.rate / 100)];
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

		if (ret < 0)
		{
			fprintf (stderr, "mpg123 error: %s\n", mpg123_plain_strerror (ret));

			if (++ error_count >= 10)
			{
				data->error = TRUE;
				goto decode_cleanup;
			}
		}
		else
			error_count = 0;

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
				fprintf (stderr, "mpg123 error: %s\n", mpg123_strerror (ctx.decoder));
				ctx.seek = -1;
				g_cond_signal (ctrl_cond);
				g_mutex_unlock(ctrl_mutex);
				continue;
			}

			AUDDBG("seeking to %ld (byte %ld)\n", ctx.seek, byteoff);
			data->output->flush(ctx.seek * 1000);
			aud_vfs_fseek(ctx.fd, byteoff, SEEK_SET);
			ctx.seek = -1;

			g_cond_signal(ctrl_cond);
		}

		g_mutex_unlock(ctrl_mutex);
	}

decode_cleanup:
	AUDDBG("eof reached\n");
	while (data->playing && data->output->buffer_playing())
		g_usleep(10000);

	AUDDBG("decode complete\n");
	g_mutex_lock (ctrl_mutex);
	data->playing = FALSE;
	g_cond_signal (ctrl_cond); /* wake up any waiting request */
	g_mutex_unlock (ctrl_mutex);

	data->output->close_audio();

cleanup:
	mpg123_delete(ctx.decoder);
	mpg123_delete_pars(ctx.params);
	aud_vfs_fclose(ctx.fd);
}

static void mpg123_stop_playback_worker (InputPlayback * data)
{
	g_mutex_lock (ctrl_mutex);

	if (data->playing)
	{
		data->output->abort_write ();
		data->playing = FALSE;
		g_cond_signal (ctrl_cond);
	}

	g_mutex_unlock (ctrl_mutex);
}

static void mpg123_pause_playback_worker (InputPlayback * data, gshort pause)
{
	g_mutex_lock (ctrl_mutex);

	if (data->playing)
        data->output->pause (pause);

	g_mutex_unlock (ctrl_mutex);
}

static void mpg123_seek_time (InputPlayback * data, gint time)
{
	g_mutex_lock (ctrl_mutex);

	if (data->playing)
	{
		((MPG123PlaybackContext *) data->data)->seek = time;
		data->output->abort_write ();
		g_cond_signal (ctrl_cond);
		g_cond_wait (ctrl_cond, ctrl_mutex);
	}

	g_mutex_unlock (ctrl_mutex);
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
