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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#ifndef _TOOLS_H
#define _TOOLS_H

#include <glib.h>
#include <FLAC/all.h>
#include "flacng.h"
#include "flac_compat.h"
#include "debug.h"

#define INFO_LOCK(__info) \
    do { \
        _DEBUG("Trying to lock info %s", (__info)->name); \
        g_mutex_lock((__info)->mutex); \
        _DEBUG("Locked info %s", (__info)->name); \
    } while(0)

#define INFO_UNLOCK(__info) \
    do { \
        _DEBUG("Unlocking info %s", (__info)->name); \
        g_mutex_unlock((__info)->mutex); \
        _DEBUG("Unlocked info %s", (__info)->name); \
    } while(0)

callback_info* init_callback_info(gchar* name);
void clean_callback_info(callback_info* info);
void reset_info(callback_info* info, gboolean close_fd);
Tuple *get_tuple(VFSFile *fd, callback_info* info);
void add_comment(callback_info* info, gchar* key, gchar* value);
gboolean read_metadata(VFSFile* fd, FLAC__StreamDecoder* decoder, callback_info* info);
ReplayGainInfo get_replay_gain(callback_info *info);

#endif
