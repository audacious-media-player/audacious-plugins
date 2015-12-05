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

class VCEdit
{
public:
    vorbis_comment vc;
    const char *lasterror = nullptr;

    VCEdit();
    ~VCEdit();

    VCEdit(const VCEdit&) = delete;
    VCEdit& operator=(const VCEdit&) = delete;

    bool open(VFSFile &in);
    bool write(VFSFile &in, VFSFile &out);

private:
    ogg_sync_state   oy;
    ogg_stream_state os;
    vorbis_info      vi;

    long serial = 0;
    int  prevW = 0;
    bool extrapage = false;
    bool eosin = false;

    String vendor;

    Index<unsigned char> mainbuf;
    Index<unsigned char> bookbuf;

    int blocksize(ogg_packet *p);
    bool fetch_next_packet(VFSFile &in, ogg_packet *p, ogg_page *page);
};

#endif /* __VCEDIT_H */

