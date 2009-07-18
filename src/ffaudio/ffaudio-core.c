/*
 * Audacious FFaudio Plugin
 * Copyright © 2009 William Pitcock <nenolod@dereferenced.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#define FFAUDIO_DEBUG
#include "ffaudio-stdinc.h"

/***********************************************************************************
 * Plugin glue.                                                                    *
 ***********************************************************************************/

void
ffaudio_init(void)
{
    AVCodec *c = NULL;

    _DEBUG("initializing avcodec");
    avcodec_init();

    _DEBUG("registering all codecs");
    avcodec_register_all();

    _DEBUG("avcodec initialized, following codecs are supported");
    while ((c = av_codec_next(c)) != NULL)
    {
        _DEBUG("  - %s", c->name);         
    }

    _DEBUG("registering all formats");
    av_register_all();

    _DEBUG("registering audvfs protocol");
    av_register_protocol(&audvfs_protocol);

    _DEBUG("initialization completed");
}

gint
ffaudio_probe(gchar *filename, VFSFile *file)
{
    AVCodec *codec2 = NULL;
    AVCodecContext *c2 = NULL;
    AVFormatContext *ic2 = NULL;
    gint i;

    _DEBUG("probing for %s, filehandle %p", filename, file);

    if (av_open_input_vfsfile(&ic2, filename, file, NULL, 0, NULL) < 0)
        return 0;

    for (i = 0; i < ic2->nb_streams; i++)
    {
        c2 = ic2->streams[i]->codec;
        if(c2->codec_type == CODEC_TYPE_AUDIO)
        {
            av_find_stream_info(ic2);
            codec2 = avcodec_find_decoder(c2->codec_id);
            if (codec2)
                break;
        }
    }

    if (!codec2)
        return 0;

    av_find_stream_info(ic2);

    codec2 = avcodec_find_decoder(c2->codec_id);

    if (!codec2)
    {
        av_close_input_vfsfile(ic2);
        return 0;
    }

    av_close_input_vfsfile(ic2);

    return 1;
}

void
ffaudio_play(InputPlayback *playback)
{

}

gchar *ffaudio_fmts[] = { "mpc", NULL };

InputPlugin ffaudio_ip = {
    .init = ffaudio_init,
    .is_our_file_from_vfs = ffaudio_probe,
    .description = "FFaudio Plugin",
    .vfs_extensions = ffaudio_fmts,
};

InputPlugin *ffaudio_iplist[] = { &ffaudio_ip, NULL };

SIMPLE_INPUT_PLUGIN(ffaudio, ffaudio_iplist);
