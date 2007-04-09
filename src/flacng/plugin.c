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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <audacious/util.h>
#include <audacious/output.h>
#include <glib/gi18n.h>
#include "flacng.h"
#include "tools.h"
#include "plugin.h"
#include "seekable_stream_callbacks.h"
#include "version.h"
#include "debug.h"

static gchar *flac_fmts[] = { "flac", NULL };

InputPlugin flac_ip = {
    NULL,
    NULL,
    "FLACng Audio Plugin",
    flac_init,
    flac_aboutbox,
    NULL,
    flac_is_our_file,
    NULL,
    flac_play_file,
    flac_stop,
    flac_pause,
    flac_seek,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    flac_get_song_info,
    NULL,
    NULL,
    flac_get_song_tuple,	// get a tuple
    NULL,
    NULL,			// write a tuple back to a file as a tag
/*    flac_is_our_fd */ NULL,	// version of is_our_file which is handed an FD
    flac_fmts			// vector of fileextensions allowed by the plugin
};

FLAC__StreamDecoder* test_decoder;
FLAC__StreamDecoder* main_decoder;
callback_info* test_info;
callback_info* main_info;
gboolean plugin_initialized = FALSE;
gint seek_to = -1;
static GThread* thread;
GMutex* flac_pl_mutex;

/* === */

InputPlugin* get_iplugin_info(void) {

    _ENTER;

    _DEBUG("%s (%s)", flac_ip.description, _VERSION);

    _LEAVE &flac_ip;
}

/* --- */

void flac_init(void) {

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
        _ERROR("Could not initialize test FLAC decoder: %s(%d)", StreamDecoderInitState(ret), ret);
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
        _ERROR("Could not initialize main FLAC decoder: %s(%d)", StreamDecoderInitState(ret), ret);
        _LEAVE;
     }

     /*
      * Initialize the play loop mutex
      */
     flac_pl_mutex = g_mutex_new();

     _DEBUG("plugin initialized OK!");
     plugin_initialized = TRUE;
    _LEAVE;
}

/* --- */

gboolean flac_is_our_file(gchar* filename) {

    _ENTER;

    if (!plugin_initialized) {
        _ERROR("Plugin not initialized!");
        _LEAVE FALSE;
    }

    _DEBUG("Testing file: %s", filename);

    if (FALSE == read_metadata(filename, test_decoder, test_info)) {
        _DEBUG("File not handled by this plugin!");
        _LEAVE FALSE;
    }

    /*
     * See if the metadata has changed
     */
    if (FALSE == test_info->metadata_changed) {
        _DEBUG("No metadata found in stream");
        _LEAVE FALSE;
    }

    /*
     * If we get here, the file is supported by FLAC.
     * The stream characteristics have been filled in by
     * the metadata callback.
     * We can close the stream now.
     */

     vfs_fclose(test_info->input_stream);
     test_info->input_stream = NULL;


    _DEBUG("Stream encoded at %d Hz, %d bps, %d channels",
        test_info->stream.samplerate,
        test_info->stream.bits_per_sample,
        test_info->stream.channels);

    if (MAX_SUPPORTED_CHANNELS < test_info->stream.channels) {
        _ERROR("This number of channels (%d) is currently not supported, stream not handled by this plugin",
            test_info->stream.channels);
        _LEAVE FALSE;
    }

    if ((16 != test_info->stream.bits_per_sample) &&
        (24 != test_info->stream.bits_per_sample) &&
        (8 != test_info->stream.bits_per_sample)) {
        _ERROR("This number of bits (%d) is currently not supported, stream not handled by this plugin",
            test_info->stream.bits_per_sample);
        _LEAVE FALSE;
    }

    /*
     * Looks good.
     */

    _DEBUG("Accepting file %s", filename);

    reset_info(test_info);

    _LEAVE TRUE;
}

/* --- */

