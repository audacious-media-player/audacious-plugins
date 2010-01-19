/*
 *  A FLAC decoder plugin for the Audacious Media Player
 *  Copyright (C) 2005 Ralf Ertzinger
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

#include "flacng.h"
#include <audacious/output.h>
#include <audacious/i18n.h>
#include "tools.h"
#include "plugin.h"
#include "seekable_stream_callbacks.h"
#include "version.h"
#include "debug.h"

static gchar *flac_fmts[] = { "flac", "fla", NULL };

InputPlugin flac_ip = {
    .description = "FLACng Audio Plugin",
    .init = flac_init,
    .cleanup = flac_cleanup,
    .about = flac_aboutbox,
    .play_file = flac_play_file,
    .stop = flac_stop,
    .pause = flac_pause,
    .seek = flac_seek,
    .get_song_tuple = flac_get_song_tuple,	// get a tuple
    .is_our_file_from_vfs = flac_is_our_fd,	// version of is_our_file which is handed an FD
    .vfs_extensions = flac_fmts			// vector of fileextensions allowed by the plugin
};

InputPlugin *flac_iplist[] = { &flac_ip, NULL };

SIMPLE_INPUT_PLUGIN (flacng, flac_iplist)

FLAC__StreamDecoder* test_decoder;
FLAC__StreamDecoder* main_decoder;
callback_info* test_info;
callback_info* main_info;
gboolean plugin_initialized = FALSE;
static GMutex *seek_mutex;
static GCond *seek_cond;
static gboolean pause_flag;
static gint seek_value;

/* === */

static void flac_init(void)
{

    FLAC__StreamDecoderInitStatus ret;

    _ENTER;

    /*
     * Callback structure and decoder for file test
     * purposes
     */
    if (NULL == (test_info = init_callback_info("test"))) {
        _ERROR("Could not initialize the test callback structure!");
        _LEAVE;
    }
    _DEBUG("Test callback structure at %p", test_info);

    if (NULL == (test_decoder = FLAC__stream_decoder_new())) {
        _ERROR("Could not create the test FLAC decoder instance!");
        _LEAVE;
    }


    /*
     * We want the VORBISCOMMENT metadata, for the file tags
     * and SEEKTABLE
     */
    FLAC__stream_decoder_set_metadata_respond(test_decoder, FLAC__METADATA_TYPE_VORBIS_COMMENT);
    FLAC__stream_decoder_set_metadata_respond(test_decoder, FLAC__METADATA_TYPE_SEEKTABLE);

    /*
     * Callback structure and decoder for main decoding
     * loop
     */
    if (NULL == (main_info = init_callback_info("main"))) {
        _ERROR("Could not initialize the main callback structure!");
        _LEAVE;
    }
    _DEBUG("Main callback structure at %p", main_info);

    if (NULL == (main_decoder = FLAC__stream_decoder_new())) {
        _ERROR("Could not create the main FLAC decoder instance!");
        _LEAVE;
    }


    /*
     * We want the VORBISCOMMENT metadata, for the file tags
     * and SEEKTABLE
     */
    FLAC__stream_decoder_set_metadata_respond(main_decoder, FLAC__METADATA_TYPE_VORBIS_COMMENT);
    FLAC__stream_decoder_set_metadata_respond(main_decoder, FLAC__METADATA_TYPE_SEEKTABLE);

    /*
     * Initialize decoders
     */
    if (FLAC__STREAM_DECODER_INIT_STATUS_OK != (ret = FLAC__stream_decoder_init_stream(
            test_decoder,
            read_callback,
            seek_callback,
            tell_callback,
            length_callback,
            eof_callback,
            write_callback,
            metadata_callback,
            error_callback,
            test_info))) {
        _ERROR("Could not initialize test FLAC decoder: %s(%d)",
                FLAC__StreamDecoderInitStatusString[ret], ret);
        _LEAVE;
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
            main_info))) {
        _ERROR("Could not initialize main FLAC decoder: %s(%d)",
                FLAC__StreamDecoderInitStatusString[ret], ret);
        _LEAVE;
     }

    seek_mutex = g_mutex_new();
    seek_cond = g_cond_new();

     _DEBUG("plugin initialized OK!");
     plugin_initialized = TRUE;
    _LEAVE;
}

