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

VCEdit::VCEdit()
{
    ogg_sync_init(&oy);
    ogg_stream_init(&os, 0);
    vorbis_comment_init(&vc);
    vorbis_info_init(&vi);
}

VCEdit::~VCEdit()
{
    ogg_sync_clear(&oy);
    ogg_stream_clear(&os);
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);
}

/* Next two functions pulled straight from libvorbis, apart from one change
 * - we don't want to overwrite the vendor string.
 */
static void
_v_writestring(oggpack_buffer *o, const char *s, int len)
{
    while (len--) {
        oggpack_write(o, *s++, 8);
    }
}

static void
_commentheader_out(vorbis_comment *vc, const char *vendor, ogg_packet *op)
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

int VCEdit::blocksize(ogg_packet *p)
{
    int size = vorbis_packet_blocksize(&vi, p);
    int ret = (size + prevW) / 4;

    if (!prevW) {
        prevW = size;
        return 0;
    }

    prevW = size;
    return ret;
}

bool VCEdit::fetch_next_packet(VFSFile &in, ogg_packet *p, ogg_page *page)
{
    if (ogg_stream_packetout(&os, p) > 0)
        return true;
    else {
        if (eosin)
            return false;
        while (ogg_sync_pageout(&oy, page) <= 0) {
            char *buffer = ogg_sync_buffer(&oy, CHUNKSIZE);
            int64_t bytes = in.fread(buffer, 1, CHUNKSIZE);
            ogg_sync_wrote(&oy, bytes);
            if (bytes == 0)
                return false;
        }
        if (ogg_page_eos(page))
            eosin = true;
        else if (ogg_page_serialno(page) != serial) {
            eosin = true;
            extrapage = true;
            return false;
        }

        ogg_stream_pagein(&os, page);
        return fetch_next_packet(in, p, page);
    }
}

bool VCEdit::open(VFSFile &in)
{
    ogg_packet header_main;
    ogg_packet header_comments;
    ogg_packet header_codebooks;
    ogg_page og;

    char *buffer = ogg_sync_buffer(&oy, CHUNKSIZE);

    int64_t bytes = in.fread(buffer, 1, CHUNKSIZE);

    ogg_sync_wrote(&oy, bytes);

    if (ogg_sync_pageout(&oy, &og) != 1) {
        if (bytes < CHUNKSIZE)
            lasterror = "Input truncated or empty.";
        else
            lasterror = "Input is not an Ogg bitstream.";
        return false;
    }

    serial = ogg_page_serialno(&og);

    ogg_stream_reset_serialno(&os, serial);

    if (ogg_stream_pagein(&os, &og) < 0) {
        lasterror = "Error reading first page of Ogg bitstream.";
        return false;
    }

    if (ogg_stream_packetout(&os, &header_main) != 1) {
        lasterror = "Error reading initial header packet.";
        return false;
    }

    if (vorbis_synthesis_headerin(&vi, &vc, &header_main) < 0) {
        lasterror = "Ogg bitstream does not contain vorbis data.";
        return false;
    }

    mainbuf.clear();
    mainbuf.insert(header_main.packet, 0, header_main.bytes);

    int i = 0;
    ogg_packet *header = &header_comments;
    while (i < 2) {
        while (i < 2) {
            int result = ogg_sync_pageout(&oy, &og);
            if (result == 0)
                break;          /* Too little data so far */
            else if (result == 1) {
                ogg_stream_pagein(&os, &og);
                while (i < 2) {
                    result = ogg_stream_packetout(&os, header);
                    if (result == 0)
                        break;
                    if (result == -1) {
                        lasterror = "Corrupt secondary header.";
                        return false;
                    }
                    vorbis_synthesis_headerin(&vi, &vc, header);
                    if (i == 1) {
                        bookbuf.clear();
                        bookbuf.insert(header->packet, 0, header->bytes);
                    }
                    i++;
                    header = &header_codebooks;
                }
            }
        }

        buffer = ogg_sync_buffer(&oy, CHUNKSIZE);
        bytes = in.fread(buffer, 1, CHUNKSIZE);
        if (bytes == 0 && i < 2) {
            lasterror = "EOF before end of vorbis headers.";
            return false;
        }
        ogg_sync_wrote(&oy, bytes);
    }

    /* Copy the vendor tag */
    vendor = String(vc.vendor);

    /* Headers are done! */
    return true;
}

