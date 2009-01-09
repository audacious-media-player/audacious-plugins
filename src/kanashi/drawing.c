/*
 * kanashi: iterated javascript-driven visualization engine
 * Copyright (c) 2009 William Pitcock <nenolod@dereferenced.org>
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

#include <math.h>

#include "kanashi.h"
#include "kanashi_utils.h"

void
kanashi_draw_dot (guint x, guint y, guchar value)
{
  if (x > kanashi_image_data->width || x < 0 || y > kanashi_image_data->height || y < 0)
    return;

  kanashi_image_data->surface[0][PN_IMG_INDEX(x, y)] = value;
}

void
kanashi_draw_line (guint _x0, guint _y0, guint _x1, guint _y1, guchar value)
{
  gint x0 = _x0;
  gint y0 = _y0;
  gint x1 = _x1;
  gint y1 = _y1;

  gint dx = x1 - x0;
  gint dy = y1 - y0;

  kanashi_draw_dot(x0, y0, value);

  if (dx != 0)
    {
      gfloat m = (gfloat) dy / (gfloat) dx;
      gfloat b = y0 - m * x0;

      dx = (x1 > x0) ? 1 : - 1;
      while (x0 != x1)
        {
          x0 += dx;
          y0 = m * x0 + b;

          kanashi_draw_dot(x0, y0, value);
        }
    }
}
