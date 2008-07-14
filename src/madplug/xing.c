/*
 * mad plugin for audacious
 * Copyright (C) 2005-2007 William Pitcock, Yoshiki Yazawa
 *
 * Derived from: mad - MPEG audio decoder
 * Copyright (C) 2000-2001 Robert Leslie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <mad.h>
#include "xing.h"

#define XING_MAGIC	(('X' << 24) | ('i' << 16) | ('n' << 8) | 'g')

/*
 * NAME:	xing->init()
 * DESCRIPTION:	initialize Xing structure
 */
void
xing_init(struct xing *xing)
{
    xing->flags = 0;
    xing->frames = 0;
    xing->bytes = 0;
    xing->toc[0] = '\0';
    xing->scale = 0;
}

/*
 * NAME:	xing->parse()
 * DESCRIPTION:	parse a Xing VBR header
 */
int
xing_parse(struct xing *xing, struct mad_bitptr ptr, guint bitlen)
{
    if (bitlen < 64 || mad_bit_read(&ptr, 32) != XING_MAGIC)
        goto fail;

    xing->flags = mad_bit_read(&ptr, 32);
    bitlen -= 64;

    if (xing->flags & XING_FRAMES) {
        if (bitlen < 32)
            goto fail;

        xing->frames = mad_bit_read(&ptr, 32);
        bitlen -= 32;
    }

    if (xing->flags & XING_BYTES) {
        if (bitlen < 32)
            goto fail;

        xing->bytes = mad_bit_read(&ptr, 32);
        bitlen -= 32;
    }

    if (xing->flags & XING_TOC) {
        gint i;

        if (bitlen < 800)
            goto fail;

        for (i = 0; i < 100; ++i)
            xing->toc[i] = mad_bit_read(&ptr, 8);
        
        bitlen -= 800;
    }

    if (xing->flags & XING_SCALE) {
        if (bitlen < 32)
            goto fail;

        xing->scale = mad_bit_read(&ptr, 32);
        bitlen -= 32;
    }

    return 0;

  fail:
    xing->flags = 0;
    return -1;
}
