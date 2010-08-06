/*
 *  A FLAC decoder plugin for the Audacious Media Player
 *  Copyright (C) 2005 Ralf Ertzinger
 *  Copyright (C) 2010 Michał Lipski <tallica@o2.pl>
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

#include <audacious/debug.h>
#include <audacious/i18n.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "flacng.h"

#include "tools.h"
#include "seekable_stream_callbacks.h"
#include "version.h"

FLAC__StreamDecoder *test_decoder;
FLAC__StreamDecoder *main_decoder;
callback_info *test_info;
callback_info *main_info;
gboolean plugin_initialized = FALSE;
static GMutex *seek_mutex;
static GCond *seek_cond;
static gint seek_value;

static void flac_init(void)
{
    FLAC__StreamDecoderInitStatus ret;

    /* Callback structure and decoder for file test purposes */

    if ((test_info = init_callback_info()) == NULL)
    {
        ERROR("Could not initialize the test callback structure!\n");
        return;
    }

    if ((test_decoder = FLAC__stream_decoder_new()) == NULL)
    {
        ERROR("Could not create the test FLAC decoder instance!\n");
        return;
    }

    FLAC__stream_decoder_set_metadata_respond(test_decoder, FLAC__METADATA_TYPE_VORBIS_COMMENT);

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
        test_info)))
    {
        ERROR("Could not initialize the test FLAC decoder: %s(%d)\n",
            FLAC__StreamDecoderInitStatusString[ret], ret);
        return;
    }

    /* Callback structure and decoder for main decoding loop */

    if ((main_info = init_callback_info()) == NULL)
    {
        ERROR("Could not initialize the main callback structure!\n");
        return;
    }

    if ((main_decoder = FLAC__stream_decoder_new()) == NULL)
    {
        ERROR("Could not create the main FLAC decoder instance!\n");
        return;
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
        ERROR("Could not initialize the main FLAC decoder: %s(%d)\n",
            FLAC__StreamDecoderInitStatusString[ret], ret);
        return;
    }

    seek_mutex = g_mutex_new();
    seek_cond = g_cond_new();

    AUDDBG("Plugin initialized.\n");
    plugin_initialized = TRUE;
}

static void flac_cleanup(void)
{
    g_mutex_free(seek_mutex);
    g_cond_free(seek_cond);

    FLAC__stream_decoder_delete(main_decoder);
    clean_callback_info(main_info);

    FLAC__stream_decoder_delete(test_decoder);
    clean_callback_info(test_info);

    plugin_initialized = FALSE;
}

