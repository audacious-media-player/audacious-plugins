/*  BMP - Cross-platform multimedia player
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
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

#include "OSS4.h"

#include <glib.h>
#include <audacious/i18n.h>
#include <stdlib.h>
#include <audacious/configdb.h>

OutputPlugin oss_op = {
    .description = "OSS4 Output Plugin",                      /* Description */
    .init = oss_init,
    .cleanup = oss_cleanup,
    .about = oss_about,
    .configure = oss_configure,
    .get_volume = oss_get_volume,
    .set_volume = oss_set_volume,
    .open_audio = oss_open,
    .write_audio = oss_write,
    .close_audio = oss_close,
    .flush = oss_flush,
    .pause = oss_pause,
    .buffer_free = oss_free,
    .buffer_playing = oss_playing,
    .output_time = oss_get_output_time,
    .written_time = oss_get_written_time,
    .tell_audio = oss_tell
};

OutputPlugin *oss_oplist[] = { &oss_op, NULL };

DECLARE_PLUGIN(OSS4, NULL, NULL, NULL, oss_oplist, NULL, NULL, NULL, NULL);

void oss_cleanup(void)
{
    ConfigDb *db;
    db = bmp_cfg_db_open();
    bmp_cfg_db_set_int(db, "OSS", "saved_volume", vol);
    bmp_cfg_db_close(db);

    if (oss_cfg.alt_audio_device) {
        free(oss_cfg.alt_audio_device);
        oss_cfg.alt_audio_device = NULL;
    }
   
}
