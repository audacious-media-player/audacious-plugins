/*
 * mad plugin for audacious
 * Copyright (C) 2005-2009 William Pitcock, Yoshiki Yazawa, John Lindgren
 *
 * Portions derived from xmms-mad:
 * Copyright (C) 2001-2002 Sam Clegg - See COPYING
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <math.h>
#include <assert.h>

#include <audacious/plugin.h>
#include "plugin.h"
#include "input.h"

#define BUFFER_SIZE (16*1024)
#define N_AVERAGE_FRAMES 10

#define error(...) fprintf (stderr, "madplug: " __VA_ARGS__)

static void write_output (struct mad_info_t * info, struct mad_pcm * pcm,
 struct mad_header * header)
{
    gint channels = MAD_NCHANNELS (header);
    gfloat * data = g_malloc (sizeof (gfloat) * channels * pcm->length);
    gfloat * end = data + channels * pcm->length;
    gint channel;

    for (channel = 0; channel < channels; channel ++)
    {
        const mad_fixed_t * from = pcm->samples[channel];
        gfloat * to = data + channel;

        while (to < end)
        {
            * to = (gfloat) (* from ++) / (1 << 28);
            to += channels;
        }
    }

    info->playback->pass_audio (info->playback, FMT_FLOAT, channels, sizeof
     (gfloat) * channels * pcm->length, data, NULL);
    g_free (data);
}

/**
 * Decode all headers in the file and fill in stats
 * @return FALSE if scan failed.
 */
gboolean
scan_file(struct mad_info_t * info, gboolean fast)
{
    struct mad_stream stream;
    struct mad_header header;
    int remainder = 0;
    int data_used = 0;
    int len = 0;
    int tagsize = 0;
    unsigned char buffer[BUFFER_SIZE];
    struct mad_frame frame;     /* to read xing data */
    guint bitrate_frames = 0;
    double xing_bitrate = 0.0;
    double accum_bitrate = 0.0;
    mad_timer_t timer = mad_timer_zero;

    mad_stream_init(&stream);
    mad_stream_options(&stream, 0); // check CRC
    mad_header_init(&header);
    mad_frame_init(&frame);
    xing_init(&info->xing);

    info->bitrate = 0;

    while (1) {
        remainder = stream.bufend - stream.next_frame;

        if(buffer != stream.this_frame && remainder)
            memmove(buffer, stream.this_frame, remainder);

        len = input_get_data(info, buffer + remainder,
                             BUFFER_SIZE - remainder);

        if (len <= 0) {
            AUDDBG("scan_file: len <= 0 len = %d\n", len);
            break;
        }

        mad_stream_buffer(&stream, buffer, remainder + len);

        while (!fast || (fast && info->frames < N_AVERAGE_FRAMES)) {
            if (mad_header_decode(&header, &stream) == -1) {
                if (stream.error == MAD_ERROR_BUFLEN) {
                    break;
                }
                if (!MAD_RECOVERABLE(stream.error)) {
                    AUDDBG("(fatal) error decoding header %d: %s\n",
                              info->frames, mad_stream_errorstr(&stream));
                    AUDDBG("remainder = %d\n", remainder);
                    AUDDBG("len = %d\n", len);
                    break;
                }
                if (stream.error == MAD_ERROR_LOSTSYNC) {
                    /* ignore LOSTSYNC due to ID3 tags */
                    tagsize = id3_tag_query(stream.this_frame,
                                            stream.bufend -
                                            stream.this_frame);
                    if (tagsize > 0) {
                        AUDDBG("skipping id3_tag: %d\n", tagsize);
                        mad_stream_skip(&stream, tagsize);
                        continue;
                    }
                }

                AUDDBG("(recovered) error decoding header %d: %s\n",
                          info->frames, mad_stream_errorstr(&stream));
                AUDDBG("remainder = %d\n", remainder);
                AUDDBG("len = %d\n", len);

                continue;
            }
            info->frames++;

            mad_timer_add (& timer, header.duration);

            data_used += stream.next_frame - stream.this_frame;
            if (info->frames == 1) {
                /* most of these *should* remain constant */
                info->bitrate = header.bitrate;
                info->freq = header.samplerate;
                info->channels = MAD_NCHANNELS(&header);
                info->mpeg_layer = header.layer;
                info->mode = header.mode;

                if (audmad_config->use_xing) {
                    frame.header = header;
                    if (mad_frame_decode(&frame, &stream) == -1) {
                        AUDDBG("xing frame decode failed\n");
                        goto no_xing;
                    }
                    if (xing_parse(&info->xing, stream.anc_ptr, stream.anc_bitlen) == 0) {
                        AUDDBG("xing header found\n");
                        info->vbr = TRUE;   /* otherwise xing header would have been 'Info' */

                        AUDDBG("xing: bytes = %ld frames = %ld\n", info->xing.bytes, info->xing.frames);

                        /* we have enough info to calculate bitrate and duration */
                        if(info->xing.bytes && info->xing.frames) {
                            xing_bitrate = 8 * (double)info->xing.bytes * 38 / (double)info->xing.frames; //38fps in MPEG1.
#ifdef DEBUG
                            {
                                gint tmp = (gint)(info->xing.bytes * 8 / xing_bitrate);
                                AUDDBG("xing: bitrate = %4.1f kbps\n", xing_bitrate / 1000);
                                AUDDBG("xing: duration = %d:%02d\n", tmp / 60, tmp % 60);
                            }
#endif
                        }
                        continue;
                    }
#ifdef DEBUG
                    else {
                        AUDDBG("no usable xing header\n");
                        continue;
                    }
#endif
                } /* xing */

            }
            else {
                /* perhaps we have a VBR file */
                if (info->bitrate != header.bitrate)
                    info->vbr = TRUE;
                if (info->vbr) {
                    accum_bitrate += (double)header.bitrate;
                    bitrate_frames++;
                }
                /* check for changin layer/samplerate/channels */
                if (info->mpeg_layer != header.layer)
                    g_warning("layer varies!!");
                if (info->freq != header.samplerate)
                    g_warning("samplerate varies!!");
                if (info->channels != MAD_NCHANNELS(&header))
                    g_warning("number of channels varies!!");
            }
        no_xing:
            if (fast && info->frames >= N_AVERAGE_FRAMES) {
                if (xing_bitrate > 0)
                    info->length = (gfloat) info->xing.bytes * 8000 /
                     xing_bitrate;
                else if (info->size > 0)
                    info->length = (gint64) mad_timer_count (timer,
                     MAD_UNITS_MILLISECONDS) * info->size / data_used;

                break;
            }
        } /* while */
        if (stream.error != MAD_ERROR_BUFLEN)
            break;
    }

    if (info->xing.frames)
        info->frames = info->xing.frames;

    if (info->vbr && xing_bitrate != 0) {
        info->bitrate = (guint)xing_bitrate;
    }
    else if (info->vbr && xing_bitrate == 0 && bitrate_frames != 0) {
        info->bitrate = accum_bitrate / bitrate_frames;
    }

    aud_tuple_associate_int(info->tuple, FIELD_BITRATE, NULL, info->bitrate / 1000);

    mad_frame_finish(&frame);
    mad_header_finish(&header);
    mad_stream_finish(&stream);
    xing_finish(&info->xing);

    return (info->frames > 0);
}

