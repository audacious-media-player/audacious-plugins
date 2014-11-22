/*
 * itunes-cover.c
 * John Lindgren, 2010
 *
 * The author hereby releases this code into the public domain.
 *
 * Reference:
 * http://atomicparsley.sourceforge.net/mpeg-4files.html
 */

#define __STDC_LIMIT_MACROS
#include "ffaudio-stdinc.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const struct {
    const char * id;
    int skip;
} hierarchy[] = {
    {"moov", 0},
    {"udta", 0},
    {"meta", 4},
    {"ilst", 0},
    {"covr", 0},
    {"data", 8}
};

Index<char> read_itunes_cover(const char * filename, VFSFile & file)
{
    unsigned char b[8];
    int bsize;

    Index<char> data;

    /* Check for ftyp frame. */

    if (file.fread (b, 1, 8) != 8)
        return data;
    if ((bsize = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]) < 8)
        return data;
    if (strncmp ((char *) b + 4, "ftyp", 4))
        return data;
    if (file.fseek (bsize - 8, VFS_SEEK_CUR))
        return data;

    int64_t stop = INT64_MAX;
    int64_t at = bsize;

    /* Descend into frame hierarchy. */

    for (auto & frame : hierarchy)
    {
        while (1)
        {
            if (file.fread (b, 1, 8) != 8)
                return data;
            if ((bsize = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]) < 8 || at + bsize > stop)
                return data;
            if (! strncmp ((char *) b + 4, frame.id, 4))
                break;
            if (file.fseek (bsize - 8, VFS_SEEK_CUR))
                return data;

            at += bsize;
        }

        stop = at + bsize;
        at += 8;

        /* Skip leading bytes in some frames. */

        if (frame.skip)
        {
            if (file.fseek (frame.skip, VFS_SEEK_CUR))
                return data;
            at += frame.skip;
        }
    }

    /* We're there. */

    data.insert (0, stop - at);

    if (file.fread (data.begin (), 1, stop - at) != stop - at)
        data.clear ();

    return data;
}
