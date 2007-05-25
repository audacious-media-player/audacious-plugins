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

#include "OSS.h"

#include <glib.h>
#include <audacious/i18n.h>
#include <stdlib.h>

OutputPlugin oss_op = {
    NULL,
    NULL,
    "OSS Output Plugin",                       /* Description */
    oss_init,
    oss_cleanup,
    oss_about,
    oss_configure,
    oss_get_volume,
    oss_set_volume,
    oss_open,
    oss_write,
    oss_close,
    oss_flush,
    oss_pause,
    oss_free,
    oss_playing,
    oss_get_output_time,
    oss_get_written_time,
    oss_tell
};

OutputPlugin *oss_oplist[] = { &oss_op, NULL };

DECLARE_PLUGIN(OSS, NULL, NULL, NULL, oss_oplist, NULL, NULL, NULL);

void oss_cleanup(void)
{
    if (oss_cfg.alt_audio_device) {
        free(oss_cfg.alt_audio_device);
        oss_cfg.alt_audio_device = NULL;
    }

    if (oss_cfg.alt_mixer_device) {
        free(oss_cfg.alt_mixer_device);
        oss_cfg.alt_mixer_device = NULL;
    }
}
