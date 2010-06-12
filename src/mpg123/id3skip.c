/*
 * id3skip.c
 * Copyright 2010 John Lindgren
 *
 * This file is part of Audacious.
 *
 * Audacious is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2 or version 3 of the License.
 *
 * Audacious is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Audacious. If not, see <http://www.gnu.org/licenses/>.
 *
 * The Audacious team does not consider modular code linking to Audacious or
 * using our public API to be a derived work.
 */

#include <string.h>

#include <glib.h>

#pragma pack(push) /* must be byte-aligned */
#pragma pack(1)
struct id3_header
{
    gchar magic[3];
    guchar version;
    guchar revision;
    guchar flags;
    guint32 size;
};
#pragma pack(pop)

static guint32 unsyncsafe (guint32 x)
{
    return (x & 0x7f) | ((x & 0x7f00) >> 1) | ((x & 0x7f0000) >> 2) | ((x &
     0x7f000000) >> 3);
}

gint id3_header_size (const guchar * data, gint size)
{
    struct id3_header header;

    if (size < sizeof (struct id3_header))
        return 0;

    memcpy (& header, data, sizeof (struct id3_header));

    if (memcmp (header.magic, "ID3", 3))
        return 0;

    return sizeof (struct id3_header) + unsyncsafe (GUINT32_FROM_BE (header.size));
}