void squeeze_audio(gint32* src, void* dst, guint count, guint src_res, guint dst_res) {

    /*
     * Changes the sample width of count samples in src from
     * src_res to dst_res and places the result in dst
     */

    gint i;
    gint32* rp = src;
    gint8* wp = dst;
    gint16* wp2 = dst;
    gint32* wp4 = dst;

    _ENTER;

    _DEBUG("Converting %d samples %d->%d", count, src_res, dst_res);

    if ((src_res % 8 != 0) || (dst_res % 8 != 0)) {
        _ERROR("Can not convert from %d bps to %d bps: not a multiple of 8",
                src_res, dst_res);
        _LEAVE;
    }

    if (16 == dst_res) {
        if (8 == src_res) {
            for (i=0; i<count; i++, wp2++, rp++) {
                *wp2 = (*rp << 8) & 0xffff;
            }
        } else if (16 == src_res) {
            for (i=0; i<count; i++, wp2++, rp++) {
                *wp2 = *rp & 0xffff;
            }
        } else if (24 == src_res) {
            for (i=0; i<count; i++, wp2++, rp++) {
                *wp2 = (*rp >> 8) & 0xffff;
            }
        } else if (32 == src_res) {
            for (i=0; i<count; i++, wp2++, rp++) {
                *wp2 = (*rp >> 16) & 0xffff;
            }
        }
    } else if (8 == dst_res) {
        if (8 == src_res) {
            for (i=0; i<count; i++, wp++, rp++) {
                *wp = *rp & 0xff;
            }
        } else if (16 == src_res) {
            for (i=0; i<count; i++, wp++, rp++) {
                *wp = (*rp >> 8) & 0xff;
            }
        } else if (24 == src_res) {
            for (i=0; i<count; i++, wp++, rp++) {
                *wp = (*rp >> 16) & 0xff;
            }
        } else if (32 == src_res) {
            for (i=0; i<count; i++, wp++, rp++) {
                *wp = (*rp >> 24) & 0xff;
            }
        }
    } else if (32 == dst_res) {
        if (8 == src_res) {
            for (i=0; i<count; i++, wp4++, rp++) {
                *wp4 = (*rp << 24) & 0xffffffff;
            }
        } else if (16 == src_res) {
            for (i=0; i<count; i++, wp4++, rp++) {
                *wp4 = (*rp << 16) & 0xffffffff;
            }
        } else if (24 == src_res) {
            for (i=0; i<count; i++, wp4++, rp++) {
                *wp4 = (*rp << 8) & 0xffffffff;
            }
        } else if (32 == src_res) {
            for (i=0; i<count; i++, wp4++, rp++) {
                *wp4 = *rp;
            }
        }
    }

    _LEAVE;
}

/* --- */

static gpointer flac_play_loop(gpointer arg) {

    /*
     * The main play loop.
     * Decode a frame, push the decoded data to the output plugin
     * chunkwise. Repeat until finished.
     *
     * Must be entered with flac_pl_mutex held!
     */

    gint ofree;
    gint32* read_pointer;
    gint elements_left;
    gint seek_sample;
    FLAC__StreamDecoderState state;
    struct stream_info stream_info;
    guint sample_count;
    void* play_buffer;
    InputPlayback* playback = (InputPlayback *) arg;

    _ENTER;

    if (NULL == (play_buffer = malloc(BUFFER_SIZE_BYTE))) {
        _ERROR("Could not allocate conversion buffer");
        playback->playing = FALSE;
        g_mutex_unlock(flac_pl_mutex);
        _LEAVE NULL;
    }

    stream_info.samplerate = main_info->stream.samplerate;
    stream_info.channels = main_info->stream.channels;
    main_info->metadata_changed = FALSE;

    if (!playback->output->open_audio(FMT_S16_NE,
        main_info->stream.samplerate,
        main_info->stream.channels)) {
        playback->playing = FALSE;
        _ERROR("Could not open output plugin!");
        g_mutex_unlock(flac_pl_mutex);
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

            /*
             * Convert the audio.
             * Currently this is hardcoded to 16 bit.
             * 8 and 24 bit are sampled up/down linear.
             */
            _DEBUG("Converting %d samples to FMT_S16_NE", sample_count);
            squeeze_audio(read_pointer, play_buffer, sample_count, main_info->frame.bits_per_sample, 16);

            _DEBUG("Copying %d samples to output plugin", sample_count);

            produce_audio(flac_ip.output->written_time(), FMT_S16_NE, main_info->frame.channels, sample_count * sizeof(gint16), play_buffer, NULL);

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
         * Do we have to seek to somewhere?
         */
        if (-1 != seek_to) {
            _DEBUG("Seek requested to %d seconds", seek_to);

            seek_sample = seek_to * main_info->stream.samplerate;
            _DEBUG("Seek requested to sample %d", seek_sample);
            if (FALSE == FLAC__stream_decoder_seek_absolute(main_decoder, seek_sample)) {
                _ERROR("Could not seek to sample %d!", seek_sample);
            } else {
                /*
                 * Flush the buffers
                 */
                flac_ip.output->flush(seek_to * 1000);
            }
            seek_to = -1;
        }

        /*
         * Have we reached the end of the stream?
         */
        state = FLAC__stream_decoder_get_state(main_decoder);
        if (0 == elements_left && (FLAC__STREAM_DECODER_END_OF_STREAM == state)) {
            /*
             * Yes. Drain the output buffer and stop playing.
             */
            
            _DEBUG("End of stream reached, draining output buffer");

            while((-1 == seek_to) && flac_ip.output->buffer_playing() && playback->playing == TRUE) {
                g_usleep(40000);
            }

            if (-1 == seek_to) {
                _DEBUG("Output buffer empty.");
                playback->playing = FALSE;
            }
        }
    }

    /*
     * Clean up a bit
     */
    playback->playing = FALSE;
    _DEBUG("Closing audio device");
    flac_ip.output->close_audio();
    _DEBUG("Audio device closed");

    free(play_buffer);

    if (FALSE == FLAC__stream_decoder_flush(main_decoder)) {
        _ERROR("Could not flush decoder state!");
    }

    /*
     * Release the play loop mutex
     */
    g_mutex_unlock(flac_pl_mutex);
    
    g_thread_exit(NULL);

    _LEAVE NULL;
}

