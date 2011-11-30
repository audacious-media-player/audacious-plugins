/*
 *  A FLAC decoder plugin for the Audacious Media Player
 *  Copyright (C) 2005 Ralf Ertzinger
 *  Copyright (C) 2010-2011 Michał Lipski <tallica@o2.pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <string.h>

#include <audacious/debug.h>
#include <audacious/i18n.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "flacng.h"

FLAC__StreamDecoder *main_decoder;
callback_info *main_info;
static GMutex *seek_mutex;
static GCond *seek_cond;
static gint seek_value;
static gboolean stop_flag = FALSE;

static gboolean flac_init (void)
{
    FLAC__StreamDecoderInitStatus ret;

    /* Callback structure and decoder for main decoding loop */

    if ((main_info = init_callback_info()) == NULL)
    {
        FLACNG_ERROR("Could not initialize the main callback structure!\n");
        return FALSE;
    }

    if ((main_decoder = FLAC__stream_decoder_new()) == NULL)
    {
        FLACNG_ERROR("Could not create the main FLAC decoder instance!\n");
        return FALSE;
    }

    if (FLAC__STREAM_DECODER_INIT_STATUS_OK != (ret = FLAC__stream_decoder_init_stream(
        main_decoder,
        read_callback,
        seek_callback,
        tell_callback,
        length_callback,
        eof_callback,
        write_callback,
        metadata_callback,
        error_callback,
        main_info)))
    {
        FLACNG_ERROR("Could not initialize the main FLAC decoder: %s(%d)\n",
            FLAC__StreamDecoderInitStatusString[ret], ret);
        return FALSE;
    }

    seek_mutex = g_mutex_new();
    seek_cond = g_cond_new();

    AUDDBG("Plugin initialized.\n");
    return TRUE;
}

static void flac_cleanup(void)
{
    g_mutex_free(seek_mutex);
    g_cond_free(seek_cond);

    FLAC__stream_decoder_delete(main_decoder);
    clean_callback_info(main_info);
}

gboolean flac_is_our_fd(const gchar *filename, VFSFile *fd)
{
    AUDDBG("Probe for FLAC.\n");

    gchar buf[4];
    if (vfs_fread (buf, 1, sizeof buf, fd) != sizeof buf)
        return FALSE;

    return ! strncmp (buf, "fLaC", sizeof buf);
}

static void squeeze_audio(gint32* src, void* dst, guint count, guint res)
{
    gint i;
    gint32* rp = src;
    gint8*  wp = dst;
    gint16* wp2 = dst;
    gint32* wp4 = dst;

    switch (res)
    {
        case 8:
            for (i = 0; i < count; i++, wp++, rp++)
                *wp = *rp & 0xff;
            break;

        case 16:
            for (i = 0; i < count; i++, wp2++, rp++)
                *wp2 = *rp & 0xffff;
            break;

        case 24:
        case 32:
            for (i = 0; i < count; i++, wp4++, rp++)
                *wp4 = *rp;
            break;

        default:
            FLACNG_ERROR("Can not convert to %d bps\n", res);
    }
}