/* --- */

static void flac_cleanup(void)
{
    _ENTER;

    g_mutex_free(seek_mutex);
    g_cond_free(seek_cond);

    FLAC__stream_decoder_delete(main_decoder);
    clean_callback_info(main_info);

    FLAC__stream_decoder_delete(test_decoder);
    clean_callback_info(test_info);

    plugin_initialized = FALSE;

    _LEAVE;
}

/* --- */

gboolean flac_is_our_fd(const gchar* filename, VFSFile* fd) {

    _ENTER;

    if (!plugin_initialized) {
        _ERROR("Plugin not initialized!");
        _LEAVE FALSE;
    }

    _DEBUG("Testing fd for file: %s", filename);

    INFO_LOCK(test_info);

    if (FALSE == read_metadata(fd, test_decoder, test_info)) {
        _DEBUG("File not handled by this plugin!");
        INFO_UNLOCK(test_info);
        _LEAVE FALSE;
    }

    /*
     * See if the metadata has changed
     */
    if (FALSE == test_info->metadata_changed) {
        _DEBUG("No metadata found in stream");
        INFO_UNLOCK(test_info);
        _LEAVE FALSE;
    }

    /*
     * If we get here, the file is supported by FLAC.
     * The stream characteristics have been filled in by
     * the metadata callback.
     */

    _DEBUG("Stream encoded at %d Hz, %d bps, %d channels",
        test_info->stream.samplerate,
        test_info->stream.bits_per_sample,
        test_info->stream.channels);

    if (MAX_SUPPORTED_CHANNELS < test_info->stream.channels) {
        _ERROR("This number of channels (%d) is currently not supported, stream not handled by this plugin",
            test_info->stream.channels);
        INFO_UNLOCK(test_info);
        _LEAVE FALSE;
    }

    if ((8  != test_info->stream.bits_per_sample) &&
        (16 != test_info->stream.bits_per_sample) &&
        (24 != test_info->stream.bits_per_sample) &&
        (32 != test_info->stream.bits_per_sample)) {
        _ERROR("This number of bits (%d) is currently not supported, stream not handled by this plugin",
            test_info->stream.bits_per_sample);
        INFO_UNLOCK(test_info);
        _LEAVE FALSE;
    }

    /*
     * Looks good.
     */

    _DEBUG("Accepting file %s", filename);

    reset_info(test_info, FALSE);
    INFO_UNLOCK(test_info);

    _LEAVE TRUE;
}

/* --- */

void squeeze_audio(gint32* src, void* dst, guint count, guint res) {

    gint i;
    gint32* rp = src;
    gint8* wp = dst;
    gint16* wp2 = dst;
    gint32* wp4 = dst;

    _ENTER;

    _DEBUG("Converting %d samples to %d bit", count, res);

    if (res % 8 != 0) {
        _ERROR("Can not convert to %d bps: not a multiple of 8", res);
        _LEAVE;
    }

    if (res == 8) {
        for (i=0; i<count; i++, wp++, rp++) {
            *wp = *rp & 0xff;
        }
    } else if (res == 16) {
        for (i=0; i<count; i++, wp2++, rp++) {
            *wp2 = *rp & 0xffff;
        }
    } else if (res == 24 || res == 32) { /* 24bit value stored in lowest 3 bytes */
       for (i=0; i<count; i++, wp4++, rp++) {
           *wp4 = *rp;
       }
    }

    _LEAVE;
}

/* --- */

