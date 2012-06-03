/*
 * itunes-cover.c
 * John Lindgren, 2010
 *
 * The author hereby releases this code into the public domain.
 *
 * Reference:
 * http://atomicparsley.sourceforge.net/mpeg-4files.html
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libaudcore/vfs.h>

static const char * const hier[] = {"moov", "udta", "meta", "ilst", "covr", "data"};
static const int skip[] = {0, 0, 4, 0, 0, 8};

bool_t read_itunes_cover (const char * filename, VFSFile * file, void * *
 data, int64_t * size)
{
    unsigned char b[8];
    int bsize;

    * data = NULL;
    * size = 0;

    /* Check for ftyp frame. */

    if (vfs_fread (b, 1, 8, file) != 8)
        return FALSE;
    if ((bsize = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]) < 8)
        return FALSE;
    if (strncmp ((char *) b + 4, "ftyp", 4))
        return FALSE;
    if (vfs_fseek (file, bsize - 8, SEEK_CUR))
        return FALSE;

    int64_t stop = INT64_MAX;
    int64_t at = bsize;

    /* Descend into frame hierarchy. */

    for (int h = 0; h < sizeof hier / sizeof hier[0]; h ++)
    {
        while (1)
        {
            if (vfs_fread (b, 1, 8, file) != 8)
                return FALSE;
            if ((bsize = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]) < 8
             || at + bsize > stop)
                return FALSE;
            if (! strncmp ((char *) b + 4, hier[h], 4))
                break;
            if (vfs_fseek (file, bsize - 8, SEEK_CUR))
                return FALSE;

            at += bsize;
        }

        stop = at + bsize;
        at += 8;

        /* Skip leading bytes in some frames. */

        if (skip[h])
        {
            if (vfs_fseek (file, skip[h], SEEK_CUR))
                return FALSE;
            at += skip[h];
        }
    }

    /* We're there. */

    * data = malloc (stop - at);
    * size = stop - at;

    if (vfs_fread (* data, 1, stop - at, file) != stop - at)
    {
        free (* data);
        * data = NULL;
        * size = 0;
        return FALSE;
    }

    return TRUE;
}