gboolean flac_is_our_fd(const gchar *filename, VFSFile *fd)
{
    AUDDBG("Probe for FLAC.\n");

    gchar *buf = g_new0(gchar, 4);
    gboolean flac;

    if (vfs_fseek(fd, 0, SEEK_SET))
    {
        g_free(buf);
        return FALSE;
    }

    vfs_fread(buf, 4, 1, fd);
    flac = strncmp(buf, "fLaC", 4);
    g_free(buf);

    if (!flac)
        return TRUE;
    else
        return FALSE;
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
            ERROR("Can not convert to %d bps\n", res);
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

    if (!plugin_initialized)
    {
        ERROR("Plugin not initialized!\n");
        return FALSE;
    }

    main_info->fd = file;

    if (read_metadata(main_decoder, main_info) == FALSE)
    {
        ERROR("Could not prepare file for playing!\n");
        goto ERR_NO_CLOSE;
    }

    if (main_info->stream.channels > MAX_SUPPORTED_CHANNELS)
    {
        ERROR("This number of channels (%d) is currently not supported, stream not handled by this plugin.\n",
            main_info->stream.channels);
        goto ERR_NO_CLOSE;
    }

    if (main_info->stream.bits_per_sample != 8  &&
        main_info->stream.bits_per_sample != 16 &&
        main_info->stream.bits_per_sample != 24 &&
        main_info->stream.bits_per_sample != 32)
    {
        ERROR("This number of bits (%d) is currently not supported, stream not handled by this plugin.\n",
            main_info->stream.bits_per_sample);
        goto ERR_NO_CLOSE;
    }

    if ((play_buffer = g_malloc0(BUFFER_SIZE_BYTE)) == NULL)
    {
        ERROR("Could not allocate conversion buffer\n");
        goto ERR_NO_CLOSE;
    }

    if (! playback->output->open_audio (SAMPLE_FMT
     (main_info->stream.bits_per_sample), main_info->stream.samplerate,
     main_info->stream.channels))
    {
        ERROR("Could not open output plugin!\n");
        goto ERR_NO_CLOSE;
    }

    if (pause)
        playback->output->pause (TRUE);

    seek_value = (start_time > 0) ? start_time : -1;
    playback->playing = TRUE;

    playback->set_params(playback, NULL, 0, main_info->bitrate,
        main_info->stream.samplerate, main_info->stream.channels);
    playback->set_pb_ready(playback);

    stream_info.samplerate = main_info->stream.samplerate;
    stream_info.channels = main_info->stream.channels;
    main_info->metadata_changed = FALSE;

    playback->set_gain_from_playlist(playback);

    while (playback->playing)
    {
        if (stop_time >= 0 && playback->output->written_time () >= stop_time)
            goto DRAIN;

        /* Try to decode a single frame of audio */
        if (FLAC__stream_decoder_process_single(main_decoder) == FALSE)
        {
            ERROR("Error while decoding!\n");
            goto CLEANUP;
        }

        /* Maybe the metadata changed midstream. Check. */
        if (main_info->metadata_changed)
        {
            /* Samplerate and channels are important */
            if (stream_info.samplerate != main_info->stream.samplerate)
            {
                ERROR("Samplerate changed midstream (now: %d, was: %d). This is not supported yet.\n",
                    main_info->stream.samplerate, stream_info.samplerate);
                goto CLEANUP;
            }

            if (stream_info.channels != main_info->stream.channels)
            {
                ERROR("Number of channels changed midstream (now: %d, was: %d). This is not supported yet.\n",
                    main_info->stream.channels, stream_info.channels);
                goto CLEANUP;
            }

            main_info->metadata_changed = FALSE;
        }

        /* Compare the frame metadata to the current stream metadata */
        if (main_info->stream.samplerate != main_info->frame.samplerate)
        {
            ERROR("Frame samplerate mismatch (stream: %d, frame: %d)!\n",
                main_info->stream.samplerate, main_info->frame.samplerate);
            goto CLEANUP;
        }

        if (main_info->stream.channels != main_info->frame.channels)
        {
            ERROR("Frame channel mismatch (stream: %d, frame: %d)!\n",
                main_info->stream.channels, main_info->frame.channels);
            goto CLEANUP;
        }

        g_mutex_lock(seek_mutex);

        if (seek_value >= 0)
        {
            playback->output->flush (seek_value);
            FLAC__stream_decoder_seek_absolute (main_decoder, (gint64)
             seek_value * main_info->stream.samplerate / 1000);
            seek_value = -1;
            g_cond_signal(seek_cond);
        }

        g_mutex_unlock(seek_mutex);

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

        while (playback->playing && elements_left != 0)
        {
            if (stop_time >= 0 && playback->output->written_time () >= stop_time)
                goto DRAIN;

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

DRAIN:
            while (playback->output->buffer_playing() && playback->playing)
                g_usleep(20000);

            goto CLEANUP;
        }
    }

CLEANUP:
    g_mutex_lock(seek_mutex);
    playback->playing = FALSE;
    g_cond_signal(seek_cond); /* wake up any waiting request */
    g_mutex_unlock(seek_mutex);

    AUDDBG("Closing audio device.\n");
    playback->output->close_audio();
    AUDDBG("Audio device closed.\n");

ERR_NO_CLOSE:
    if (play_buffer)
        g_free(play_buffer);

    reset_info(main_info);

    if (FLAC__stream_decoder_flush(main_decoder) == FALSE)
        ERROR("Could not flush decoder state!\n");

    return ! playback->error;
}

