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

#include <assert.h>
#include <string.h>
#include "formatter.h"

StringBuf Formatter::format (const char * format) const
{
    const char * p;
    char * q;
    int len;

    for (p = format, len = 0; * p; p ++)
    {
        if (* p == '%')
        {
            if (values[(int) * ++ p])
                len += strlen (values[(int) * p]);
            else if (! * p)
            {
                len += 1;
                p --;
            }
            else
                len += 2;
        }
        else
            len ++;
    }

    StringBuf buffer (len);

    for (p = format, q = buffer; * p; p ++)
    {
        if (* p == '%')
        {
            if (values[(int) * ++ p])
            {
                strcpy (q, values[(int) * p]);
                q += strlen (q);
            }
            else
            {
                * q ++ = '%';
                if (* p != '\0')
                    * q ++ = * p;
                else
                    p --;
            }
        }
        else
            * q ++ = * p;
    }

    // sanity check
    assert (q == buffer + buffer.len ());

    return buffer;
}