bool VCEdit::write(VFSFile &in, VFSFile &out)
{
    ogg_stream_state streamout;
    ogg_packet header_main;
    ogg_packet header_comments;
    ogg_packet header_codebooks;

    ogg_page ogout, ogin;
    ogg_packet op;
    ogg_int64_t granpos = 0;
    bool needflush = false;
    bool needout = false;

    eosin = false;
    extrapage = false;

    header_main.bytes = mainbuf.len();
    header_main.packet = mainbuf.begin();
    header_main.b_o_s = 1;
    header_main.e_o_s = 0;
    header_main.granulepos = 0;

    header_codebooks.bytes = bookbuf.len();
    header_codebooks.packet = bookbuf.begin();
    header_codebooks.b_o_s = 0;
    header_codebooks.e_o_s = 0;
    header_codebooks.granulepos = 0;

    ogg_stream_init(&streamout, serial);

    _commentheader_out(&vc, vendor, &header_comments);

    ogg_stream_packetin(&streamout, &header_main);
    ogg_stream_packetin(&streamout, &header_comments);
    ogg_stream_packetin(&streamout, &header_codebooks);

    while (ogg_stream_flush(&streamout, &ogout)) {
        if (out.fwrite(ogout.header, 1, ogout.header_len) != ogout.header_len)
            goto cleanup;
        if (out.fwrite(ogout.body, 1, ogout.body_len) != ogout.body_len)
            goto cleanup;
    }

    while (fetch_next_packet(in, &op, &ogin)) {
        granpos += blocksize(&op);

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

        needflush = false;
        needout = false;

        if (op.granulepos == -1) {
            op.granulepos = granpos;
            ogg_stream_packetin(&streamout, &op);
        }
        else {                  /* granulepos is set, validly. Use it, and force a flush to
                                   account for shortened blocks (vcut) when appropriate */
            if (granpos > op.granulepos) {
                granpos = op.granulepos;
                ogg_stream_packetin(&streamout, &op);
                needflush = true;
            }
            else {
                ogg_stream_packetin(&streamout, &op);
                needout = true;
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

    if (extrapage) {
        if (out.fwrite(ogin.header, 1, ogin.header_len) != ogin.header_len)
            goto cleanup;
        if (out.fwrite(ogin.body, 1, ogin.body_len) != ogin.body_len)
            goto cleanup;
    }

    eosin = false;       /* clear it, because not all paths to here do */
    while (!eosin) {     /* We reached eos, not eof */
        /* We copy the rest of the stream (other logical streams)
         * through, a page at a time. */
        while (1) {
            int result = ogg_sync_pageout(&oy, &ogout);
            if (result == 0)
                break;
            if (result < 0)
                lasterror = "Corrupt or missing data, continuing...";
            else {
                /* Don't bother going through the rest, we can just
                 * write the page out now */
                if (out.fwrite(ogout.header, 1, ogout.header_len) != ogout.header_len)
                    goto cleanup;
                if (out.fwrite(ogout.body, 1, ogout.body_len) != ogout.body_len)
                    goto cleanup;
            }
        }
        char *buffer = ogg_sync_buffer(&oy, CHUNKSIZE);
        int64_t bytes = in.fread(buffer, 1, CHUNKSIZE);
        ogg_sync_wrote(&oy, bytes);
        if (bytes == 0) {
            eosin = true;
            break;
        }
    }

  cleanup:
    ogg_stream_clear(&streamout);
    ogg_packet_clear(&header_comments);

    if (!eosin) {
        lasterror =
            "Error writing stream to output. "
            "Output stream may be corrupted or truncated.";
        return false;
    }

    return true;
}
