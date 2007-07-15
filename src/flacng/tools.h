/*
 *  A FLAC decoder plugin for the Audacious Media Player
 *  Copyright (C) 2005 Ralf Ertzinger
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
 */
#ifndef _TOOLS_H
#define _TOOLS_H

#include <glib.h>
#include <FLAC/all.h>
#include "flacng.h"
#include "flac_compat.h"

callback_info* init_callback_info(gchar* name);
void reset_info(callback_info* info, gboolean close_fd);
gchar* get_title(const gchar* filename, callback_info* info);
TitleInput *get_tuple(const gchar *filename, callback_info* info);
void add_comment(callback_info* info, gchar* key, gchar* value);
gboolean read_metadata(VFSFile* fd, FLAC__StreamDecoder* decoder, callback_info* info);

#endif
