/*
 * base64.c
 * Copyright 2011 John Lindgren
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

#include <stdio.h>
#include <string.h>
#include <glib.h>

static gint unalpha (gchar c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return 26 + c - 'a';
    if (c >= '0' && c <= '9')
        return 52 + c - '0';
    if (c == '+')
        return 62;
    if (c == '/')
        return 63;

    return -1;
}

gboolean base64_decode (const gchar * text, void * * data, gint * length)
{
    gint length_estimate = (strlen (text) * 3 + 3) / 4;
    guchar * buf = g_malloc (length_estimate);
    memset (buf, 0, length_estimate);

    const gchar * get = text;
    guchar * set = buf;
    gint x, a;

    while (1)
    {
        x = 0;

        if ((a = unalpha (* get)) < 0)
            break;
        x |= a << 18;
        get ++;

        if ((a = unalpha (* get)) < 0)
            break;
        x |= a << 12;
        get ++;

        if ((a = unalpha (* get)) < 0)
            break;
        x |= a << 6;
        get ++;

        if ((a = unalpha (* get)) < 0)
            break;
        x |= a;
        get ++;

        * set ++ = x >> 16;
        * set ++ = x >> 8;
        * set ++ = x;
    }

    if (! strcmp (get, "")) /* no bytes remaining */
    {
        if (x)
            goto ERR;
    }
    else if (! strcmp (get, "==")) /* one byte remaining */
    {
        if (x & ~0xff0000)
            goto ERR;

        * set ++ = x >> 16;
    }
    else if (! strcmp (get, "=")) /* two bytes remaining */
    {
        if (x & ~0xffff00)
            goto ERR;

        * set ++ = x >> 16;
        * set ++ = x >> 8;
    }
    else
        goto ERR;

    * data = buf;
    * length = set - buf;
    return TRUE;

ERR:
    fprintf (stderr, "base64 decoding error at byte %d.\n", (int) (get - text));
    g_free (buf);
    return FALSE;
}
