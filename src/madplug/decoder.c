/*
 * mad plugin for audacious
 * Copyright (C) 2005-2007 William Pitcock, Yoshiki Yazawa
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

/* #define AUD_DEBUG 1 */

#include <math.h>
#include <assert.h>

#include <audacious/plugin.h>
#include "plugin.h"
#include "input.h"

#define BUFFER_SIZE (16*1024)
#define N_AVERAGE_FRAMES 10

#define error(...) fprintf (stderr, "madplug: " __VA_ARGS__)

void
write_output(struct mad_info_t *info, struct mad_pcm *pcm,
             struct mad_header *header)
{
    unsigned int nsamples;
    mad_fixed_t const *left_ch, *right_ch;
    mad_fixed_t *output;
    int outlen = 0;
    int outbyte = 0;
    int pos = 0;

    nsamples = pcm->length;
    left_ch = pcm->samples[0];
    right_ch = pcm->samples[1];
    outlen = nsamples * MAD_NCHANNELS(header);
    outbyte = outlen * sizeof(mad_fixed_t);

    output = (mad_fixed_t *) g_malloc(outbyte);

    while (nsamples--) {
        output[pos++] = *left_ch++;

        if (MAD_NCHANNELS(header) == 2) {
            output[pos++] = *right_ch++;
        }
    }

    assert(pos == outlen);
    if (!info->playback->playing) {
        g_free(output);
        return;
    }

    info->playback->pass_audio(info->playback,
                               info->fmt, MAD_NCHANNELS(header), outbyte, output,
                               &(info->playback->playing));
    g_free(output);
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

    mad_stream_init(&stream);
    mad_stream_options(&stream, 0); // check CRC
    mad_header_init(&header);
    mad_frame_init(&frame);
    xing_init(&info->xing);

    info->bitrate = 0;
    info->duration = mad_timer_zero; // should be cleared before loop, if we use it as break condition.

    if(info->fileinfo_request == TRUE) {
        aud_tuple_associate_int(info->tuple, FIELD_LENGTH, NULL, -1);
        info->fileinfo_request = FALSE;
    }

    AUDDBG("f: scan_file\n");
    AUDDBG("scan_file frames = %d\n", info->frames);

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

#ifdef DEBUG_INTENSIVELY
            AUDDBG("header bitrate = %ld\n", header.bitrate);
            AUDDBG("duration = %ul\n",
                      mad_timer_count(header.duration,
                                      MAD_UNITS_MILLISECONDS));
            AUDDBG("size = %d\n", stream.next_frame - stream.this_frame);
#endif
            if(aud_tuple_get_int(info->tuple, FIELD_LENGTH, NULL) == -1)
                mad_timer_add(&info->duration, header.duration);
            else {
                gint length = aud_tuple_get_int(info->tuple, FIELD_LENGTH, NULL);

                info->duration.seconds = length / 1000;
                info->duration.fraction = length % 1000;
            }
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
#ifdef AUD_DEBUG
                            {
                                gint tmp = (gint)(info->xing.bytes * 8 / xing_bitrate);
                                AUDDBG("xing: bitrate = %4.1f kbps\n", xing_bitrate / 1000);
                                AUDDBG("xing: duration = %d:%02d\n", tmp / 60, tmp % 60);
                            }
#endif
                        }
                        continue;
                    }
#ifdef AUD_DEBUG
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
                float frame_size = ((double) data_used) / N_AVERAGE_FRAMES;

                AUDDBG("bitrate = %ld samplerate = %d\n", header.bitrate, header.samplerate);
                AUDDBG("data_used = %d info->frames = %d info->size = %d tagsize = %d frame_size = %lf\n",
                          data_used, info->frames, info->size, tagsize, frame_size);

                if(info->size != 0)
                    info->frames = (info->size - tagsize) / frame_size;

                AUDDBG("info->frames = %d\n", info->frames);

                if(aud_tuple_get_int(info->tuple, FIELD_LENGTH, NULL) == -1) {
                    if(xing_bitrate > 0.0) {
                        /* calc duration with xing info */
                        double tmp = 8 * (double)info->xing.bytes * 1000 / xing_bitrate;
                        info->duration.seconds = (guint)(tmp / 1000);
                        info->duration.fraction = (guint)(tmp - info->duration.seconds * 1000);
                    }
                    else {
                        info->duration.seconds /= N_AVERAGE_FRAMES;
                        info->duration.fraction /= N_AVERAGE_FRAMES;
                        mad_timer_multiply(&info->duration, info->frames);
                    }
                }
                else {
                    gint length = aud_tuple_get_int(info->tuple, FIELD_LENGTH, NULL);

                    info->duration.seconds = length / 1000;
                    info->duration.fraction = length % 1000;
                }
