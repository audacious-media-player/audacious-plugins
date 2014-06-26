/*  Audacious
 *  Copyright (C) 2005-2007  Audacious team
 *
 *  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2003  Peter Alm, Mikael Alm, Olle Hallnas,
 *                           Thomas Nilsson and 4Front Technologies
 *  Copyright (C) 1999-2003  Haavard Kvaalen
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#include <glib.h>
#include <string.h>
#include "formatter.h"

Formatter *
formatter_new(void)
{
    Formatter *formatter = g_slice_new0(Formatter);

    formatter_associate(formatter, '%', "%");
    return formatter;
}

void
formatter_destroy(Formatter * formatter)
{
    int i;

    for (i = 0; i < 256; i++)
        if (formatter->values[i])
            g_free(formatter->values[i]);

    g_slice_free(Formatter, formatter);
}

void
formatter_associate(Formatter * formatter, const unsigned char id, const char *value)
{
    formatter_dissociate(formatter, id);
    formatter->values[id] = g_strdup(value);
}

void
formatter_dissociate(Formatter * formatter, const unsigned char id)
{
    if (formatter->values[id])
        g_free(formatter->values[id]);
    formatter->values[id] = 0;
}

char *
formatter_format(Formatter * formatter, const char *format)
{
    const char *p;
    char *q, *buffer;
    int len;

    for (p = format, len = 0; *p; p++)
        if (*p == '%') {
            if (formatter->values[(int) *++p])
                len += strlen(formatter->values[(int) *p]);
            else if (!*p) {
                len += 1;
                p--;
            }
            else
                len += 2;
        }
        else
            len++;
    buffer = g_new(char, len + 1);
    for (p = format, q = buffer; *p; p++)
        if (*p == '%') {
            if (formatter->values[(int) *++p]) {
                g_strlcpy(q, formatter->values[(int) *p], len - 1);
                q += strlen(q);
            }
            else {
                *q++ = '%';
                if (*p != '\0')
                    *q++ = *p;
                else
                    p--;
            }
        }
        else
            *q++ = *p;
    *q = 0;
    return buffer;
}