static gboolean flac_play (InputPlayback * playback, const gchar * filename,
 VFSFile * file, gint start_time, gint stop_time, gboolean pause)
{
    if (file == NULL)
        return FALSE;

    gint32 *read_pointer;
    gint elements_left;
    struct stream_info stream_info;
    guint sample_count;
    gpointer play_buffer = NULL;
    gboolean error = FALSE;

    main_info->fd = file;

    if (read_metadata(main_decoder, main_info) == FALSE)
    {
        FLACNG_ERROR("Could not prepare file for playing!\n");
        error = TRUE;
        goto ERR_NO_CLOSE;
    }

    if (main_info->stream.channels > MAX_SUPPORTED_CHANNELS)
    {
        FLACNG_ERROR("This number of channels (%d) is currently not supported, stream not handled by this plugin.\n",
            main_info->stream.channels);
        error = TRUE;
        goto ERR_NO_CLOSE;
    }

    if (main_info->stream.bits_per_sample != 8  &&
        main_info->stream.bits_per_sample != 16 &&
        main_info->stream.bits_per_sample != 24 &&
        main_info->stream.bits_per_sample != 32)
    {
        FLACNG_ERROR("This number of bits (%d) is currently not supported, stream not handled by this plugin.\n",
            main_info->stream.bits_per_sample);
        error = TRUE;
        goto ERR_NO_CLOSE;
    }

    if ((play_buffer = g_malloc0(BUFFER_SIZE_BYTE)) == NULL)
    {
        FLACNG_ERROR("Could not allocate conversion buffer\n");
        error = TRUE;
        goto ERR_NO_CLOSE;
    }

    if (! playback->output->open_audio (SAMPLE_FMT
     (main_info->stream.bits_per_sample), main_info->stream.samplerate,
     main_info->stream.channels))
    {
        error = TRUE;
        goto ERR_NO_CLOSE;
    }

    if (pause)
        playback->output->pause (TRUE);

    seek_value = (start_time > 0) ? start_time : -1;
    stop_flag = FALSE;

    playback->set_params(playback, main_info->bitrate,
        main_info->stream.samplerate, main_info->stream.channels);
    playback->set_pb_ready(playback);

    stream_info.samplerate = main_info->stream.samplerate;
    stream_info.channels = main_info->stream.channels;
    main_info->metadata_changed = FALSE;

    playback->set_gain_from_playlist(playback);

    while (1)
    {
        g_mutex_lock(seek_mutex);

        if (stop_flag)
        {
            g_mutex_unlock(seek_mutex);
            break;
        }

        if (seek_value >= 0)
        {
            playback->output->flush (seek_value);
            FLAC__stream_decoder_seek_absolute (main_decoder, (gint64)
             seek_value * main_info->stream.samplerate / 1000);
            seek_value = -1;
            g_cond_signal(seek_cond);
        }

        g_mutex_unlock(seek_mutex);

        /* Try to decode a single frame of audio */
        if (FLAC__stream_decoder_process_single(main_decoder) == FALSE)
        {
            FLACNG_ERROR("Error while decoding!\n");
            error = TRUE;
            goto CLEANUP;
        }

        /* Maybe the metadata changed midstream. Check. */
        if (main_info->metadata_changed)
        {
            /* Samplerate and channels are important */
            if (stream_info.samplerate != main_info->stream.samplerate)
            {
                FLACNG_ERROR("Samplerate changed midstream (now: %d, was: %d). This is not supported yet.\n",
                    main_info->stream.samplerate, stream_info.samplerate);
                error = TRUE;
                goto CLEANUP;
            }

            if (stream_info.channels != main_info->stream.channels)
            {
                FLACNG_ERROR("Number of channels changed midstream (now: %d, was: %d). This is not supported yet.\n",
                    main_info->stream.channels, stream_info.channels);
                error = TRUE;
                goto CLEANUP;
            }

            main_info->metadata_changed = FALSE;
        }

        /* Compare the frame metadata to the current stream metadata */
        if (main_info->stream.samplerate != main_info->frame.samplerate)
        {
            FLACNG_ERROR("Frame samplerate mismatch (stream: %d, frame: %d)!\n",
                main_info->stream.samplerate, main_info->frame.samplerate);
            error = TRUE;
            goto CLEANUP;
        }

        if (main_info->stream.channels != main_info->frame.channels)
        {
            FLACNG_ERROR("Frame channel mismatch (stream: %d, frame: %d)!\n",
                main_info->stream.channels, main_info->frame.channels);
            error = TRUE;
            goto CLEANUP;
        }

        /*
         * If the frame decoded was an audio frame we now
         * have data in main_info->output_buffer
         *
         * The data in this buffer is in 32 bit wide samples, even if the
         * real sample width is smaller. It has to be converted before
         * feeding it to he output plugin.
         *
         * Feed the data in chunks of OUTPUT_BLOCK_SIZE elements to the output
         * plugin
         */
        read_pointer = main_info->output_buffer;
        elements_left = main_info->buffer_used;

        while (!stop_flag && elements_left != 0)
        {
            if (stop_time >= 0 && playback->output->written_time () >= stop_time)
                goto CLEANUP;

            sample_count = MIN(OUTPUT_BLOCK_SIZE, elements_left);

            squeeze_audio(read_pointer, play_buffer, sample_count, main_info->stream.bits_per_sample);

            playback->output->write_audio(play_buffer, sample_count * SAMPLE_SIZE(main_info->stream.bits_per_sample));

            read_pointer += sample_count;
            elements_left -= sample_count;
        }

        /* Clear the buffer. */
        main_info->write_pointer = main_info->output_buffer;
        main_info->buffer_free = BUFFER_SIZE_SAMP;
        main_info->buffer_used = 0;

        /* Have we reached the end of the stream? */
        if (elements_left == 0 &&
            FLAC__stream_decoder_get_state(main_decoder) == FLAC__STREAM_DECODER_END_OF_STREAM)
        {
            /* Yes. Drain the output buffer and stop playing. */
            AUDDBG("End of stream reached, draining output buffer\n");
            goto CLEANUP;
        }
    }

CLEANUP:
    while (playback->output->buffer_playing())
        g_usleep(20000);

    g_mutex_lock(seek_mutex);
    g_cond_signal(seek_cond); /* wake up any waiting request */
    g_mutex_unlock(seek_mutex);

    AUDDBG("Closing audio device.\n");
    playback->output->close_audio();
    AUDDBG("Audio device closed.\n");

ERR_NO_CLOSE:
    g_free(play_buffer);
    reset_info(main_info);

    if (FLAC__stream_decoder_flush(main_decoder) == FALSE)
        FLACNG_ERROR("Could not flush decoder state!\n");

    return ! error;
}

