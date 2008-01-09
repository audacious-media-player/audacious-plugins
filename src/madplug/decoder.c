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

extern int triangular_dither_noise(int nbits);

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
    if (!info->playback->playing) {
        g_free(output);
        return;
    }
    info->playback->pass_audio(info->playback,
                  FMT_S16_LE, MAD_NCHANNELS(header), olen, output, &(info->playback->playing));
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
    guint bitrate_frames = 0;
    double xing_bitrate = 0.0;
    double accum_bitrate = 0.0;

    mad_stream_init(&stream);
    mad_stream_options(&stream, 0); // check CRC
    mad_header_init(&header);
    mad_frame_init(&frame);
    xing_init(&info->xing);

    info->bitrate = 0;
    info->pos = mad_timer_zero;
    info->duration = mad_timer_zero; // should be cleared before loop, if we use it as break condition.

    if(info->fileinfo_request == TRUE) {
        aud_tuple_associate_int(info->tuple, FIELD_LENGTH, NULL, -1);
        info->fileinfo_request = FALSE;
    }

    AUDDBG("f: scan_file\n");
    AUDDBG("scan_file frames = %d\n", info->frames);

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
            AUDDBG("scan_file: len <= 0 len = %d\n", len);
            break;
        }

        mad_stream_buffer(&stream, buffer, len + remainder);

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

                if (audmad_config.use_xing) {
                    frame.header = header;
                    if (mad_frame_decode(&frame, &stream) == -1) {
                        AUDDBG("xing frame decode failed\n");
                        goto no_xing;
                    }
                    if (xing_parse(&info->xing, stream.anc_ptr, stream.anc_bitlen) == 0) {
                        AUDDBG("xing header found\n");
                        has_xing = TRUE;
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

    mad_frame_finish(&frame);
    mad_header_finish(&header);
    mad_stream_finish(&stream);
    xing_finish(&info->xing);

    AUDDBG("scan_file: info->frames = %d\n", info->frames);
    AUDDBG("e: scan_file\n");

    return (info->frames != 0 || info->remote == TRUE);
}

/* sanity check for audio open parameters */
static gboolean check_audio_param(struct mad_info_t *info)
{
    if(info->fmt < FMT_U8 || info->fmt > FMT_S16_NE)
        return FALSE;
    if(info->freq < 0) // not sure about maximum frequency. --yaz
        return FALSE;
    if(info->channels < 1 || info->channels > 2)
        return FALSE;

    return TRUE;
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

    AUDDBG("f: decode\n");

    /* init mad stuff */
    mad_frame_init(&frame);
    mad_stream_init(&stream);
    mad_stream_options(&stream, MAD_OPTION_IGNORECRC);
    mad_synth_init(&synth);

    if(!info->playback){
        AUDDBG("decode: playback == NULL\n");
        return NULL;
    }

    AUDDBG("decode: fmt = %d freq = %d channels = %d\n", info->fmt, info->freq, info->channels);

    if(check_audio_param(info) == FALSE)
        return NULL;

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
    info->title = aud_tuple_formatter_make_title_string(info->tuple, audmad_config.title_override == TRUE ?
                                       audmad_config.id3_format : aud_get_gentitle_format());

    tlen = (gint) mad_timer_count(info->duration, MAD_UNITS_MILLISECONDS),
        info->playback->set_params(info->playback, info->title,
                             (tlen == 0 || info->size <= 0) ? -1 : tlen,
                             info->bitrate, info->freq, info->channels);

    AUDDBG("decode: tlen = %d\n", tlen);

    /* main loop */
    do {
        if (!info->playback->playing) {
            AUDDBG("decode: stop signaled\n");
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
            AUDDBG("finished decoding\n");
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

            AUDDBG("skipping: %d\n", seek_skip);

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

                AUDDBG("seeking: %ld\n", info->seek);

                int new_position;
                gulong milliseconds =
                    mad_timer_count(info->duration, MAD_UNITS_MILLISECONDS);
                if (info->seek >= milliseconds)
                    info->seek = milliseconds;

                mad_timer_set(&info->pos, 0, info->seek, 1000); // in millisecond
                new_position =
                    ((double) info->seek / (double) milliseconds) * info->size;

                if(new_position < 0)
                    new_position = 0;

                AUDDBG("seeking to: %d bytes\n", new_position);

                if (aud_vfs_fseek(info->infile, new_position, SEEK_SET) == -1)
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

                AUDDBG("(recovered) error decoding header %d: %s\n",
                          info->current_frame,
                          mad_stream_errorstr(&stream));

                continue;
            }

            info->bitrate = frame.header.bitrate;

            if (!audmad_config.show_avg_vbr_bitrate && info->vbr && (iteration % 40 == 0)) {

#ifdef DEBUG_INTENSIVELY
                AUDDBG("decode vbr tlen = %d\n", tlen);
#endif
                info->playback->set_params(info->playback, info->title,
                                     (tlen == 0 || info->size <= 0) ? -1 : tlen,
                                     info->bitrate, info->freq, info->channels);
            }
            iteration++;

            if (mad_frame_decode(&frame, &stream) == -1) {
                if (!MAD_RECOVERABLE(stream.error))
                    break;

                AUDDBG("(recovered) error decoding frame %d: %s\n",
                          info->current_frame,
                          mad_stream_errorstr(&stream));

            }

            info->current_frame++;

            if (info->freq != frame.header.samplerate
                || info->channels !=
                (guint) MAD_NCHANNELS(&frame.header)) {

                AUDDBG("change in audio type detected\n");
                AUDDBG("old: frequency = %d, channels = %d\n", info->freq,
                          info->channels);
                AUDDBG("new: frequency = %d, channels = %d\n",
                          frame.header.samplerate,
                          (guint) MAD_NCHANNELS(&frame.header));

                info->freq = frame.header.samplerate;
                info->channels = MAD_NCHANNELS(&frame.header);

                if(audmad_config.force_reopen_audio && check_audio_param(info)) {
                    gint current_time = info->playback->output->output_time();

                    AUDDBG("re-opening audio due to change in audio type\n");

                    info->playback->output->close_audio();
                    if (!info->playback->output->open_audio(info->fmt, info->freq,
                                                            info->channels)) {
                        g_mutex_lock(pb_mutex);
                        info->playback->error = TRUE;
                        info->playback->eof = 1;
                        g_mutex_unlock(pb_mutex);
                        g_message("failed to re-open audio output: %s",
                                  info->playback->output->description);
                        return NULL;
                    }
                    // restore time and advance 0.5sec
                    info->seek = current_time + 500;
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

    AUDDBG("e: decode\n");

    aud_tuple_free(info->tuple);
    info->tuple = NULL;

    info->playback->output->close_audio();
    g_mutex_lock(mad_mutex);
    info->playback->playing = 0;
    g_mutex_unlock(mad_mutex);
    return NULL; /* dummy */
}
