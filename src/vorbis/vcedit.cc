/* This program is licensed under the GNU Library General Public License, version 2,
 * a copy of which is included with this program (LICENCE.LGPL).
 *
 * (c) 2000-2001 Michael Smith <msmith@labyrinth.net.au>
 *
 * Comment editing backend, suitable for use by nice frontend interfaces.
 *
 * Somewhat cleaned up for C++ by John Lindgren, 2015.
 */
#include <stdlib.h>
#include <string.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "vcedit.h"

#define CHUNKSIZE 4096

/* Next two functions pulled straight from libvorbis, apart from one change
 * - we don't want to overwrite the vendor string.
 */
static void
_v_writestring(oggpack_buffer * o, const char *s, int len)
{
    while (len--) {
        oggpack_write(o, *s++, 8);
    }
}

static void
_commentheader_out(vorbis_comment * vc, const char *vendor, ogg_packet * op)
{
    oggpack_buffer opb;

    oggpack_writeinit(&opb);

    /* preamble */
    oggpack_write(&opb, 0x03, 8);
    _v_writestring(&opb, "vorbis", 6);

    /* vendor */
    oggpack_write(&opb, strlen(vendor), 32);
    _v_writestring(&opb, vendor, strlen(vendor));

    /* comments */
    oggpack_write(&opb, vc->comments, 32);
    if (vc->comments) {
        int i;
        for (i = 0; i < vc->comments; i++) {
            if (vc->user_comments[i]) {
                oggpack_write(&opb, vc->comment_lengths[i], 32);
                _v_writestring(&opb, vc->user_comments[i],
                               vc->comment_lengths[i]);
            }
            else {
                oggpack_write(&opb, 0, 32);
            }
        }
    }
    oggpack_write(&opb, 1, 1);

    op->packet = (unsigned char *) _ogg_malloc(oggpack_bytes(&opb));
    memcpy(op->packet, opb.buffer, oggpack_bytes(&opb));

    op->bytes = oggpack_bytes(&opb);
    op->b_o_s = 0;
    op->e_o_s = 0;
    op->granulepos = 0;
}

static int
_blocksize(vcedit_state * s, ogg_packet * p)
{
    int size = vorbis_packet_blocksize(&s->vi, p);
    int ret = (size + s->prevW) / 4;

    if (!s->prevW) {
        s->prevW = size;
        return 0;
    }

    s->prevW = size;
    return ret;
}

static bool
_fetch_next_packet(vcedit_state * s, VFSFile & in, ogg_packet * p, ogg_page * page)
{
    int result;
    char *buffer;
    int64_t bytes;

    result = ogg_stream_packetout(&s->os, p);

    if (result > 0)
        return true;
    else {
        if (s->eosin)
            return false;
        while (ogg_sync_pageout(&s->oy, page) <= 0) {
            buffer = ogg_sync_buffer(&s->oy, CHUNKSIZE);
            bytes = in.fread(buffer, 1, CHUNKSIZE);
            ogg_sync_wrote(&s->oy, bytes);
            if (bytes == 0)
                return false;
        }
        if (ogg_page_eos(page))
            s->eosin = true;
        else if (ogg_page_serialno(page) != s->serial) {
            s->eosin = true;
            s->extrapage = true;
            return false;
        }

        ogg_stream_pagein(&s->os, page);
        return _fetch_next_packet(s, in, p, page);
    }
}

bool
vcedit_open(vcedit_state * state, VFSFile & in)
{
    char *buffer;
    int64_t bytes;
    int i;
    ogg_packet *header;
    ogg_packet header_main;
    ogg_packet header_comments;
    ogg_packet header_codebooks;
    ogg_page og;

    buffer = ogg_sync_buffer(&state->oy, CHUNKSIZE);

    bytes = in.fread(buffer, 1, CHUNKSIZE);

    ogg_sync_wrote(&state->oy, bytes);

    if (ogg_sync_pageout(&state->oy, &og) != 1) {
        if (bytes < CHUNKSIZE)
            state->lasterror = "Input truncated or empty.";
        else
            state->lasterror = "Input is not an Ogg bitstream.";
        goto err;
    }

    state->serial = ogg_page_serialno(&og);

    ogg_stream_reset_serialno(&state->os, state->serial);

    if (ogg_stream_pagein(&state->os, &og) < 0) {
        state->lasterror = "Error reading first page of Ogg bitstream.";
        goto err;
    }

    if (ogg_stream_packetout(&state->os, &header_main) != 1) {
        state->lasterror = "Error reading initial header packet.";
        goto err;
    }

    if (vorbis_synthesis_headerin(&state->vi, &state->vc, &header_main) < 0) {
        state->lasterror = "Ogg bitstream does not contain vorbis data.";
        goto err;
    }

    state->mainbuf.clear();
    state->mainbuf.insert(header_main.packet, 0, header_main.bytes);

    i = 0;
    header = &header_comments;
    while (i < 2) {
        while (i < 2) {
            int result = ogg_sync_pageout(&state->oy, &og);
            if (result == 0)
                break;          /* Too little data so far */
            else if (result == 1) {
                ogg_stream_pagein(&state->os, &og);
                while (i < 2) {
                    result = ogg_stream_packetout(&state->os, header);
                    if (result == 0)
                        break;
                    if (result == -1) {
                        state->lasterror = "Corrupt secondary header.";
                        goto err;
                    }
                    vorbis_synthesis_headerin(&state->vi, &state->vc, header);
                    if (i == 1) {
                        state->bookbuf.clear();
                        state->bookbuf.insert(header->packet, 0, header->bytes);
                    }
                    i++;
                    header = &header_codebooks;
                }
            }
        }

        buffer = ogg_sync_buffer(&state->oy, CHUNKSIZE);
        bytes = in.fread(buffer, 1, CHUNKSIZE);
        if (bytes == 0 && i < 2) {
            state->lasterror = "EOF before end of vorbis headers.";
            goto err;
        }
        ogg_sync_wrote(&state->oy, bytes);
    }

    /* Copy the vendor tag */
    state->vendor = String(state->vc.vendor);

    /* Headers are done! */
    return true;

  err:
    return false;
}

