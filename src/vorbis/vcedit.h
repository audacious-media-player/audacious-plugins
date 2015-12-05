/* This program is licensed under the GNU Library General Public License, version 2,
 * a copy of which is included with this program (with filename LICENSE.LGPL).
 *
 * (c) 2000-2001 Michael Smith <msmith@labyrinth.net.au>
 *
 * VCEdit header.
 *
 */

#ifndef __VCEDIT_H
#define __VCEDIT_H

#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <libaudcore/vfs.h>

struct vcedit_state {
    ogg_sync_state   oy;
    ogg_stream_state os;
    vorbis_comment   vc;
    vorbis_info      vi;

    long        serial = 0;
    const char *lasterror = nullptr;
    int         prevW = 0;
    bool        extrapage = false;
    bool        eosin = false;

    String vendor;

    Index<unsigned char> mainbuf;
    Index<unsigned char> bookbuf;

    vcedit_state()
    {
        ogg_sync_init(&oy);
        ogg_stream_init(&os, 0);
        vorbis_comment_init(&vc);
        vorbis_info_init(&vi);
    }

    ~vcedit_state()
    {
        ogg_sync_clear(&oy);
        ogg_stream_clear(&os);
        vorbis_comment_clear(&vc);
        vorbis_info_clear(&vi);
    }
};

extern bool vcedit_open(vcedit_state *state, VFSFile &in);
extern bool vcedit_write(vcedit_state *state, VFSFile &in, VFSFile &out);

#endif /* __VCEDIT_H */

