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

#include <string.h>

#include <mpg123.h>

#include "prefs.h"

#ifdef DEBUG_MPG123_IO
# define MPG123_IODBG(...)	AUDDBG(__VA_ARGS__)
#else
# define MPG123_IODBG(...)	do { } while (0)
#endif

#include <libaudcore/audstrings.h>
#include <audacious/debug.h>
#include <audacious/i18n.h>
#include <audacious/input.h>
#include <audacious/plugin.h>
#include <audacious/preferences.h>
#include <audacious/audtag.h>

#define DECODE_OPTIONS (MPG123_QUIET | MPG123_GAPLESS | MPG123_SEEKBUFFER | MPG123_FUZZY)

static ssize_t replace_read (void * file, void * buffer, size_t length)
{
	return vfs_fread (buffer, 1, length, file);
}

static off_t replace_lseek (void * file, off_t to, int whence)
{
	return (! vfs_fseek (file, to, whence)) ? vfs_ftell (file) : -1;
}

static off_t replace_lseek_dummy (void * file, off_t to, int whence)
{
	return -1;
}

/** plugin glue **/
static bool_t aud_mpg123_init (void)
{
	AUDDBG("initializing mpg123 library\n");
	mpg123_init();

	mpg123_cfg_load ();

	return TRUE;
}

static void
aud_mpg123_deinit(void)
{
	mpg123_cfg_save ();
	AUDDBG("deinitializing mpg123 library\n");
	mpg123_exit();
}

static void set_format (mpg123_handle * dec)
{
	static const int rates[] = {8000, 11025, 12000, 16000, 22050, 24000, 32000,
	 44100, 48000};

	mpg123_format_none (dec);
	for (int i = 0; i < ARRAY_LEN (rates); i ++)
		mpg123_format (dec, rates[i], MPG123_MONO | MPG123_STEREO,
		 MPG123_ENC_FLOAT_32);
}

static void make_format_string (const struct mpg123_frameinfo * info, char *
 buf, int bsize)
{
	static const char * vers[] = {"1", "2", "2.5"};
	snprintf (buf, bsize, "MPEG-%s layer %d", vers[info->version], info->layer);
}

static bool_t mpg123_probe_for_fd (const char * fname, VFSFile * file)
{
	if (! file)
		return FALSE;

	/* MPG123 likes to grab WMA streams, so blacklist anything that starts with
	 * mms://.  If there are mms:// streams out there carrying MP3, they will
	 * just have to play in ffaudio.  --jlindgren */
	if (! strncmp (fname, "mms://", 6))
		return FALSE;

	bool_t is_streaming = vfs_is_streaming (file);

	/* Some MP3s begin with enormous ID3 tags, which fill up the whole probe
	 * buffer and thus hide any MP3 content.  As a workaround, assume that an
	 * ID3 tag means an MP3 file.  --jlindgren */
	if (! is_streaming)
	{
		char id3buf[3];
		if (vfs_fread (id3buf, 1, 3, file) != 3)
			return FALSE;

		if (! memcmp (id3buf, "ID3", 3))
			return TRUE;

		if (vfs_fseek (file, 0, SEEK_SET) < 0)
			return FALSE;
	}

	mpg123_handle * dec = mpg123_new (NULL, NULL);
	mpg123_param (dec, MPG123_ADD_FLAGS, DECODE_OPTIONS, 0);

	if (is_streaming)
		mpg123_replace_reader_handle (dec, replace_read, replace_lseek_dummy, NULL);
	else
		mpg123_replace_reader_handle (dec, replace_read, replace_lseek, NULL);

	set_format (dec);

	int res;
	if ((res = mpg123_open_handle (dec, file)) < 0)
	{
ERR:
		AUDDBG ("Probe error: %s\n", mpg123_plain_strerror (res));
		mpg123_delete (dec);
		return FALSE;
	}

	if (mpg123cfg.full_scan)
		if (mpg123_scan (dec) < 0)
			goto ERR;

RETRY:;
	long rate;
	int chan, enc;
	if ((res = mpg123_getformat (dec, & rate, & chan, & enc)) < 0)
		goto ERR;

	struct mpg123_frameinfo info;
	if ((res = mpg123_info (dec, & info)) < 0)
		goto ERR;

	float out[chan * (rate / 10)];
	size_t done;
	while ((res = mpg123_read (dec, (void *) out, sizeof out, & done)) < 0)
	{
		if (res == MPG123_NEW_FORMAT)
			goto RETRY;
		goto ERR;
	}

	char str[32];
	make_format_string (& info, str, sizeof str);
	AUDDBG ("Accepted as %s: %s.\n", str, fname);

	mpg123_delete (dec);
	return TRUE;
}

