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

int url_vopen(URLContext **puc, VFSFile *fd)
{
    URLContext *uc;
    URLProtocol *up;
    int err = 0;

    up = &audvfs_protocol;
    uc = av_malloc(sizeof(URLContext) + strlen(fd->uri) + 1);
    if (!uc) {
        err = -ENOMEM;
        goto fail;
    }
    uc->filename = (char *) &uc[1];
    strcpy(uc->filename, fd->uri);
    uc->prot = up;
    uc->flags = URL_RDONLY;
    uc->is_streamed = 0; /* default = not streamed */
    uc->max_packet_size = 0; /* default: stream file */
    uc->priv_data = fd;
    if (err < 0) {
        free(uc);
        *puc = NULL;
        return err;
    }
    *puc = uc;
    return 0;
 fail:
    *puc = NULL;
    return err;
}

int url_vfdopen(ByteIOContext *s, VFSFile *fd)
{
    URLContext *h;
    int err;

    err = url_vopen(&h, fd);
    if (err < 0)
        return err;
    err = url_fdopen(&s, h);
    if (err < 0) {
        return err;
    }
    return 0;
}

int av_open_input_vfsfile(AVFormatContext **ic_ptr, const char *filename, VFSFile *fd,
                       AVInputFormat *fmt,
                       int buf_size,
                       AVFormatParameters *ap)
{
    int err, must_open_file, file_opened;
    uint8_t buf[buf_size];
    AVProbeData probe_data = {}, *pd = &probe_data;
    ByteIOContext pb1 = {}, *pb = &pb1;

    file_opened = 0;
    pd->filename = "";
    if (filename)
        pd->filename = filename;
    pd->buf = buf;
    pd->buf_size = 0;

    if (!fmt) {
        /* guess format if no file can be opened  */
        fmt = av_probe_input_format(pd, 0);
    }

    /* do not open file if the format does not need it. XXX: specific
       hack needed to handle RTSP/TCP */
    must_open_file = 1;
    if (fmt && (fmt->flags & AVFMT_NOFILE)) {
        must_open_file = 0;
    }

    if (!fmt || must_open_file) {
        /* if no file needed do not try to open one */
        if (url_vfdopen(pb, fd) < 0) {
            err = AVERROR_IO;
            _DEBUG("i/o error");
            goto fail;
        }
        file_opened = 1;
        if (buf_size > 0) {
            url_setbufsize(pb, buf_size);
        }
        if (!fmt) {
            /* read probe data */
            pd->buf_size = get_buffer(pb, pd->buf, buf_size);
            url_fseek(pb, 0, SEEK_SET);
        }
    }

    /* guess file format */
    if (!fmt) {
        fmt = av_probe_input_format(pd, 1);
    }

    /* if still no format found, error */
    if (!fmt) {
        err = AVERROR_NOFMT;
        _DEBUG("probe failed");
        goto fail;
    }

    err = av_open_input_stream(ic_ptr, pb, filename, fmt, ap);
    if (err) {
        _DEBUG("fail %d", err);
        goto fail;
    }
    return 0;
 fail:
    *ic_ptr = NULL;
    return err;
}

void av_close_input_vfsfile(AVFormatContext *s)
{
    int i;
    AVStream *st;

    for(i=0;i<s->nb_streams;i++) {
        /* free all data in a stream component */
        st = s->streams[i];
        if (st == NULL)
            continue;

        if (st->parser) {
            av_parser_close(st->parser);
        }
        free(st->index_entries);
        free(st);
        s->streams[i] = NULL;
    }
    av_freep(&s->priv_data);
    free(s);
}