static gpointer flac_play_loop(gpointer arg)
{
    /*
     * The main play loop.
     * Decode a frame, push the decoded data to the output plugin
     * chunkwise. Repeat until finished.
     */

    gint32* read_pointer;
    gint elements_left;
    FLAC__StreamDecoderState state;
    struct stream_info stream_info;
    guint sample_count;
    void* play_buffer;
    InputPlayback* playback = (InputPlayback *) arg;
    gboolean paused = FALSE;

    _ENTER;

    if (NULL == (play_buffer = g_malloc(BUFFER_SIZE_BYTE))) {
        _ERROR("Could not allocate conversion buffer");
        playback->playing = FALSE;
        reset_info(main_info, TRUE);
        _LEAVE NULL;
    }

    stream_info.samplerate = main_info->stream.samplerate;
    stream_info.channels = main_info->stream.channels;
    main_info->metadata_changed = FALSE;

    ReplayGainInfo rg_info = get_replay_gain(main_info);
    playback->set_replaygain_info(playback, &rg_info);

    if (!playback->output->open_audio(SAMPLE_FMT(main_info->stream.bits_per_sample),
                                      main_info->stream.samplerate,
                                      main_info->stream.channels)) {
        g_free(play_buffer);
        reset_info(main_info, TRUE);
        playback->playing = FALSE;
        _ERROR("Could not open output plugin!");
        _LEAVE NULL;
    }

    while (TRUE == playback->playing) {

        /*
         * Try to decode a single frame of audio
         */
        if (FALSE == FLAC__stream_decoder_process_single(main_decoder)) {
            /*
             * Something went wrong
             */
            _ERROR("Error while decoding!");
            break;
        }

        /*
         * Maybe the metadata changed midstream. Check.
         */

        if (main_info->metadata_changed) {
            /*
             * Samplerate and channels are important
             */
            if (stream_info.samplerate != main_info->stream.samplerate) {
                _ERROR("Samplerate changed midstream (now: %d, was: %d). This is not supported yet.",
                    main_info->stream.samplerate, stream_info.samplerate);
                break;
            }

            if (stream_info.channels != main_info->stream.channels) {
                _ERROR("Number of channels changed midstream (now: %d, was: %d). This is not supported yet.",
                    main_info->stream.channels, stream_info.channels);
                break;
            }

            main_info->metadata_changed = FALSE;
        }

        /*
         * Compare the frame metadata to the current stream metadata
         */
        if (main_info->stream.samplerate != main_info->frame.samplerate) {
            _ERROR("Frame samplerate mismatch (stream: %d, frame: %d)!",
                main_info->stream.samplerate, main_info->frame.samplerate);
            break;
        }

        if (main_info->stream.channels != main_info->frame.channels) {
            _ERROR("Frame channel mismatch (stream: %d, frame: %d)!",
                main_info->stream.channels, main_info->frame.channels);
            break;
        }

        g_mutex_lock(seek_mutex);

        if (seek_value >= 0)
        {
            playback->output->flush(1000 * seek_value);
            FLAC__stream_decoder_seek_absolute(main_decoder, (long long) seek_value * main_info->stream.samplerate);
            seek_value = -1;
            g_cond_signal(seek_cond);
        }

        if (pause_flag != paused)
        {
            playback->output->pause(pause_flag);
            paused = pause_flag;
            g_cond_signal(seek_cond);
        }

        if (paused)
        {
            g_cond_wait(seek_cond, seek_mutex);
            g_mutex_unlock(seek_mutex);
            continue;
        }

        g_mutex_unlock(seek_mutex);

        /*
         * If the frame decoded was an audio frame we now
         * have data in info->output_buffer
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

        while ((TRUE == playback->playing) && (0 != elements_left)) {

            sample_count = MIN(OUTPUT_BLOCK_SIZE, elements_left);

            squeeze_audio(read_pointer, play_buffer, sample_count, main_info->stream.bits_per_sample);

            _DEBUG("Copying %d samples to output plugin", sample_count);

            playback->pass_audio(playback,
                                 SAMPLE_FMT(main_info->stream.bits_per_sample),
                                 main_info->stream.channels,
                                 sample_count * SAMPLE_SIZE(main_info->stream.bits_per_sample),
                                 play_buffer,
                                 NULL);

            read_pointer += sample_count;
            elements_left -= sample_count;

            _DEBUG("%d elements left to be output", elements_left);
        }

        /*
         * Clear the buffer.
         */
        main_info->write_pointer = main_info->output_buffer;
        main_info->buffer_free = BUFFER_SIZE_SAMP;
        main_info->buffer_used = 0;


        /*
         * Have we reached the end of the stream?
         */
        state = FLAC__stream_decoder_get_state(main_decoder);
        if (0 == elements_left && (FLAC__STREAM_DECODER_END_OF_STREAM == state)) {
            /*
             * Yes. Drain the output buffer and stop playing.
             */

            _DEBUG("End of stream reached, draining output buffer");

            while (playback->output->buffer_playing() && playback->playing == TRUE) {
                g_usleep(40000);
            }

            playback->playing = FALSE;
        }
    }

    /*
     * Clean up a bit
     */
    playback->playing = FALSE;
    _DEBUG("Closing audio device");
    playback->output->close_audio();
    _DEBUG("Audio device closed");

    g_free(play_buffer);
    reset_info(main_info, TRUE);

    if (FALSE == FLAC__stream_decoder_flush(main_decoder)) {
        _ERROR("Could not flush decoder state!");
    }

    _LEAVE NULL;
}