static void flac_stop(InputPlayback *playback)
{
    g_mutex_lock(seek_mutex);

    if (!stop_flag)
    {
        stop_flag = TRUE;
        playback->output->abort_write();
        g_cond_signal(seek_cond);
    }

    g_mutex_unlock (seek_mutex);
}

static void flac_pause(InputPlayback *playback, gboolean pause)
{
    g_mutex_lock(seek_mutex);

    if (!stop_flag)
        playback->output->pause(pause);

    g_mutex_unlock(seek_mutex);
}

static void flac_seek (InputPlayback * playback, gint time)
{
    g_mutex_lock(seek_mutex);

    if (!stop_flag)
    {
        seek_value = time;
        playback->output->abort_write();
        g_cond_signal(seek_cond);
        g_cond_wait(seek_cond, seek_mutex);
    }

    g_mutex_unlock(seek_mutex);
}

static void flac_aboutbox(void)
{
    static GtkWidget * window = NULL;

    audgui_simple_message(& window, GTK_MESSAGE_INFO, _("About FLAC Audio Plugin"),
        _("\n\nOriginal code by\n"
          "Ralf Ertzinger <ralf@skytale.net>\n\n"
          "http://www.skytale.net/projects/bmp-flac2/")
);
}

static const gchar *flac_fmts[] = { "flac", "fla", NULL };

AUD_INPUT_PLUGIN
(
    .name = "FLAC",
    .init = flac_init,
    .cleanup = flac_cleanup,
    .about = flac_aboutbox,
    .play = flac_play,
    .stop = flac_stop,
    .pause = flac_pause,
    .mseek = flac_seek,
    .probe_for_tuple = flac_probe_for_tuple,
    .is_our_file_from_vfs = flac_is_our_fd,
    .extensions = flac_fmts,
    .update_song_tuple = flac_update_song_tuple,
    .get_song_image = flac_get_image,
    .priority = 1
)
