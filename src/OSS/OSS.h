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

#ifndef OSS_H
#define OSS_H

#include "config.h"

#include <glib.h>

#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#else
#include <soundcard.h>
#endif

#include <audacious/plugin.h>

#define IS_BIG_ENDIAN (G_BYTE_ORDER == G_BIG_ENDIAN)

extern OutputPlugin op;

typedef struct {
    gint audio_device;
    gint mixer_device;
    gint prebuffer;
    gboolean use_master;
    gboolean use_alt_audio_device, use_alt_mixer_device;
    gchar *alt_audio_device, *alt_mixer_device;
} OSSConfig;

extern OSSConfig oss_cfg;

void oss_configure(void);

void oss_get_volume(int *l, int *r);
void oss_set_volume(int l, int r);

int oss_free(void);
void oss_write(void *ptr, int length);
void oss_close(void);
void oss_flush(int time);
void oss_pause (gboolean p);
int oss_open(gint fmt, int rate, int nch);
int oss_get_output_time(void);
int oss_get_written_time(void);
void oss_set_audio_params(void);

#endif