/* --- */

void flac_play_file (InputPlayback* input) {

    gint l;

    _ENTER;

    if (!plugin_initialized) {
        _ERROR("plugin not initialized!");
        _LEAVE;
    }

    /*
     * This will end a currently running decoder thread
     */
    input->playing = FALSE;
    xmms_usleep(20000);

    if (FALSE == read_metadata(input->filename, main_decoder, main_info)) {
        _ERROR("Could not prepare file for playing!");
        _LEAVE;
    }

    /*
     * Calculate the length
     */
    if (0 == main_info->stream.samplerate) {
        _ERROR("Invalid sample rate for stream!");
        l = -1;
    } else {
        l = (main_info->stream.samples / main_info->stream.samplerate) * 1000;
    }

    /*
     * Grab the play loop mutex
     */
    _DEBUG("Waiting for play loop mutex...");
    g_mutex_lock(flac_pl_mutex);
    _DEBUG("Got play loop mutex");

    input->playing = TRUE;

    flac_ip.set_info(get_title(input->filename, main_info), l, -1, main_info->stream.samplerate, main_info->stream.channels);

    thread = g_thread_create(flac_play_loop, input, TRUE, NULL);

    _LEAVE;
}

/* --- */

void flac_stop(InputPlayback* input) {

    _ENTER;

    input->playing = FALSE;

    _LEAVE;
}

/* --- */

void flac_pause(InputPlayback* input, gshort p) {

    _ENTER;

    input->output->pause(p);

    _LEAVE;
}

/* --- */

void flac_seek(InputPlayback* input, gint time) {

    _ENTER;

    if (!input->playing) {
        _DEBUG("Can not seek while not playing");
        _LEAVE;
    }

    _DEBUG("Requesting seek to %d", time);
    seek_to = time;

    while (-1 != seek_to) {
        xmms_usleep(10000);
    }

    _LEAVE;
}

/* --- */

void flac_get_song_info(gchar* filename, gchar** title, gint* length) {

    gint l;

    _ENTER;

    if (FALSE == read_metadata(filename, test_decoder, test_info)) {
        _ERROR("Could not read file info!");
        *length = -1;
        *title = g_strdup("");
        _LEAVE;
    }

    /*
     * Calculate the stream length (milliseconds)
     */
    if (0 == test_info->stream.samplerate) {
        _ERROR("Invalid sample rate for stream!");
        l = -1;
    } else {
        l = (test_info->stream.samples / test_info->stream.samplerate) * 1000;
        _DEBUG("Stream length: %d seconds", l/1000);
    }

    *length = l;
    *title = get_title(filename, test_info);

    reset_info(test_info);

    _LEAVE;
}

/* --- */

TitleInput *flac_get_song_tuple(gchar* filename) {

    gint l;
    TitleInput *tuple;

    _ENTER;

    if (FALSE == read_metadata(filename, test_decoder, test_info)) {
        _ERROR("Could not read metadata tuple for file <%s>", filename);
        _LEAVE NULL;
    }

    tuple = get_tuple(filename, test_info);

    reset_info(test_info);

    _LEAVE tuple;
}

/* --- */

void flac_aboutbox(void) {

    static GtkWidget* about_window;

    if (about_window) {
        gdk_window_raise(about_window->window);
    }

    about_window = xmms_show_message(_("About FLAC Audio Plugin"),
                                     ("FLAC Audio Plugin (" _VERSION ")\n\n"
                                      "Original code by\n"
                                      "Ralf Ertzinger <ralf@skytale.net>\n"
                                      "\n"
                                      "http://www.skytale.net/projects/bmp-flac2/"),
                                     _("OK"), FALSE, NULL, NULL);
    g_signal_connect(G_OBJECT(about_window), "destroy",
                     G_CALLBACK(gtk_widget_destroyed), &about_window);
}
