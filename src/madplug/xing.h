/*
 * mad - MPEG audio decoder
 * Copyright (C) 2000-2001 Robert Leslie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef XING_H
#define XING_H

#include <glib.h>

struct xing
{
    guint flags;        /* valid fields (see below) */
    guint frames;       /* total number of frames */
    guint bytes;        /* total number of bytes */
    guchar toc[100];    /* 100-point seek table */
    gulong scale;       /* ?? */
};

enum
{
    XING_FRAMES = 0x00000001L,
    XING_BYTES = 0x00000002L,
    XING_TOC = 0x00000004L,
    XING_SCALE = 0x00000008L
};

void xing_init(struct xing *);

#define xing_finish(xing)       /* nothing */

int xing_parse(struct xing *, struct mad_bitptr, unsigned int);

#endif
