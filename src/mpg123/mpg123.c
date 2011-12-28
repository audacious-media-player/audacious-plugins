/*
 * Copyright (c) 2010 William Pitcock <nenolod@dereferenced.org>.
 * Copyright (c) 2010-2011 John Lindgren <john.lindgren@tds.net>.
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

/* must come before <mpg123.h> to get _FILE_OFFSET_BITS */
#include "config.h"

#include <glib.h>
#include <string.h>
#include <mpg123.h>

#ifdef DEBUG_MPG123_IO
# define MPG123_IODBG(...)	AUDDBG(__VA_ARGS__)
#else
# define MPG123_IODBG(...)	do { } while (0)
#endif

#include <libaudcore/audstrings.h>
#include <audacious/debug.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <audacious/audtag.h>

/* Define to read all frame headers when calculating file length */
/* #define FULL_SCAN */

static GMutex *ctrl_mutex = NULL;
static GCond *ctrl_cond = NULL;

static ssize_t replace_read (void * file, void * buffer, size_t length)
{
	return vfs_fread (buffer, 1, length, file);
}

static off_t replace_lseek (void * file, off_t to, int whence)
{
	return (! vfs_fseek (file, to, whence)) ? vfs_ftell (file) : -1;
}

/** plugin glue **/
static gboolean aud_mpg123_init (void)
{
	AUDDBG("initializing mpg123 library\n");
	mpg123_init();

	AUDDBG("initializing control mutex\n");
	ctrl_mutex = g_mutex_new();
	ctrl_cond = g_cond_new();

	return TRUE;
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

static void set_format (mpg123_handle * dec)
{
	static const gint rates[] = {8000, 11025, 12000, 16000, 22050, 24000, 32000,
	 44100, 48000};

	mpg123_format_none (dec);
	for (gint i = 0; i < G_N_ELEMENTS (rates); i ++)
		mpg123_format (dec, rates[i], MPG123_MONO | MPG123_STEREO,
		 MPG123_ENC_FLOAT_32);
}

static void make_format_string (const struct mpg123_frameinfo * info, gchar *
 buf, gint bsize)
{
	static const gchar * vers[] = {"1", "2", "2.5"};
	snprintf (buf, bsize, "MPEG-%s layer %d", vers[info->version], info->layer);
}

static gboolean mpg123_probe_for_fd (const gchar * fname, VFSFile * file)
{
	if (! file)
		return FALSE;

	/* MPG123 likes to grab WMA streams, so blacklist anything that starts with
	 * mms://.  If there are mms:// streams out there carrying MP3, they will
	 * just have to play in ffaudio.  --jlindgren */
	if (! strncmp (fname, "mms://", 6))
		return FALSE;

	mpg123_handle * dec = mpg123_new (NULL, NULL);
	g_return_val_if_fail (dec, FALSE);
	mpg123_param (dec, MPG123_ADD_FLAGS, MPG123_QUIET, 0);
	mpg123_replace_reader_handle (dec, replace_read, replace_lseek, NULL);
	set_format (dec);

	gint res;
	if ((res = mpg123_open_handle (dec, file)) < 0)
	{
ERR:
		AUDDBG ("Probe error: %s\n", mpg123_plain_strerror (res));
		mpg123_delete (dec);
		return FALSE;
	}

#ifdef FULL_SCAN
	if (mpg123_scan (dec) < 0)
		goto ERR;
#endif

RETRY:;
	glong rate;
	gint chan, enc;
	if ((res = mpg123_getformat (dec, & rate, & chan, & enc)) < 0)
		goto ERR;

	struct mpg123_frameinfo info;
	if ((res = mpg123_info (dec, & info)) < 0)
		goto ERR;

	gfloat out[chan * (rate / 10)];
	size_t done;
	while ((res = mpg123_read (dec, (void *) out, sizeof out, & done)) < 0)
	{
		if (res == MPG123_NEW_FORMAT)
			goto RETRY;
		goto ERR;
	}

	gchar str[32];
	make_format_string (& info, str, sizeof str);
	AUDDBG ("Accepted as %s: %s.\n", str, fname);

	mpg123_delete (dec);
	return TRUE;
}

static Tuple * mpg123_probe_for_tuple (const gchar * filename, VFSFile * file)
{
	if (! file)
		return NULL;

	mpg123_handle * decoder = mpg123_new (NULL, NULL);
	gint result;
	glong rate;
	gint channels, encoding;
	struct mpg123_frameinfo info;
	gchar scratch[32];

	g_return_val_if_fail (decoder, NULL);
	mpg123_param (decoder, MPG123_ADD_FLAGS, MPG123_QUIET, 0);
	mpg123_replace_reader_handle (decoder, replace_read, replace_lseek, NULL);

	if ((result = mpg123_open_handle (decoder, file)) < 0)
		goto ERR;

#ifdef FULL_SCAN
	if (mpg123_scan (decoder) < 0)
		goto ERR;
#endif

	if ((result = mpg123_getformat (decoder, & rate, & channels, & encoding)) <
	 0)
		goto ERR;
	if ((result = mpg123_info (decoder, & info)) < 0)
		goto ERR;

	Tuple * tuple = tuple_new_from_filename (filename);
	make_format_string (& info, scratch, sizeof scratch);
	tuple_set_str (tuple, FIELD_CODEC, NULL, scratch);
	snprintf (scratch, sizeof scratch, "%s, %d Hz", (channels == 2)
	 ? _("Stereo") : (channels > 2) ? _("Surround") : _("Mono"), (gint) rate);
	tuple_set_str (tuple, FIELD_QUALITY, NULL, scratch);
	tuple_set_int (tuple, FIELD_BITRATE, NULL, info.bitrate);

	if (! vfs_is_streaming (file))
	{
		gint64 size = vfs_fsize (file);
		gint64 samples = mpg123_length (decoder);
		gint length = (samples > 0) ? samples * 1000 / rate : 0;

		if (length > 0)
			tuple_set_int (tuple, FIELD_LENGTH, NULL, length);
		if (size > 0 && length > 0)
			tuple_set_int (tuple, FIELD_BITRATE, NULL, 8 * size / length);
	}

	mpg123_delete (decoder);

	if (! vfs_is_streaming (file))
	{
		vfs_rewind (file);
		tag_tuple_read (tuple, file);
	}

	return tuple;

ERR:
	fprintf (stderr, "mpg123 probe error for %s: %s\n", filename, mpg123_plain_strerror (result));
	mpg123_delete (decoder);
	return NULL;
}

typedef struct {
	VFSFile *fd;
	mpg123_handle *decoder;
	glong rate;
	gint channels;
	gint encoding;
	gint64 seek;
	gboolean stop;
	gboolean stream;
	Tuple *tu;
} MPG123PlaybackContext;

static gchar *
get_stream_metadata(VFSFile *file, const gchar *name)
{
	gchar *raw = vfs_get_metadata(file, name);
	gchar *converted = (raw != NULL && raw[0]) ? str_to_utf8(raw) : NULL;

	g_free(raw);
	return converted;
}

static gboolean
update_stream_metadata(VFSFile *file, const gchar *name, Tuple *tuple, gint item)
{
	gchar *old = tuple_get_str(tuple, item, NULL);
	gchar *new = get_stream_metadata(file, name);
	gboolean changed = (new != NULL && (old == NULL || strcmp(old, new)));

	if (changed)
		tuple_set_str(tuple, item, NULL, new);

	g_free(new);
	str_unref(old);
	return changed;
}

static Tuple * get_stream_tuple (InputPlayback * p, const gchar * filename,
 VFSFile * file)
{
	Tuple * tuple = mpg123_probe_for_tuple (filename, file);
	if (! tuple)
		tuple = tuple_new_from_filename (filename);

	update_stream_metadata (file, "track-name", tuple, FIELD_TITLE);
	update_stream_metadata (file, "stream-name", tuple, FIELD_ARTIST);

	tuple_ref (tuple);
	p->set_tuple (p, tuple);

	return tuple;
}

static void update_stream_tuple (InputPlayback * p, VFSFile * file,
 Tuple * tuple)
{
	if (update_stream_metadata (file, "track-name", tuple, FIELD_TITLE) ||
	 update_stream_metadata (file, "stream-name", tuple, FIELD_ARTIST))
	{
		tuple_ref (tuple);
		p->set_tuple (p, tuple);
	}
}

static gboolean mpg123_playback_worker (InputPlayback * data, const gchar *
 filename, VFSFile * file, gint start_time, gint stop_time, gboolean pause)
{
	if (! file)
		return FALSE;

	gboolean error = FALSE;
	MPG123PlaybackContext ctx;
	gint ret;
	gint bitrate = 0, bitrate_sum = 0, bitrate_count = 0;
	gint bitrate_updated = -1000; /* >= a second away from any position */
	struct mpg123_frameinfo fi;
	gint error_count = 0;

	memset(&ctx, 0, sizeof(MPG123PlaybackContext));
	memset(&fi, 0, sizeof(struct mpg123_frameinfo));

	AUDDBG("playback worker started for %s\n", filename);
	ctx.fd = file;

	AUDDBG ("Checking for streaming ...\n");
	ctx.stream = vfs_is_streaming (file);
	ctx.tu = ctx.stream ? get_stream_tuple (data, filename, file) : NULL;

	ctx.seek = (start_time > 0) ? start_time : -1;
	ctx.stop = FALSE;
	data->set_data (data, & ctx);

	ctx.decoder = mpg123_new (NULL, NULL);
	g_return_val_if_fail (ctx.decoder, FALSE);
	mpg123_param (ctx.decoder, MPG123_ADD_FLAGS, MPG123_QUIET, 0);
	mpg123_param (ctx.decoder, MPG123_ADD_FLAGS, MPG123_GAPLESS, 0);
	mpg123_param (ctx.decoder, MPG123_ADD_FLAGS, MPG123_SEEKBUFFER, 0);
	mpg123_replace_reader_handle (ctx.decoder, replace_read, replace_lseek, NULL);
	set_format (ctx.decoder);

	if (mpg123_open_handle (ctx.decoder, file) < 0)
	{
OPEN_ERROR:
		fprintf (stderr, "mpg123: Error opening %s: %s.\n", filename,
		 mpg123_strerror (ctx.decoder));
		error = TRUE;
		goto cleanup;
	}

	gfloat outbuf[8192];
	size_t outbuf_size = 0;

#ifdef FULL_SCAN
	if (mpg123_scan (ctx.decoder) < 0)
		goto OPEN_ERROR;
#endif

GET_FORMAT:
	if (mpg123_getformat (ctx.decoder, & ctx.rate, & ctx.channels,
	 & ctx.encoding) < 0)
		goto OPEN_ERROR;

	while ((ret = mpg123_read (ctx.decoder, (void *) outbuf, sizeof outbuf,
	 & outbuf_size)) < 0)
	{
		if (ret == MPG123_NEW_FORMAT)
			goto GET_FORMAT;
		goto OPEN_ERROR;
	}

	if (mpg123_info (ctx.decoder, & fi) < 0)
		goto OPEN_ERROR;

	bitrate = fi.bitrate * 1000;
	data->set_params (data, bitrate, ctx.rate, ctx.channels);

	if (! data->output->open_audio (FMT_FLOAT, ctx.rate, ctx.channels))
	{
		error = TRUE;
		goto cleanup;
	}

	if (pause)
		data->output->pause (TRUE);

	data->set_gain_from_playlist (data);

	g_mutex_lock(ctrl_mutex);

	AUDDBG("starting decode\n");
	data->set_pb_ready(data);

	g_mutex_unlock(ctrl_mutex);

	gint64 frames_played = 0;
	gint64 frames_total = (stop_time - start_time) * ctx.rate / 1000;

	while (1)
	{
		g_mutex_lock (ctrl_mutex);

		if (ctx.stop)
		{
			g_mutex_unlock (ctrl_mutex);
			break;
		}

		if (ctx.seek >= 0)
		{
			if (mpg123_seek (ctx.decoder, (gint64) ctx.seek * ctx.rate / 1000,
			 SEEK_SET) < 0)
			{
				fprintf (stderr, "mpg123 error in %s: %s\n", filename,
				 mpg123_strerror (ctx.decoder));
				ctx.seek = -1;
				g_cond_signal (ctrl_cond);
				g_mutex_unlock (ctrl_mutex);
			}
			else
			{
				data->output->flush (ctx.seek);
				frames_played = (ctx.seek - start_time) * ctx.rate / 1000;
				outbuf_size = 0;
				ctx.seek = -1;
			}

			g_cond_signal (ctrl_cond);
		}

		g_mutex_unlock (ctrl_mutex);

		mpg123_info(ctx.decoder, &fi);
		bitrate_sum += fi.bitrate;
		bitrate_count ++;

		if (bitrate_sum / bitrate_count != bitrate && abs
		 (data->output->written_time () - bitrate_updated) >= 1000)
		{
			data->set_params (data, bitrate_sum / bitrate_count * 1000,
			 ctx.rate, ctx.channels);
			bitrate = bitrate_sum / bitrate_count;
			bitrate_sum = 0;
			bitrate_count = 0;
			bitrate_updated = data->output->written_time ();
		}

		if (ctx.stream)
			update_stream_tuple (data, file, ctx.tu);

		if (! outbuf_size && (ret = mpg123_read (ctx.decoder, (void *) outbuf,
		 sizeof outbuf, & outbuf_size)) < 0)
		{
			if (ret == MPG123_DONE || ret == MPG123_ERR_READER)
				goto decode_cleanup;

			fprintf (stderr, "mpg123 error in %s: %s\n", filename,
			 mpg123_strerror (ctx.decoder));

			if (++ error_count >= 10)
			{
				error = TRUE;
				goto decode_cleanup;
			}
		}
		else
		{
			error_count = 0;

			gboolean stop = FALSE;

			if (stop_time >= 0)
			{
				gint64 remain = sizeof outbuf[0] * ctx.channels * (frames_total - frames_played);
				remain = MAX (0, remain);

				if (outbuf_size >= remain)
				{
					outbuf_size = remain;
					stop = TRUE;
				}
			}

			data->output->write_audio (outbuf, outbuf_size);
			frames_played += outbuf_size / (sizeof outbuf[0] * ctx.channels);
			outbuf_size = 0;

			if (stop)
				goto decode_cleanup;
		}
	}

decode_cleanup:
	while (data->output->buffer_playing ())
		g_usleep(10000);

	AUDDBG("decode complete\n");
	g_mutex_lock (ctrl_mutex);
	data->set_data (data, NULL);
	g_cond_signal (ctrl_cond); /* wake up any waiting request */
	g_mutex_unlock (ctrl_mutex);

	data->output->close_audio();

cleanup:
	mpg123_delete(ctx.decoder);
	if (ctx.tu)
		tuple_unref (ctx.tu);
	return ! error;
}

static void mpg123_stop_playback_worker (InputPlayback * data)
{
	g_mutex_lock (ctrl_mutex);
	MPG123PlaybackContext * context = data->get_data (data);

	if (context != NULL)
	{
		context->stop = TRUE;
		data->output->abort_write ();
		g_cond_signal (ctrl_cond);
	}

	g_mutex_unlock (ctrl_mutex);
}

static void mpg123_pause_playback_worker (InputPlayback * data, gboolean pause)
{
	g_mutex_lock (ctrl_mutex);
	MPG123PlaybackContext * context = data->get_data (data);

	if (context != NULL)
		data->output->pause (pause);

	g_mutex_unlock (ctrl_mutex);
}

static void mpg123_seek_time (InputPlayback * data, gint time)
{
	g_mutex_lock (ctrl_mutex);
	MPG123PlaybackContext * context = data->get_data (data);

	if (context != NULL)
	{
		context->seek = time;
		data->output->abort_write ();
		g_cond_signal (ctrl_cond);
		g_cond_wait (ctrl_cond, ctrl_mutex);
	}

	g_mutex_unlock (ctrl_mutex);
}

static gboolean mpg123_write_tag (const Tuple * tuple, VFSFile * handle)
{
	if (! handle)
		return FALSE;

	return tag_tuple_write (tuple, handle, TAG_TYPE_APE);
}

static gboolean mpg123_get_image (const gchar * filename, VFSFile * handle,
 void * * data, gint64 * length)
{
	if (! handle || vfs_is_streaming (handle))
		return FALSE;

	return tag_image_read (handle, data, length);
}

/** plugin description header **/
static const gchar *mpg123_fmts[] = { "mp3", "mp2", "mp1", "bmu", NULL };

AUD_INPUT_PLUGIN
(
	.init = aud_mpg123_init,
	.cleanup = aud_mpg123_deinit,
	.name = "MPG123",
	.extensions = mpg123_fmts,
	.is_our_file_from_vfs = mpg123_probe_for_fd,
	.probe_for_tuple = mpg123_probe_for_tuple,
	.play = mpg123_playback_worker,
	.stop = mpg123_stop_playback_worker,
	.mseek = mpg123_seek_time,
	.pause = mpg123_pause_playback_worker,
	.update_song_tuple = mpg123_write_tag,
	.get_song_image = mpg123_get_image,
)