static void flac_stop(InputPlayback *playback)
{
    g_mutex_lock(seek_mutex);

    if (playback->playing)
    {
        playback->playing = FALSE;
        playback->output->abort_write();
        g_cond_signal(seek_cond);
    }

    g_mutex_unlock (seek_mutex);
}

static void flac_pause(InputPlayback *playback, gshort pause)
{
    g_mutex_lock(seek_mutex);

    if (playback->playing)
        playback->output->pause(pause);

    g_mutex_unlock(seek_mutex);
}

static void flac_seek (InputPlayback * playback, gulong time)
{
    g_mutex_lock(seek_mutex);

    if (playback->playing)
    {
        seek_value = time;
        playback->output->abort_write();
        g_cond_signal(seek_cond);
        g_cond_wait(seek_cond, seek_mutex);
    }

    g_mutex_unlock(seek_mutex);
}

static Tuple *flac_probe_for_tuple(const gchar *filename, VFSFile *fd)
{
    AUDDBG("Probe for tuple.\n");
    Tuple *tuple = NULL;

    g_mutex_lock(test_info->mutex);

    test_info->fd = fd;

    if (read_metadata(test_decoder, test_info))
        tuple = get_tuple_from_file(filename, test_info);
    else
        ERROR("Could not read metadata tuple for file <%s>\n", filename);

    reset_info(test_info);
    g_mutex_unlock(test_info->mutex);

    return tuple;
}

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

    audgui_simple_message (& about_window, GTK_MESSAGE_INFO,
     _("About FLAC Audio Plugin"), about_text);

    g_free(about_text);
}

static void insert_str_tuple_to_vc (FLAC__StreamMetadata * vc_block,
 const Tuple * tuple, gint tuple_name, gchar * field_name)
{
    FLAC__StreamMetadata_VorbisComment_Entry entry;
    gchar *str;
    gchar *val = (gchar *) tuple_get_string(tuple, tuple_name, NULL);

    if (val == NULL)
        return;

    str  = g_strdup_printf("%s=%s", field_name, val);
    entry.entry = (FLAC__byte *) str;
    entry.length = strlen(str);
    FLAC__metadata_object_vorbiscomment_replace_comment(vc_block, entry, false, true);
    g_free(str);
}

static void insert_int_tuple_to_vc (FLAC__StreamMetadata * vc_block,
 const Tuple * tuple, gint tuple_name, gchar * field_name)
{
    FLAC__StreamMetadata_VorbisComment_Entry entry;
    gchar *str;
    gint val = tuple_get_int(tuple, tuple_name, NULL);

    if (val <= 0)
        return;

    if (FIELD_TRACK_NUMBER == tuple_name)
        str  = g_strdup_printf("%s=%.2d", field_name, val);
    else
        str  = g_strdup_printf("%s=%d", field_name, val);

    entry.entry = (FLAC__byte *) str;
    entry.length = strlen(str);
    FLAC__metadata_object_vorbiscomment_replace_comment(vc_block, entry, false, true);
    g_free(str);
}

