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

#include "alsa-stdinc.h"

static alsaplug_format_mapping_t alsaplug_format_conv_tbl[] = {
    {FMT_FLOAT,  SND_PCM_FORMAT_FLOAT},
    {FMT_S24_LE, SND_PCM_FORMAT_S24_LE},
    {FMT_S24_BE, SND_PCM_FORMAT_S24_BE},
    {FMT_S24_NE, SND_PCM_FORMAT_S24},
    {FMT_U24_LE, SND_PCM_FORMAT_U24_LE},
    {FMT_U24_BE, SND_PCM_FORMAT_U24_BE},
    {FMT_U24_NE, SND_PCM_FORMAT_U24},
    {FMT_S16_LE, SND_PCM_FORMAT_S16_LE},
    {FMT_S16_BE, SND_PCM_FORMAT_S16_BE},
    {FMT_S16_NE, SND_PCM_FORMAT_S16},
    {FMT_U16_LE, SND_PCM_FORMAT_U16_LE},
    {FMT_U16_BE, SND_PCM_FORMAT_U16_BE},
    {FMT_U16_NE, SND_PCM_FORMAT_U16},
    {FMT_U8, SND_PCM_FORMAT_U8},
    {FMT_S8, SND_PCM_FORMAT_S8},
};

snd_pcm_format_t
alsaplug_format_convert(AFormat fmt)
{
    gint i;

    for (i = 0; i < G_N_ELEMENTS(alsaplug_format_conv_tbl); i++)
    {
         if (alsaplug_format_conv_tbl[i].aud_fmt == fmt)
             return alsaplug_format_conv_tbl[i].alsa_fmt;
    }

    return SND_PCM_FORMAT_UNKNOWN;
}
