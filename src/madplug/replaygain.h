/*
 * mad plugin for audacious
 * Copyright (C) 2005-2007 William Pitcock, Yoshiki Yazawa
 *
 * Portions derived from xmms-mad:
 * Copyright (C) 2001-2002 Sam Clegg - See COPYING
 * Copyright (C) 2001-2007 Samuel Krempp
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "plugin.h"

#ifndef __replaygain_h__
#define __replaygain_h__

struct APETagFooterStruct
{
    unsigned char ID[8];
    unsigned char Version[4];
    unsigned char Length[4];
    unsigned char TagCount[4];
    unsigned char Flags[4];
    unsigned char Reserved[8];
};

/* prototypes */
void read_replaygain(struct mad_info_t *file_info);

#endif
