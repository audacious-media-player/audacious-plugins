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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <math.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>

#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/util.h>
#include <sys/time.h>
#include "plugin.h"
#include "input.h"

#define BUFFER_SIZE 16*1024
#define N_AVERAGE_FRAMES 10

extern long triangular_dither_noise(int nbits);

/**
 * Scale PCM data
 */
static inline signed int
scale(mad_fixed_t sample, struct mad_info_t *file_info)
{
    /* replayGain by SamKR */
    gdouble scale = -1;
    if (audmad_config.replaygain.enable) {
        if (file_info->has_replaygain) {
            scale = file_info->replaygain_track_scale;
            if (file_info->replaygain_album_scale != -1
                && (scale == -1 || !audmad_config.replaygain.track_mode))
            {
                scale = file_info->replaygain_album_scale;
            }
        }
        if (scale == -1)
            scale = audmad_config.replaygain.default_scale;
    }
    if (scale == -1)
        scale = 1.0;
    if (audmad_config.pregain_scale != 1)
        scale = scale * audmad_config.pregain_scale;

    /* hard-limit (clipping-prevention) */
    if (audmad_config.hard_limit) {
        /* convert to double before computation, to avoid mad_fixed_t wrapping */
        double x = mad_f_todouble(sample) * scale;
        static const double k = 0.5;    // -6dBFS
        if (x > k) {
            x = tanh((x - k) / (1 - k)) * (1 - k) + k;
        }
        else if (x < -k) {
            x = tanh((x + k) / (1 - k)) * (1 - k) - k;
        }
        sample = x * (MAD_F_ONE);
    }
    else
        sample *= scale;

    int n_bits_to_loose = MAD_F_FRACBITS + 1 - 16;

    /* round */
    /* add half of the bits_to_loose range to round */
    sample += (1L << (n_bits_to_loose - 1));

#ifdef DEBUG_DITHER
    mad_fixed_t no_dither = sample;
#endif

    /* dither one bit of actual output */
    if (audmad_config.dither) {
        int dither = triangular_dither_noise(n_bits_to_loose + 1);
        sample += dither;
    }

    /* clip */
    /* make sure we are between -1 and 1 */
    if (sample >= MAD_F_ONE) {
        sample = MAD_F_ONE - 1;
    }
    else if (sample < -MAD_F_ONE) {
        sample = -MAD_F_ONE;
    }

    /* quantize */
    /*
     * Turn our mad_fixed_t into an integer.
     * Shift all but 16-bits of the fractional part
     * off the right hand side and shift an extra place
     * to get the sign bit.
     */
    sample >>= n_bits_to_loose;
#ifdef DEBUG_DITHER
    static int n_zeros = 0;
    no_dither >>= n_bits_to_loose;
    if (no_dither - sample == 0)
        n_zeros++;
    else {
        g_message("dither: %d %d", n_zeros, no_dither - sample);
        n_zeros = 0;
    }
#endif
    return sample;
}

void
write_output(struct mad_info_t *info, struct mad_pcm *pcm,
             struct mad_header *header)
{
    unsigned int nsamples;
    mad_fixed_t const *left_ch, *right_ch;
    char *output;
    int olen = 0;
    int pos = 0;

    nsamples = pcm->length;
    left_ch = pcm->samples[0];
    right_ch = pcm->samples[1];
    olen = nsamples * MAD_NCHANNELS(header) * 2;
    output = (char *) g_malloc(olen * sizeof(char));

    while (nsamples--) {
        signed int sample;
        /* output sample(s) in 16-bit signed little-endian PCM */
        sample = scale(*left_ch++, info);
        output[pos++] = (sample >> 0) & 0xff;
        output[pos++] = (sample >> 8) & 0xff;

        if (MAD_NCHANNELS(header) == 2) {
            sample = scale(*right_ch++, info);
            output[pos++] = (sample >> 0) & 0xff;
            output[pos++] = (sample >> 8) & 0xff;
        }
    }
    assert(pos == olen);
    if (!info->playback->playing)
        return;
    produce_audio(info->playback->output->written_time(),
                  FMT_S16_LE, MAD_NCHANNELS(header), olen, output, &(info->playback->playing));
    if (!info->playback->playing)
        return;
    g_free(output);
}

