/** @file daap_md5.h
 *
 *  Header for DAAP (iTunes Music Sharing) hashing, connection
 *  Slightly modified for use in XMMS2
 *
 *  Copyright (C) 2004,2005 Charles Schmidt <cschmidt2@emich.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef DAAP_MD5_H
#define DAAP_MD5_H

#include <glib.h>

void
daap_hash_generate (short version_major,
                    const guchar *url,
                    guchar hash_select,
                    guchar *out,
                    gint request_id);

#endif

