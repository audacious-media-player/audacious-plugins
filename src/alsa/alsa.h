/*  XMMS - ALSA output plugin
 *  Copyright (C) 2001-2003 Matthieu Sozeau
 *  Copyright (C) 1998-2003  Peter Alm, Mikael Alm, Olle Hallnas,
 *                           Thomas Nilsson and 4Front Technologies
 *  Copyright (C) 1999-2004  Håvard Kvålen
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
#ifndef ALSA_H
#define ALSA_H

#define NDEBUG

#include "config.h"

#include <audacious/plugin.h>
#include <audacious/i18n.h>

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>
#include <alsa/pcm_plugin.h>

#include <gtk/gtk.h>

#define ALSA_CFGID  "ALSA"

extern OutputPlugin op;

struct alsa_config
{
	gchar *pcm_device;
	gint mixer_card;
	gchar *mixer_device;
	gint buffer_time;
	gint period_time;
	gboolean debug;
	struct
	{
		gint left, right;
	} vol;
};

extern struct alsa_config alsa_cfg;

void alsa_about(void);
void alsa_configure(void);
gint alsa_get_mixer(snd_mixer_t **mixer, gint card);
void alsa_save_config(void);

void alsa_get_volume(gint *l, gint *r);
void alsa_set_volume(gint l, gint r);

gint alsa_playing(void);
gint alsa_free(void);
void alsa_write(void *ptr, gint length);
void alsa_close(void);
void alsa_flush(gint time);
void alsa_pause(gshort p);
gint alsa_open(AFormat fmt, gint rate, gint nch);
gint alsa_get_output_time(void);
gint alsa_get_written_time(void);
void alsa_tell(AFormat * fmt, gint * rate, gint * nch);

extern GStaticMutex alsa_mutex;

#endif