static Tuple * mpg123_probe_for_tuple (const char * filename, VFSFile * file)
{
	if (! file)
		return NULL;

	bool_t stream = vfs_is_streaming (file);
	mpg123_handle * decoder = mpg123_new (NULL, NULL);
	int result;
	long rate;
	int channels, encoding;
	struct mpg123_frameinfo info;
	char scratch[32];

	mpg123_param (decoder, MPG123_ADD_FLAGS, DECODE_OPTIONS, 0);

	if (stream)
		mpg123_replace_reader_handle (decoder, replace_read, replace_lseek_dummy, NULL);
	else
		mpg123_replace_reader_handle (decoder, replace_read, replace_lseek, NULL);

	if ((result = mpg123_open_handle (decoder, file)) < 0)
		goto ERR;

	if (mpg123cfg.full_scan)
		if (mpg123_scan (decoder) < 0)
			goto ERR;

	if ((result = mpg123_getformat (decoder, & rate, & channels, & encoding)) <
	 0)
		goto ERR;
	if ((result = mpg123_info (decoder, & info)) < 0)
		goto ERR;

	Tuple * tuple = tuple_new_from_filename (filename);
	make_format_string (& info, scratch, sizeof scratch);
	tuple_set_str (tuple, FIELD_CODEC, scratch);
	snprintf (scratch, sizeof scratch, "%s, %d Hz", (channels == 2)
	 ? _("Stereo") : (channels > 2) ? _("Surround") : _("Mono"), (int) rate);
	tuple_set_str (tuple, FIELD_QUALITY, scratch);
	tuple_set_int (tuple, FIELD_BITRATE, info.bitrate);

	if (! stream)
	{
		int64_t size = vfs_fsize (file);
		int64_t samples = mpg123_length (decoder);
		int length = (samples > 0 && rate > 0) ? samples * 1000 / rate : 0;

		if (length > 0)
			tuple_set_int (tuple, FIELD_LENGTH, length);
		if (size > 0 && length > 0)
			tuple_set_int (tuple, FIELD_BITRATE, 8 * size / length);
	}

	mpg123_delete (decoder);

	if (! stream && ! vfs_fseek (file, 0, SEEK_SET))
		tag_tuple_read (tuple, file);

	return tuple;

ERR:
	fprintf (stderr, "mpg123 probe error for %s: %s\n", filename, mpg123_plain_strerror (result));
	mpg123_delete (decoder);
	return NULL;
}

typedef struct {
	VFSFile *fd;
	mpg123_handle *decoder;
	long rate;
	int channels;
	int encoding;
	bool_t stream;
	Tuple *tu;
} MPG123PlaybackContext;

static bool_t
update_stream_metadata(VFSFile *file, const char *name, Tuple *tuple, int item)
{
	char *old = tuple_get_str(tuple, item);
	char *new = vfs_get_metadata(file, name);
	bool_t changed = (new != NULL && (old == NULL || strcmp(old, new)));

	if (changed)
		tuple_set_str(tuple, item, new);

	str_unref(new);
	str_unref(old);
	return changed;
}

static Tuple * get_stream_tuple (const char * filename, VFSFile * file)
{
	Tuple * tuple = mpg123_probe_for_tuple (filename, file);
	if (! tuple)
		tuple = tuple_new_from_filename (filename);

	update_stream_metadata (file, "track-name", tuple, FIELD_TITLE);
	update_stream_metadata (file, "stream-name", tuple, FIELD_ARTIST);

	tuple_ref (tuple);
	aud_input_set_tuple (tuple);

	return tuple;
}

static void update_stream_tuple (VFSFile * file, Tuple * tuple)
{
	if (update_stream_metadata (file, "track-name", tuple, FIELD_TITLE) ||
	 update_stream_metadata (file, "stream-name", tuple, FIELD_ARTIST))
	{
		tuple_ref (tuple);
		aud_input_set_tuple (tuple);
	}
}

static void print_mpg123_error (const char * filename, mpg123_handle * decoder)
{
	fprintf (stderr, "mpg123 error in %s: %s\n", filename, mpg123_strerror (decoder));
}