static void seek (struct mad_info_t * info)
{
    if (info->length <= 0)
        goto DONE;

    if (aud_vfs_fseek (info->infile, info->size * (long long) info->seek /
     info->length, SEEK_SET))
    {
        error ("aud_vfs_fseek failed.\n");
        goto DONE;
    }

    mad_stream_buffer (info->stream, info->buffer, 0);
    info->resync = 1;
    info->playback->output->flush (info->seek);

DONE:
    info->seek = -1;
}

static char fill_buffer (struct mad_info_t * info)
{
    int remains, readed;

    remains = info->stream->bufend - info->stream->this_frame;
    memmove (info->buffer, info->stream->this_frame, remains);
    readed = aud_vfs_fread (info->buffer + remains, 1, info->buffer_size -
     remains, info->infile);

    if (readed < 0)
    {
        error ("aud_vfs_read failed.\n");
        readed = 0;
    }

    mad_stream_buffer (info->stream, info->buffer, remains + readed);
    return (readed > 0);
}

gpointer
decode_loop(gpointer arg)
{
    int skip, current;
    unsigned int iteration = 0;
    gboolean paused = FALSE;
    ReplayGainInfo gain;

    /* mad structs */
    struct mad_stream stream;
    struct mad_frame frame;
    struct mad_synth synth;

    /* track info is passed in as thread argument */
    struct mad_info_t *info = (struct mad_info_t *) arg;

    AUDDBG("f: decode\n");

    /* init mad stuff */
    mad_frame_init(&frame);
    mad_stream_init(&stream);
    mad_stream_options(&stream, MAD_OPTION_IGNORECRC);
    mad_synth_init(&synth);

    info->resync = 0;
    info->buffer = malloc (1024);
    info->buffer_size = 1024;
    mad_stream_buffer (& stream, info->buffer, 0);
    info->stream = & stream;

    if (! info->playback->output->open_audio (FMT_FLOAT, info->freq,
     info->channels))
    {
        info->playback->error = TRUE;
        goto CLEAN_UP;
    }

    gain.track_gain = info->replaygain_track_scale;
    gain.track_peak = info->replaygain_track_peak;
    gain.album_gain = info->replaygain_album_scale;
    gain.album_peak = info->replaygain_album_peak;
    info->playback->output->set_replaygain_info (& gain);

    while (info->playback->playing)
    {
        g_mutex_lock (control_mutex);

        if (info->seek >= 0)
        {
            seek (info);
            g_cond_signal (control_cond);
        }

        if (info->pause != paused)
        {
            info->playback->output->pause (info->pause);
            paused = info->pause;
            g_cond_signal (control_cond);
        }

        if (paused)
        {
            g_cond_wait (control_cond, control_mutex);
            g_mutex_unlock (control_mutex);
            continue;
        }

        g_mutex_unlock (control_mutex);

        input_process_remote_metadata (info);

        if (info->resync)
            stream.sync = 0;

    RETRY:
        if (mad_header_decode (& frame.header, & stream) == -1)
            goto ERROR;

        info->bitrate = frame.header.bitrate;

        if (info->vbr && (iteration % 40 == 0))
        {
            info->playback->set_params (info->playback, NULL, 0,
                info->bitrate, info->freq, info->channels);
        }

        iteration ++;

        if (mad_frame_decode (& frame, & stream) == -1)
            goto ERROR;

        if (info->freq != frame.header.samplerate || info->channels !=
         MAD_NCHANNELS (& frame.header))
        {
            current = info->playback->output->written_time ();
            info->playback->output->close_audio ();

            info->freq = frame.header.samplerate;
            info->channels = MAD_NCHANNELS (& frame.header);

            if (! info->playback->output->open_audio (FMT_FLOAT, info->freq,
             info->channels))
            {
                info->playback->error = TRUE;
                goto CLEAN_UP;
            }

            info->playback->output->set_replaygain_info (& gain);
            info->playback->output->flush (current);
        }

        mad_synth_frame (& synth, & frame);
        write_output (info, & synth.pcm, & frame.header);
        info->resync = 0;
        continue;

    ERROR:
        // Not really an error; just need more data.
        if (stream.error == MAD_ERROR_BUFLEN)
        {
            int buffered = stream.bufend - stream.this_frame;

            if (buffered < info->buffer_size / 2)
            {
                fill_buffer (info);

                if (stream.bufend - stream.this_frame == buffered)
                    break;
            }
            else
            {
                int offset = stream.this_frame - info->buffer;

                info->buffer_size *= 2;
                info->buffer = realloc (info->buffer, info->buffer_size);
                mad_stream_buffer (& stream, info->buffer + offset, buffered);
            }

            continue;
        }

        // Did we get confused by a false sync marker?
        if (info->resync && MAD_RECOVERABLE (stream.error))
            goto RETRY;

        // Did we hit an ID3 tag?
        if (stream.error == MAD_ERROR_LOSTSYNC)
        {
            skip = id3_tag_query (stream.this_frame, stream.bufend -
             stream.this_frame);

            if (skip > 0)
            {
                if (skip > stream.bufend - stream.this_frame)
                {
                    if (aud_vfs_fseek (info->infile, skip - (stream.bufend -
                     stream.this_frame), SEEK_CUR))
                        error ("aud_vfs_fseek failed.\n");

                    mad_stream_buffer (& stream, info->buffer, 0);
                }
                else
                    mad_stream_skip (& stream, skip);

                goto RETRY;
            }
            else if (!info->resync)
            {
                unsigned char const *ptr = stream.this_frame;
                int framelength = 144 * info->bitrate / info->freq;
                ptr += 4;
                for (skip = 0; skip < framelength; skip++)
                {
                    if (memcmp(ptr,MPEG_SYNC_MARKER,2) == 0)
                        break;
                    ptr += 1;
                }
                error("Skipping %i bytes over broken frame\n", skip);
                mad_stream_skip (& stream, skip);
                info->resync = 1;
                goto RETRY;
            }
        }

        error ("%s.\n", mad_stream_errorstr (& stream));

        if (MAD_RECOVERABLE (stream.error))
            goto RETRY;

        break;
    }

    while (info->playback->playing && info->playback->output->buffer_playing ())
        g_usleep (50000);

    info->playback->output->close_audio ();

CLEAN_UP:
    free (info->buffer);
    mad_frame_finish (& frame);
    mad_stream_finish (& stream);
    mad_synth_finish (& synth);

    aud_tuple_free(info->tuple);
    info->tuple = NULL;

    g_mutex_lock(mad_mutex);
    info->playback->playing = 0;
    g_mutex_unlock(mad_mutex);
    return NULL; /* dummy */
}