/**
 * Decode all headers in the file and fill in stats
 * @return FALSE if scan failed.
 */
gboolean scan_file(struct mad_info_t * info, gboolean fast)
{
    struct mad_stream stream;
    struct mad_header header;
    int remainder = 0;
    int data_used = 0;
    int len = 0;
    int tagsize = 0;
    unsigned char buffer[BUFFER_SIZE];
    struct mad_frame frame;     /* to read xing data */
    gboolean has_xing = FALSE;
    int bitrate_frames = 0;

    mad_stream_init(&stream);
    mad_header_init(&header);
    mad_frame_init(&frame);
    xing_init(&info->xing);

    info->bitrate = 0;
    info->pos = mad_timer_zero;
    info->duration = mad_timer_zero; // should be cleared before loop, if we use it as break condition.

#ifdef DEBUG
    g_message("f: scan_file");
    g_message("scan_file frames = %d", info->frames);
#endif                          /* DEBUG */

    while (1) {
        remainder = stream.bufend - stream.next_frame;

        /*
           if (remainder >= BUFFER_SIZE)
           {
           printf("oh dear.. remainder = %d\n", remainder);
           }
         */

        memcpy(buffer, stream.this_frame, remainder);
        len = input_get_data(info, buffer + remainder,
                             BUFFER_SIZE - remainder);

        if (len <= 0) {
#ifdef DEBUG
            g_message("scan_file: len <= 0 abort.");
#endif
            break;
        }

        mad_stream_buffer(&stream, buffer, len + remainder);

        while (!fast || (fast && info->frames < N_AVERAGE_FRAMES)) {
            if (mad_header_decode(&header, &stream) == -1) {
                if (stream.error == MAD_ERROR_BUFLEN) {
                    break;
                }
                if (!MAD_RECOVERABLE(stream.error)) {
#ifdef DEBUG
                    g_message("(fatal) error decoding header %d: %s",
                              info->frames, mad_stream_errorstr(&stream));
                    g_message("remainder = %d", remainder);
                    g_message("len = %d", len);
#endif                          /* DEBUG */
                    break;
                }
                if (stream.error == MAD_ERROR_LOSTSYNC) {
                    /* ignore LOSTSYNC due to ID3 tags */
                    tagsize = id3_tag_query(stream.this_frame,
                                            stream.bufend -
                                            stream.this_frame);
                    if (tagsize > 0) {
#ifdef DEBUG
                        g_message("skipping id3_tag: %d", tagsize);
#endif                          /* DEBUG */
                        mad_stream_skip(&stream, tagsize);
                        continue;
                    }
                }
#ifdef DEBUG
                g_message("(recovered) error decoding header %d: %s",
                          info->frames, mad_stream_errorstr(&stream));
                g_message("remainder = %d", remainder);
                g_message("len = %d", len);
#endif                          /* DEBUG */
                continue;
            }
            info->frames++;

#ifdef DEBUG
#ifdef DEBUG_INTENSIVELY
            g_message("duration = %lu",
                      mad_timer_count(header.duration,
                                      MAD_UNITS_MILLISECONDS));
            g_message("size = %d", stream.next_frame - stream.this_frame);
#endif
#endif
            if(info->tuple->length == -1)
                mad_timer_add(&info->duration, header.duration);
            else {
                info->duration.seconds = info->tuple->length / 1000;
                info->duration.fraction = info->tuple->length % 1000;
            }
            data_used += stream.next_frame - stream.this_frame;
            if (info->frames == 1) {
                /* most of these *should* remain constant */
                info->bitrate = header.bitrate;
                info->freq = header.samplerate;
                info->channels = MAD_NCHANNELS(&header);
                info->mpeg_layer = header.layer;
                info->mode = header.mode;

                if (audmad_config.use_xing) {
                    frame.header = header;
                    if (mad_frame_decode(&frame, &stream) == -1) {
#ifdef DEBUG
                        g_message("xing frame decode failed");
#endif
                        goto no_xing;
                    }
                    if (xing_parse
                        (&info->xing, stream.anc_ptr,
                         stream.anc_bitlen) == 0) {
#ifdef DEBUG
                        g_message("found xing header");
#endif                          /* DEBUG */
                        has_xing = TRUE;
                        info->vbr = TRUE;   /* otherwise xing header would have been 'Info' */
                        info->frames = info->xing.frames;
                        if(info->tuple->length == -1)
                            mad_timer_multiply(&info->duration, info->frames);
                        else {
                            info->duration.seconds = info->tuple->length / 1000;
                            info->duration.fraction = info->tuple->length % 1000;
                        }
                        info->bitrate =
                            8.0 * info->xing.bytes /
                            mad_timer_count(info->duration,
                                            MAD_UNITS_SECONDS);
                        break;
                    }
                }

            }
            else {
                /* perhaps we have a VRB file */
                if (info->bitrate != header.bitrate)
                    info->vbr = TRUE;
                if (info->vbr) {
                    info->bitrate += header.bitrate;
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
#ifdef DEBUG
                g_message("info->frames = %d info->size = %d tagsize = %d frame_size = %lf",
                          info->frames, info->size, tagsize, frame_size);
#endif
                if(info->size != 0)
                    info->frames = (info->size - tagsize) / frame_size;
#ifdef DEBUG
                g_message("info->frames = %d", info->frames);
#endif
                if(info->tuple->length == -1) {
                    info->duration.seconds /= N_AVERAGE_FRAMES;
                    info->duration.fraction /= N_AVERAGE_FRAMES;
                    mad_timer_multiply(&info->duration, info->frames);
                }
                else {
                    info->duration.seconds = info->tuple->length / 1000;
                    info->duration.fraction = info->tuple->length % 1000;
                }
#ifdef DEBUG
                g_message("using fast playtime calculation");
                g_message("data used = %d [tagsize=%d framesize=%f]",
                          data_used, tagsize, frame_size);
                g_message("frames = %d, frequency = %d, channels = %d",
                          info->frames, info->freq, info->channels);
                long millis = mad_timer_count(info->duration,
                                              MAD_UNITS_MILLISECONDS);
                g_message("duration = %lu:%lu", millis / 1000 / 60,
                          (millis / 1000) % 60);
#endif                          /* DEBUG */
                break;
            }
        }
        if (stream.error != MAD_ERROR_BUFLEN)
            break;
    }

    if (info->vbr && !has_xing)
        info->bitrate = info->bitrate / bitrate_frames;

    mad_frame_finish(&frame);
    mad_header_finish(&header);
    mad_stream_finish(&stream);
    xing_finish(&info->xing);

#ifdef DEBUG
    g_message("e: scan_file");
#endif                          /* DEBUG */
    return (info->frames != 0 || info->remote == TRUE);
}

gpointer decode_loop(gpointer arg)
{
    unsigned char buffer[BUFFER_SIZE];
    int len;
    gboolean seek_skip = FALSE;
    int remainder = 0;
    gint tlen;
    unsigned int iteration = 0;

    /* mad structs */
    struct mad_stream stream;
    struct mad_frame frame;
    struct mad_synth synth;

    /* track info is passed in as thread argument */
    struct mad_info_t *info = (struct mad_info_t *) arg;

#ifdef DEBUG
    g_message("f: decode");
#endif                          /* DEBUG */

    /* init mad stuff */
    mad_frame_init(&frame);
    mad_stream_init(&stream);
    mad_synth_init(&synth);

    if(!info->playback){
#ifdef DEBUG
        g_message("decode: playback == NULL");
#endif
        return NULL;
    }

#ifdef DEBUG
    g_message("decode: fmt = %d freq = %d channels = %d", info->fmt, info->freq, info->channels);
#endif
    if (!info->playback->output->open_audio(info->fmt, info->freq, info->channels)) {
        g_mutex_lock(pb_mutex);
        info->playback->error = TRUE;
        info->playback->eof = 1;
        g_mutex_unlock(pb_mutex);        
        g_message("failed to open audio output: %s",
                  info->playback->output->description);
        return NULL;
    }

    /* set mainwin title */
    if (info->title)
        g_free(info->title);
    info->title = xmms_get_titlestring(audmad_config.title_override == TRUE ?
                                       audmad_config.id3_format : xmms_get_gentitle_format(), info->tuple);
    
    tlen = (gint) mad_timer_count(info->duration, MAD_UNITS_MILLISECONDS),
        mad_plugin->set_info(info->title,
                             (tlen == 0 || info->size <= 0) ? -1 : tlen,
                             info->bitrate, info->freq, info->channels);
#ifdef DEBUG
    g_message("decode: tlen = %d\n", tlen);
#endif

    /* main loop */
    do {
        if (!info->playback->playing) {
#ifdef DEBUG
            g_message("decode: stop signaled");
#endif                          /* DEBUG */
            break;
        }
        if (seek_skip)
            remainder = 0;
        else {
            remainder = stream.bufend - stream.next_frame;
            memcpy(buffer, stream.this_frame, remainder);
        }
        len = input_get_data(info, buffer + remainder,
                             BUFFER_SIZE - remainder);

        input_process_remote_metadata(info);

        if (len <= 0) {
#ifdef DEBUG
            g_message("finished decoding");
#endif                          /* DEBUG */
            break;
        }
        len += remainder;
        if (len < MAD_BUFFER_GUARD) {
            int i;
            for (i = len; i < MAD_BUFFER_GUARD; i++)
                buffer[i] = 0;
            len = MAD_BUFFER_GUARD;
        }

        mad_stream_buffer(&stream, buffer, len);

        if (seek_skip) {
#ifdef DEBUG
            g_message("skipping: %d", seek_skip);
#endif
            int skip = 2;
            do {
                if (mad_frame_decode(&frame, &stream) == 0) {
                    mad_timer_add(&info->pos, frame.header.duration);
                    if (--skip == 0)
                        mad_synth_frame(&synth, &frame);
                }
                else if (!MAD_RECOVERABLE(stream.error)) {
                    g_mutex_lock(pb_mutex);
                    info->playback->error = TRUE;
                    info->playback->eof = 1;
                    g_mutex_unlock(pb_mutex);
                    break;
                }
            }
            while (skip);
            seek_skip = FALSE;
        }

        while (info->playback->playing) {
            if (info->seek != -1 && info->size > 0) {
#ifdef DEBUG
                g_message("seeking: %d", info->seek);
#endif
                int new_position;
                int seconds =
                    mad_timer_count(info->duration, MAD_UNITS_SECONDS);
                if (info->seek >= seconds)
                    info->seek = seconds;

                mad_timer_set(&info->pos, info->seek, 0, 0);
                new_position =
                    ((double) info->seek / (double) seconds) * info->size;

                if(new_position < 0)
                    new_position = 0;
#ifdef DEBUG
                g_message("seeking to: %d bytes", new_position);
#endif
                if (vfs_fseek(info->infile, new_position, SEEK_SET) == -1)
                    audmad_error("failed to seek to: %d", new_position);
                mad_frame_mute(&frame);
                mad_synth_mute(&synth);
                stream.error = MAD_ERROR_BUFLEN;
                info->playback->output->flush(mad_timer_count(info->pos, MAD_UNITS_MILLISECONDS));
                stream.sync = 0;
                info->seek = -1;
                seek_skip = TRUE;
                break;
            }

            if (mad_header_decode(&frame.header, &stream) == -1) {
                if (!MAD_RECOVERABLE(stream.error)) {
                    break;
                }
                if (stream.error == MAD_ERROR_LOSTSYNC) {
                    /* ignore LOSTSYNC due to ID3 tags */
                    int tagsize = id3_tag_query(stream.this_frame,
                                                stream.bufend -
                                                stream.this_frame);
                    if (tagsize > 0) {
                        mad_stream_skip(&stream, tagsize);
                        continue;
                    }
                }
#ifdef DEBUG
                g_message("(recovered) error decoding header %d: %s",
                          info->current_frame,
                          mad_stream_errorstr(&stream));
#endif                          /* DEBUG */
                continue;
            }

            info->bitrate = frame.header.bitrate;

            if (!audmad_config.show_avg_vbr_bitrate && info->vbr && (iteration % 40 == 0)) {
#ifdef DEBUG
                g_message("decode vbr tlen = %d", tlen);
#endif
                mad_plugin->set_info(info->title,
                                     (tlen == 0 || info->size <= 0) ? -1 : tlen,
                                     info->bitrate, info->freq, info->channels);
            }
            iteration++;

            if (mad_frame_decode(&frame, &stream) == -1) {
                if (!MAD_RECOVERABLE(stream.error))
                    break;
#ifdef DEBUG
                g_message("(recovered) error decoding frame %d: %s",
                          info->current_frame,
                          mad_stream_errorstr(&stream));
#endif                          /* DEBUG */
            }

            info->current_frame++;

            if (info->freq != frame.header.samplerate
                || info->channels !=
                (guint) MAD_NCHANNELS(&frame.header)) {
#ifdef DEBUG
                g_message("re-opening audio due to change in audio type");
                g_message("old: frequency = %d, channels = %d", info->freq,
                          info->channels);
                g_message("new: frequency = %d, channels = %d",
                          frame.header.samplerate,
                          (guint) MAD_NCHANNELS(&frame.header));
#endif                          /* DEBUG */
                info->freq = frame.header.samplerate;
                info->channels = MAD_NCHANNELS(&frame.header);
                info->playback->output->close_audio();
                if (!info->playback->output->open_audio(info->fmt, info->freq,
                                                   info->channels)) {
                    g_mutex_lock(pb_mutex);
                    info->playback->error = TRUE;
                    info->playback->eof = 1;
                    g_mutex_unlock(pb_mutex);
                    audmad_error("failed to re-open audio output: %s",
                                  info->playback->output->description);
                }
            }
            if (!info->playback->playing)
                break;
            mad_synth_frame(&synth, &frame);
            mad_stream_sync(&stream);

            write_output(info, &synth.pcm, &frame.header);
            mad_timer_add(&info->pos, frame.header.duration);
        }
    }
    while (stream.error == MAD_ERROR_BUFLEN);

    /* free mad stuff */
    mad_frame_finish(&frame);
    mad_stream_finish(&stream);
    mad_synth_finish(&synth);

    if (info->playback->playing) {
        GTimeVal sleeptime;

        info->playback->output->buffer_free();
        info->playback->output->buffer_free();
        while (info->playback->output->buffer_playing()) {
#ifdef DEBUG
            g_message("f: buffer_playing=%d",
                      info->playback->output->buffer_playing());
#endif
            g_get_current_time(&sleeptime);
            sleeptime.tv_usec += 500000;
            if(sleeptime.tv_usec >= 1000000) {
                sleeptime.tv_sec += 1;
                sleeptime.tv_usec -= 1000000;
            }

            g_mutex_lock(mad_mutex);
            g_cond_timed_wait(mad_cond, mad_mutex, &sleeptime);
            if (!info->playback->playing) {
                g_mutex_unlock(mad_mutex);
                break;
            }
            g_mutex_unlock(mad_mutex);
        }
    }
#ifdef DEBUG
    g_message("e: decode");
#endif                          /* DEBUG */

    bmp_title_input_free(info->tuple);
    info->tuple = NULL;

    info->playback->output->close_audio();
    g_mutex_lock(mad_mutex);
    info->playback->playing = 0;
    g_mutex_unlock(mad_mutex);
    g_thread_exit(0);
    return NULL; /* dummy */
}