static bool_t mpg123_playback_worker (const char * filename, VFSFile * file)
{
	bool_t error = FALSE;
	MPG123PlaybackContext ctx;
	int ret;
	int bitrate = 0, bitrate_sum = 0, bitrate_count = 0;
	int bitrate_updated = -1000; /* >= a second away from any position */
	struct mpg123_frameinfo fi;
	int error_count = 0;

	memset(&ctx, 0, sizeof(MPG123PlaybackContext));
	memset(&fi, 0, sizeof(struct mpg123_frameinfo));

	AUDDBG("playback worker started for %s\n", filename);
	ctx.fd = file;

	AUDDBG ("Checking for streaming ...\n");
	ctx.stream = vfs_is_streaming (file);
	ctx.tu = ctx.stream ? get_stream_tuple (filename, file) : NULL;

	ctx.decoder = mpg123_new (NULL, NULL);
	mpg123_param (ctx.decoder, MPG123_ADD_FLAGS, DECODE_OPTIONS, 0);

	if (ctx.stream)
		mpg123_replace_reader_handle (ctx.decoder, replace_read, replace_lseek_dummy, NULL);
	else
		mpg123_replace_reader_handle (ctx.decoder, replace_read, replace_lseek, NULL);

	set_format (ctx.decoder);

	if (mpg123_open_handle (ctx.decoder, file) < 0)
	{
OPEN_ERROR:
		print_mpg123_error (filename, ctx.decoder);
		error = TRUE;
		goto cleanup;
	}

	float outbuf[8192];
	size_t outbuf_size = 0;

	if (mpg123cfg.full_scan)
		if (mpg123_scan (ctx.decoder) < 0)
			goto OPEN_ERROR;

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
	aud_input_set_bitrate (bitrate);

	if (! aud_input_open_audio (FMT_FLOAT, ctx.rate, ctx.channels))
	{
		error = TRUE;
		goto cleanup;
	}

	while (! aud_input_check_stop ())
	{
		int seek = aud_input_check_seek ();

		if (seek >= 0)
		{
			if (mpg123_seek (ctx.decoder, (int64_t) seek * ctx.rate / 1000, SEEK_SET) < 0)
				print_mpg123_error (filename, ctx.decoder);

			outbuf_size = 0;
		}

		mpg123_info(ctx.decoder, &fi);
		bitrate_sum += fi.bitrate;
		bitrate_count ++;

		if (bitrate_sum / bitrate_count != bitrate && abs
		 (aud_input_written_time () - bitrate_updated) >= 1000)
		{
			aud_input_set_bitrate (bitrate_sum / bitrate_count * 1000);
			bitrate = bitrate_sum / bitrate_count;
			bitrate_sum = 0;
			bitrate_count = 0;
			bitrate_updated = aud_input_written_time ();
		}

		if (ctx.stream)
			update_stream_tuple (file, ctx.tu);

		if (! outbuf_size && (ret = mpg123_read (ctx.decoder, (void *) outbuf,
		 sizeof outbuf, & outbuf_size)) < 0)
		{
			if (ret == MPG123_DONE || ret == MPG123_ERR_READER)
				break;

			print_mpg123_error (filename, ctx.decoder);

			if (++ error_count >= 10)
			{
				error = TRUE;
				break;
			}
		}
		else
		{
			error_count = 0;

			aud_input_write_audio (outbuf, outbuf_size);
			outbuf_size = 0;
		}
	}

cleanup:
	mpg123_delete(ctx.decoder);
	if (ctx.tu)
		tuple_unref (ctx.tu);
	return ! error;
}

static bool_t mpg123_write_tag (const char * filename, VFSFile * handle, const Tuple * tuple)
{
	if (! handle)
		return FALSE;

	return tag_tuple_write (tuple, handle, TAG_TYPE_APE);
}

static bool_t mpg123_get_image (const char * filename, VFSFile * handle,
 void * * data, int64_t * length)
{
	if (! handle || vfs_is_streaming (handle))
		return FALSE;

	return tag_image_read (handle, data, length);
}

/** plugin description header **/
static const char *mpg123_fmts[] = { "mp3", "mp2", "mp1", "bmu", NULL };


/** Prefs **/
static const PreferencesWidget mpg123_widgets[] = {
	{WIDGET_LABEL, N_("<b>mpg123</b>")},
	{WIDGET_CHK_BTN, N_("Use full scan (fixes wrong length detection)"),
		.cfg_type = VALUE_BOOLEAN, .cfg = & mpg123cfg.full_scan}
};

static const PluginPreferences mpg123_prefs = {
	.widgets = mpg123_widgets,
	.n_widgets = ARRAY_LEN (mpg123_widgets)
};


AUD_INPUT_PLUGIN
(
	.name = N_("MPG123 Plugin"),
	.domain = PACKAGE,
	.init = aud_mpg123_init,
	.cleanup = aud_mpg123_deinit,
	.prefs = & mpg123_prefs,
	.extensions = mpg123_fmts,
	.is_our_file_from_vfs = mpg123_probe_for_fd,
	.probe_for_tuple = mpg123_probe_for_tuple,
	.play = mpg123_playback_worker,
	.update_song_tuple = mpg123_write_tag,
	.get_song_image = mpg123_get_image,
)
