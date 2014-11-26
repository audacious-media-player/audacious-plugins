/*
 *  A FLAC decoder plugin for the Audacious Media Player
 *  Copyright (C) 2005 Ralf Ertzinger
 *  Copyright (C) 2010-2012 Michał Lipski
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

#include <libaudcore/runtime.h>

#include "flacng.h"

EXPORT FLACng aud_plugin_instance;

static FLAC__StreamDecoder *decoder;
static callback_info *cinfo;

bool FLACng::init()
{
    FLAC__StreamDecoderInitStatus ret;

    /* Callback structure and decoder for main decoding loop */

    cinfo = new callback_info;

    if ((decoder = FLAC__stream_decoder_new()) == nullptr)
    {
        AUDERR("Could not create the main FLAC decoder instance!\n");
        return false;
    }

    if (FLAC__STREAM_DECODER_INIT_STATUS_OK != (ret = FLAC__stream_decoder_init_stream(
        decoder,
        read_callback,
        seek_callback,
        tell_callback,
        length_callback,
        eof_callback,
        write_callback,
        metadata_callback,
        error_callback,
        cinfo)))
    {
        AUDERR("Could not initialize the main FLAC decoder: %s(%d)\n",
            FLAC__StreamDecoderInitStatusString[ret], ret);
        return false;
    }

    AUDDBG("Plugin initialized.\n");
    return true;
}

void FLACng::cleanup()
{
    FLAC__stream_decoder_delete(decoder);
    delete cinfo;
}

bool FLACng::is_our_file(const char *filename, VFSFile &file)
{
    AUDDBG("Probe for FLAC.\n");

    char buf[4];
    if (file.fread (buf, 1, sizeof buf) != sizeof buf)
        return false;

    return ! strncmp (buf, "fLaC", sizeof buf);
}

static void squeeze_audio(int32_t* src, void* dst, unsigned count, unsigned res)
{
    int32_t* rp = src;
    int8_t*  wp = (int8_t*) dst;
    int16_t* wp2 = (int16_t*) dst;
    int32_t* wp4 = (int32_t*) dst;

    switch (res)
    {
        case 8:
            for (unsigned i = 0; i < count; i++, wp++, rp++)
                *wp = *rp & 0xff;
            break;

        case 16:
            for (unsigned i = 0; i < count; i++, wp2++, rp++)
                *wp2 = *rp & 0xffff;
            break;

        case 24:
        case 32:
            for (unsigned i = 0; i < count; i++, wp4++, rp++)
                *wp4 = *rp;
            break;

        default:
            AUDERR("Can not convert to %u bps\n", res);
    }
}

bool FLACng::play(const char *filename, VFSFile &file)
{
    Index<char> play_buffer;
    bool error = false;

    cinfo->fd = &file;

    if (read_metadata(decoder, cinfo) == false)
    {
        AUDERR("Could not prepare file for playing!\n");
        error = true;
        goto ERR_NO_CLOSE;
    }

    play_buffer.resize(BUFFER_SIZE_BYTE);

    set_stream_bitrate(cinfo->bitrate);
    open_audio(SAMPLE_FMT(cinfo->bits_per_sample), cinfo->sample_rate, cinfo->channels);

    while (FLAC__stream_decoder_get_state(decoder) != FLAC__STREAM_DECODER_END_OF_STREAM)
    {
        if (check_stop ())
            break;

        int seek_value = check_seek ();
        if (seek_value >= 0)
            FLAC__stream_decoder_seek_absolute (decoder, (int64_t)
             seek_value * cinfo->sample_rate / 1000);

        /* Try to decode a single frame of audio */
        if (FLAC__stream_decoder_process_single(decoder) == false)
        {
            AUDERR("Error while decoding!\n");
            error = true;
            break;
        }

        squeeze_audio(cinfo->output_buffer.begin(), play_buffer.begin(),
         cinfo->buffer_used, cinfo->bits_per_sample);
        write_audio(play_buffer.begin(), cinfo->buffer_used *
         SAMPLE_SIZE(cinfo->bits_per_sample));

        cinfo->reset();
    }

ERR_NO_CLOSE:
    cinfo->reset();

    if (FLAC__stream_decoder_flush(decoder) == false)
        AUDERR("Could not flush decoder state!\n");

    return ! error;
}

const char FLACng::about[] =
 N_("Original code by\n"
    "Ralf Ertzinger <ralf@skytale.net>\n\n"
    "http://www.skytale.net/projects/bmp-flac2/");

const char *const FLACng::exts[] = { "flac", "fla", nullptr };
