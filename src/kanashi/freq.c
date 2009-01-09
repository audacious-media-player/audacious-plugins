/*
 * kanashi: iterated javascript-driven visualization framework
 * Copyright (c) 2006, 2007 William Pitcock <nenolod@dereferenced.org>
 * Portions copyright (c) 2001 Jamie Gennis <jgennis@mindspring.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>

#include <glib.h>

#include "kanashi.h"
#include "kanashi_utils.h"

/* **************** freq_dots **************** */
/* FIXME: take this piece of crap out */
static void
kanashi_freq_dots(void)
{
  int i, basex;

  basex = (kanashi_image_data->width>>1)-128;
  for (i=basex < 0 ? -basex : 0 ; i < 256; i++)
    {
      kanashi_image_data->surface[0][PN_IMG_INDEX (basex+i, (kanashi_image_data->height>>1)
					      - CAP (kanashi_sound_data->freq_data[0][i], 120))]
	= 0xff;
      kanashi_image_data->surface[0][PN_IMG_INDEX (basex+256-i, (kanashi_image_data->height>>1)
					      + CAP (kanashi_sound_data->freq_data[1][i], 120))]
	= 0xff;
    }
}

/* **************** freq_drops **************** */
static void
kanashi_freq_drops (void)
{
  int i,j;
  
  for (i=0; i<256; i++)
    for (j=0; j<kanashi_sound_data->freq_data[0][i]>>3; i++)
      kanashi_image_data->surface[0][PN_IMG_INDEX (rand() % kanashi_image_data->width,
					      rand() % kanashi_image_data->height)]
	= 0xff;
}
