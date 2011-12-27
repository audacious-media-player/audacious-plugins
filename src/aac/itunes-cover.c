/*
 * itunes-cover.c
 * John Lindgren, 2010
 *
 * The author hereby releases this code into the public domain.
 *
 * Reference:
 * http://atomicparsley.sourceforge.net/mpeg-4files.html
 */

#include <glib.h>
#include <string.h>
#include <audacious/debug.h>
#include <libaudcore/vfs.h>

static const gchar * const hier[] = {"moov", "udta", "meta", "ilst", "covr",
 "data"};
static const gint skip[] = {0, 0, 4, 0, 0, 8};

gboolean read_itunes_cover (const gchar * filename, VFSFile * file, void * *
 data, gint64 * size)
{
    guchar b[8];
    gint bsize;

    /* Check for ftyp frame. */

    if (vfs_fread (b, 1, 8, file) != 8)
        return FALSE;
    if ((bsize = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]) < 8)
        return FALSE;
    if (strncmp ((gchar *) b + 4, "ftyp", 4))
        return FALSE;
    if (vfs_fseek (file, bsize - 8, SEEK_CUR))
        return FALSE;

    AUDDBG ("Found ftyp frame, size = %d.\n", bsize);

    gint64 stop = G_MAXINT64;
    gint64 at = bsize;

    /* Descend into frame hierarchy. */

    for (gint h = 0; h < G_N_ELEMENTS (hier); h ++)
    {
        while (1)
        {
            if (vfs_fread (b, 1, 8, file) != 8)
                return FALSE;
            if ((bsize = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]) < 8
             || at + bsize > stop)
                return FALSE;
            if (! strncmp ((gchar *) b + 4, hier[h], 4))
                break;
            if (vfs_fseek (file, bsize - 8, SEEK_CUR))
                return FALSE;

            at += bsize;
        }

        AUDDBG ("Found %s frame at %d, size = %d.\n", hier[h], (gint) at, bsize);

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

    * size = stop - at;
    * data = g_malloc (stop - at);

    if (vfs_fread (* data, 1, stop - at, file) != stop - at)
    {
        g_free (* data);
        return FALSE;
    }

    return TRUE;
}