#ifdef AUD_DEBUG
                AUDDBG("using fast playtime calculation\n");
                AUDDBG("data used = %d [tagsize=%d framesize=%f]\n",
                          data_used, tagsize, frame_size);
                AUDDBG("frames = %d, frequency = %d, channels = %d\n",
                          info->frames, info->freq, info->channels);
                long millis = mad_timer_count(info->duration,
                                              MAD_UNITS_MILLISECONDS);
                AUDDBG("duration = %ld:%02ld\n", millis / 1000 / 60, (millis / 1000) % 60);
#endif                          /* DEBUG */
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

    AUDDBG("scan_file: info->frames = %d\n", info->frames);
    AUDDBG("e: scan_file\n");

    return (info->frames != 0 || info->remote == TRUE);
}

static void seek (struct mad_info_t * info)
{
    if (info->length == -1)
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

static void watch_controls (struct mad_info_t * info)
{
    g_mutex_lock (control_mutex);

    if (info->pause)
    {
        info->playback->output->pause (1);

        while (info->pause)
        {
            if (info->seek != -1)
                seek (info);

            g_cond_wait (control_cond, control_mutex);
        }

        info->playback->output->pause (0);
    }

    if (info->seek != -1)
        seek (info);

    g_mutex_unlock (control_mutex);
}

gpointer
decode_loop(gpointer arg)
{
    int skip, current;
    unsigned int iteration = 0;

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

    /* set mainwin title */
    if (info->title)
        g_free(info->title);
    info->title = aud_tuple_formatter_make_title_string(info->tuple, audmad_config->title_override == TRUE ?
                                       audmad_config->id3_format : aud_get_gentitle_format());

    info->length = mad_timer_count (info->duration, MAD_UNITS_MILLISECONDS);

    if (info->length == 0)
        info->length = -1;

    info->playback->set_params (info->playback, info->title, info->length,
     info->bitrate, info->freq, info->channels);

    info->resync = 0;
    info->buffer = malloc (1024);
    info->buffer_size = 1024;
    mad_stream_buffer (& stream, info->buffer, 0);
    info->stream = & stream;

    if (! info->playback->output->open_audio (info->fmt, info->freq,
     info->channels))
    {
        error ("open_audio failed: %s.\n", info->playback->output->description);
        goto CLEAN_UP;
    }

    while (info->playback->playing)
    {
        watch_controls (info);
        input_process_remote_metadata(info);

        if (info->resync)
            stream.sync = 0;

    RETRY:
        if (mad_header_decode (& frame.header, & stream) == -1)
            goto ERROR;

        info->bitrate = frame.header.bitrate;

        if (info->vbr && (iteration % 40 == 0))
        {
            info->playback->set_params (info->playback, info->title,
             info->length, info->bitrate, info->freq, info->channels);
        }

        iteration ++;

        if (mad_frame_decode (& frame, & stream) == -1)
            goto ERROR;

        if (info->freq != frame.header.samplerate || info->channels !=
         MAD_NCHANNELS (& frame.header))
        {
            current = info->playback->output->output_time ();
            info->playback->output->close_audio ();

            info->freq = frame.header.samplerate;
            info->channels = MAD_NCHANNELS (& frame.header);

            if (! info->playback->output->open_audio (info->fmt, info->freq,
             info->channels))
            {
                error ("open_audio failed: %s.\n",
                 info->playback->output->description);
                goto CLEAN_UP;
            }

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
                mad_stream_skip (& stream, skip);
                goto RETRY;
            }
        }

        error ("%s.\n", mad_stream_errorstr (& stream));

        if (MAD_RECOVERABLE (stream.error))
            goto RETRY;

        break;
    }

    if (info->playback->playing) {
        GTimeVal sleeptime;

        info->playback->output->buffer_free();
        info->playback->output->buffer_free();
        while (info->playback->output->buffer_playing()) {

            AUDDBG("f: buffer_playing=%d\n", info->playback->output->buffer_playing());

            g_get_current_time(&sleeptime);
            g_time_val_add(&sleeptime, 500000);

            g_mutex_lock(mad_mutex);
            g_cond_timed_wait(mad_cond, mad_mutex, &sleeptime);
            g_mutex_unlock(mad_mutex);
            if (!info->playback->playing) {
                break;
            }

        }
    }

CLEAN_UP:
    free (info->buffer);
    mad_frame_finish (& frame);
    mad_stream_finish (& stream);
    mad_synth_finish (& synth);

    aud_tuple_free(info->tuple);
    info->tuple = NULL;

    info->playback->output->close_audio();
    g_mutex_lock(mad_mutex);
    info->playback->playing = 0;
    g_mutex_unlock(mad_mutex);
    return NULL; /* dummy */
}