gboolean flac_update_song_tuple (const Tuple * tuple, VFSFile * fd)
{
    AUDDBG("Update song tuple.\n");
    FLAC__Metadata_SimpleIterator *iter;
    FLAC__StreamMetadata *vc_block;
    gchar *filename;
    FLAC__bool ret;

    if (fd != NULL)
        filename = g_filename_from_uri(fd->uri, NULL, NULL);
    else
        return FALSE;

    iter = FLAC__metadata_simple_iterator_new();
    g_return_val_if_fail(iter != NULL, FALSE);

    ret = FLAC__metadata_simple_iterator_init(iter, filename, false, false);

    if (!ret)
    {
        FLAC__metadata_simple_iterator_delete(iter);
        return FALSE;
    }

    while (FLAC__metadata_simple_iterator_get_block_type(iter) != FLAC__METADATA_TYPE_VORBIS_COMMENT)
    {
        if (!FLAC__metadata_simple_iterator_next(iter))
            break;
    }

    if (FLAC__metadata_simple_iterator_get_block_type(iter) == FLAC__METADATA_TYPE_VORBIS_COMMENT)
        vc_block = FLAC__metadata_simple_iterator_get_block(iter);
    else
        vc_block = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);

    insert_str_tuple_to_vc(vc_block, tuple, FIELD_TITLE, "TITLE");
    insert_str_tuple_to_vc(vc_block, tuple, FIELD_ARTIST, "ARTIST");
    insert_str_tuple_to_vc(vc_block, tuple, FIELD_ALBUM, "ALBUM");
    insert_str_tuple_to_vc(vc_block, tuple, FIELD_GENRE, "GENRE");
    insert_str_tuple_to_vc(vc_block, tuple, FIELD_COMMENT, "COMMENT");

    insert_int_tuple_to_vc(vc_block, tuple, FIELD_YEAR, "DATE");
    insert_int_tuple_to_vc(vc_block, tuple, FIELD_TRACK_NUMBER, "TRACKNUMBER");

    if (FLAC__metadata_simple_iterator_get_block_type(iter) == FLAC__METADATA_TYPE_VORBIS_COMMENT)
        ret = FLAC__metadata_simple_iterator_set_block(iter, vc_block, true);
    else
        ret = FLAC__metadata_simple_iterator_insert_block_after(iter, vc_block, true);

    FLAC__metadata_simple_iterator_delete(iter);
    FLAC__metadata_object_delete(vc_block);

    if (!ret)
        return FALSE;
    else
        return TRUE;
}

static gboolean flac_get_image(const gchar *filename, VFSFile *fd, void **data, gint *length)
{
    AUDDBG("Probe for song image.\n");

    FLAC__Metadata_SimpleIterator *iter;
    FLAC__StreamMetadata *metadata = NULL;
    FLAC__bool ret;
    gboolean has_image = FALSE;

    if (fd != NULL)
        filename = g_filename_from_uri(fd->uri, NULL, NULL);
    else
        return FALSE;

    iter = FLAC__metadata_simple_iterator_new();
    g_return_val_if_fail(iter != NULL, FALSE);

    ret = FLAC__metadata_simple_iterator_init(iter, filename, false, false);

    if (!ret)
        goto CLEANUP;

    while (FLAC__metadata_simple_iterator_get_block_type(iter) != FLAC__METADATA_TYPE_PICTURE)
    {
        if (!FLAC__metadata_simple_iterator_next(iter))
            break;
    }

    if (FLAC__metadata_simple_iterator_get_block_type(iter) == FLAC__METADATA_TYPE_PICTURE)
    {
        metadata = FLAC__metadata_simple_iterator_get_block(iter);

        if (metadata->data.picture.type == FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER)
        {
            AUDDBG("FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER found.");

            *data = g_memdup(metadata->data.picture.data, metadata->data.picture.data_length);
            *length = (guint32) metadata->data.picture.data_length;
            has_image = TRUE;
            goto CLEANUP;
        }
    }

CLEANUP:
    FLAC__metadata_simple_iterator_delete(iter);

    if (metadata)
        FLAC__metadata_object_delete(metadata);

    return has_image;
}

static const gchar *flac_fmts[] = { "flac", "fla", NULL };

InputPlugin flac_ip = {
    .description = "FLACng Audio Plugin",
    .init = flac_init,
    .cleanup = flac_cleanup,
    .about = flac_aboutbox,
    .play = flac_play,
    .stop = flac_stop,
    .pause = flac_pause,
    .mseek = flac_seek,
    .probe_for_tuple = flac_probe_for_tuple,
    .is_our_file_from_vfs = flac_is_our_fd,
    .vfs_extensions = flac_fmts,
    .update_song_tuple = flac_update_song_tuple,
    .get_song_image = flac_get_image,
    .priority = 1
};

InputPlugin *flac_iplist[] = { &flac_ip, NULL };

SIMPLE_INPUT_PLUGIN (flacng, flac_iplist)
