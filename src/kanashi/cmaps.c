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

#include <glib.h>

#include "kanashi.h"

static struct kanashi_color black = {0, 0, 0};
static struct kanashi_color white = {255, 255, 255};

/* **************** cmap generation funcs **************** */
static void
cmap_gen_gradient (int step, const struct kanashi_color *a,
		   const struct kanashi_color *b,
		   struct kanashi_color *c)
{
  c->r = a->r + step * ((((float)b->r) - ((float)a->r)) / 256.0);
  c->g = a->g + step * ((((float)b->g) - ((float)a->g)) / 256.0);
  c->b = a->b + step * ((((float)b->b) - ((float)a->b)) / 256.0);
}

/* **************** cmap_gradient **************** */
#if 0
static void
cmap_gradient_exec (const struct kanashi_actuator_option *opts,
		    gpointer data)
{
  int i;

  for (i=opts[0].val.ival; i<=opts[1].val.ival; i++)
    cmap_gen_gradient (((i-opts[0].val.ival)<<8)/(opts[1].val.ival
						  - opts[0].val.ival),
		       &opts[2].val.cval, &opts[3].val.cval,
		       &kanashi_image_data->cmap[i]);
}
#endif

/* **************** cmap_bwgradient **************** */
void
kanashi_set_colormap_gradient(gint low_index, gint high_index, struct kanashi_color *color)
{
  int i;

  for (i=low_index; i < 128 && i <= high_index; i++)
    cmap_gen_gradient (i<<1, &black, color,
		       &kanashi_image_data->cmap[i]);

  for (i=128; i < 256 && i <= high_index; i++)
    cmap_gen_gradient ((i-128)<<1, color, &white,
		       &kanashi_image_data->cmap[i]);
}
    