bool
vcedit_write(vcedit_state * state, VFSFile & in, VFSFile & out)
{
    ogg_stream_state streamout;
    ogg_packet header_main;
    ogg_packet header_comments;
    ogg_packet header_codebooks;

    ogg_page ogout, ogin;
    ogg_packet op;
    ogg_int64_t granpos = 0;
    int result;
    char *buffer;
    int64_t bytes;
    int needflush = 0, needout = 0;

    state->eosin = false;
    state->extrapage = false;

    header_main.bytes = state->mainbuf.len();
    header_main.packet = state->mainbuf.begin();
    header_main.b_o_s = 1;
    header_main.e_o_s = 0;
    header_main.granulepos = 0;

    header_codebooks.bytes = state->bookbuf.len();
    header_codebooks.packet = state->bookbuf.begin();
    header_codebooks.b_o_s = 0;
    header_codebooks.e_o_s = 0;
    header_codebooks.granulepos = 0;

    ogg_stream_init(&streamout, state->serial);

    _commentheader_out(&state->vc, state->vendor, &header_comments);

    ogg_stream_packetin(&streamout, &header_main);
    ogg_stream_packetin(&streamout, &header_comments);
    ogg_stream_packetin(&streamout, &header_codebooks);

    while ((result = ogg_stream_flush(&streamout, &ogout))) {
        if (out.fwrite(ogout.header, 1, ogout.header_len) != ogout.header_len)
            goto cleanup;
        if (out.fwrite(ogout.body, 1, ogout.body_len) != ogout.body_len)
            goto cleanup;
    }

    while (_fetch_next_packet(state, in, &op, &ogin)) {
        int size;
        size = _blocksize(state, &op);
        granpos += size;

        if (needflush) {
            if (ogg_stream_flush(&streamout, &ogout)) {
                if (out.fwrite(ogout.header, 1, ogout.header_len) != ogout.header_len)
                    goto cleanup;
                if (out.fwrite(ogout.body, 1, ogout.body_len) != ogout.body_len)
                    goto cleanup;
            }
        }
        else if (needout) {
            if (ogg_stream_pageout(&streamout, &ogout)) {
                if (out.fwrite(ogout.header, 1, ogout.header_len) != ogout.header_len)
                    goto cleanup;
                if (out.fwrite(ogout.body, 1, ogout.body_len) != ogout.body_len)
                    goto cleanup;
            }
        }

        needflush = needout = 0;

        if (op.granulepos == -1) {
            op.granulepos = granpos;
            ogg_stream_packetin(&streamout, &op);
        }
        else {                  /* granulepos is set, validly. Use it, and force a flush to
                                   account for shortened blocks (vcut) when appropriate */
            if (granpos > op.granulepos) {
                granpos = op.granulepos;
                ogg_stream_packetin(&streamout, &op);
                needflush = 1;
            }
            else {
                ogg_stream_packetin(&streamout, &op);
                needout = 1;
            }
        }
    }

    streamout.e_o_s = 1;
    while (ogg_stream_flush(&streamout, &ogout)) {
        if (out.fwrite(ogout.header, 1, ogout.header_len) != ogout.header_len)
            goto cleanup;
        if (out.fwrite(ogout.body, 1, ogout.body_len) != ogout.body_len)
            goto cleanup;
    }

    if (state->extrapage) {
        if (out.fwrite(ogin.header, 1, ogin.header_len) != ogin.header_len)
            goto cleanup;
        if (out.fwrite(ogin.body, 1, ogin.body_len) != ogin.body_len)
            goto cleanup;
    }

    state->eosin = false;       /* clear it, because not all paths to here do */
    while (!state->eosin) {     /* We reached eos, not eof */
        /* We copy the rest of the stream (other logical streams)
         * through, a page at a time. */
        while (1) {
            result = ogg_sync_pageout(&state->oy, &ogout);
            if (result == 0)
                break;
            if (result < 0)
                state->lasterror = "Corrupt or missing data, continuing...";
            else {
                /* Don't bother going through the rest, we can just
                 * write the page out now */
                if (out.fwrite(ogout.header, 1, ogout.header_len) != ogout.header_len)
                    goto cleanup;
                if (out.fwrite(ogout.body, 1, ogout.body_len) != ogout.body_len)
                    goto cleanup;
            }
        }
        buffer = ogg_sync_buffer(&state->oy, CHUNKSIZE);
        bytes = in.fread(buffer, 1, CHUNKSIZE);
        ogg_sync_wrote(&state->oy, bytes);
        if (bytes == 0) {
            state->eosin = true;
            break;
        }
    }

  cleanup:
    ogg_stream_clear(&streamout);
    ogg_packet_clear(&header_comments);

    if (!state->eosin) {
        state->lasterror =
            "Error writing stream to output. "
            "Output stream may be corrupted or truncated.";
        return false;
    }

    return true;
}