/* --- */

void flac_play_file(InputPlayback *playback)
{
    VFSFile *fd;
    Tuple *tuple;

    _ENTER;

    if (!plugin_initialized) {
        _ERROR("plugin not initialized!");
        _LEAVE;
    }

    /*
     * Open the file
     */
    if (NULL == (fd = aud_vfs_fopen(playback->filename, "rb"))) {
        _ERROR("Could not open file for reading! (%s)", playback->filename);
        _LEAVE;
    }

    if (FALSE == read_metadata(fd, main_decoder, main_info)) {
        reset_info(main_info, TRUE);
        _ERROR("Could not prepare file for playing!");
        _LEAVE;
    }
    else
        tuple = get_tuple(fd, main_info);


    seek_value = -1;
    pause_flag = FALSE;
    playback->playing = TRUE;

    playback->set_tuple(playback, tuple);
    playback->set_params(playback, NULL, 0, -1, main_info->stream.samplerate, main_info->stream.channels);
    playback->set_pb_ready(playback);

    flac_play_loop(playback);

    _LEAVE;
}

/* --- */

static void flac_stop(InputPlayback *playback)
{
    g_mutex_lock(seek_mutex);

    if (playback->playing)
    {
        playback->playing = FALSE;
        g_cond_signal(seek_cond);
        g_mutex_unlock(seek_mutex);
        g_thread_join(playback->thread);
        playback->thread = NULL;

        reset_info(main_info, TRUE);
    }
    else
        g_mutex_unlock (seek_mutex);
}

/* --- */

static void flac_pause(InputPlayback *playback, gshort p)
{
    g_mutex_lock(seek_mutex);

    if (playback->playing)
    {
        pause_flag = p;
        g_cond_signal(seek_cond);
        g_cond_wait(seek_cond, seek_mutex);
    }

    g_mutex_unlock(seek_mutex);
}

/* --- */

static void flac_seek(InputPlayback *playback, gint time)
{
    g_mutex_lock(seek_mutex);

    if (playback->playing)
    {
        seek_value = time;
        g_cond_signal(seek_cond);
        g_cond_wait(seek_cond, seek_mutex);
    }

    g_mutex_unlock(seek_mutex);
}

/* --- */

static Tuple *flac_get_song_tuple(const gchar* filename)
{
    VFSFile *fd;
    Tuple * tuple = NULL;

    _ENTER;

    _DEBUG("Testing file: %s", filename);
    /*
     * Open the file
     */

    if (NULL == (fd = aud_vfs_fopen(filename, "rb"))) {
        _ERROR("Could not open file for reading! (%s)", filename);
        _LEAVE NULL;
    }

    INFO_LOCK(test_info);

    if (read_metadata(fd, test_decoder, test_info))
        tuple = get_tuple(fd, test_info);
    else
        _ERROR ("Could not read metadata tuple for file <%s>", filename);

    reset_info(test_info, TRUE);
    INFO_UNLOCK(test_info);

    _LEAVE tuple;
}

/* --- */

static void flac_aboutbox(void)
{
    static GtkWidget* about_window;
    gchar *about_text;

    if (about_window) {
        gtk_window_present(GTK_WINDOW(about_window));
        return;
    }

    about_text = g_strjoin("", _("FLAC Audio Plugin "), _VERSION,
			       _("\n\nOriginal code by\n"
                               "Ralf Ertzinger <ralf@skytale.net>\n"
                               "\n"
                               "http://www.skytale.net/projects/bmp-flac2/"), NULL);

    about_window = audacious_info_dialog(_("About FLAC Audio Plugin"),
                                     about_text,
                                     _("OK"), FALSE, NULL, NULL);

    g_signal_connect(G_OBJECT(about_window), "destroy",
                     G_CALLBACK(gtk_widget_destroyed), &about_window);
    g_free(about_text);
}
