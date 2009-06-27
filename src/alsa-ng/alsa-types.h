/*
 * Audacious ALSA Plugin (-ng)
 * Copyright (c) 2009 William Pitcock <nenolod@dereferenced.org>
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

#ifndef _ALSA_TYPES_H_GUARD
#define _ALSA_TYPES_H_GUARD

#include "alsa-stdinc.h"

typedef struct {
    AFormat aud_fmt;
    snd_pcm_format_t alsa_fmt;
} alsaplug_format_mapping_t;

extern snd_pcm_format_t alsaplug_format_convert(AFormat aud_fmt);

typedef struct {
     gchar *pcm_device;
     gchar *mixer_card;
     gchar *mixer_device;
} alsaplug_cfg_t;

extern alsaplug_cfg_t alsaplug_cfg;

#endif
