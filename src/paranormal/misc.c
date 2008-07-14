/*
 * paranormal: iterated pipeline-driven visualization plugin
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

#include "paranormal.h"
#include "actuators.h"
#include "pn_utils.h"

/* ******************** misc_floater ******************** */
static struct pn_actuator_option_desc misc_floater_opts[] =
{
  { "value", "The colour value for the floater.",
    OPT_TYPE_INT, { ival: 255 } },
  { NULL }
};

typedef enum
{
  float_up    = 0x1,
  float_down  = 0x2,
  float_left  = 0x4,
  float_right = 0x8,
} FloaterDirection;

struct floater_state_data
{
  FloaterDirection dir;
  gint x;
  gint y;
};

static void
misc_floater_init(gpointer *data)
{
  struct floater_state_data *opaque_data = g_new0(struct floater_state_data, 1);
  *data = opaque_data;
  opaque_data->x = rand() % pn_image_data->width;
  opaque_data->y = rand() % pn_image_data->height;
  opaque_data->dir = (FloaterDirection) rand() % 15; /* sum of all dir values */
}

static void
misc_floater_cleanup(gpointer data)
{
  g_free(data);
}

/*
 * This implementation isn't very great.
 * Anyone want to improve it? :(
 */
static void
misc_floater_exec(const struct pn_actuator_option *opts, gpointer data)
{
  struct floater_state_data *opaque_data = (struct floater_state_data *) data;
  guchar value = (opts[0].val.ival < 0 || opts[0].val.ival > 255) ? 255 : opts[0].val.ival;

  /* determine the root coordinate first */
  if (opaque_data->dir & float_up)
    opaque_data->y -= 1;
  if (opaque_data->dir & float_down)
    opaque_data->y += 1;

  if (opaque_data->dir & float_left)
    opaque_data->x -= 1;
  if (opaque_data->dir & float_right)
    opaque_data->x += 1;

  /* make sure we're within surface boundaries. segfaults suck, afterall. */
  if (opaque_data->x + 1 <= pn_image_data->width &&
       opaque_data->x - 1 >= 0 &&
       opaque_data->y + 1 <= pn_image_data->height &&
       opaque_data->y - 1 >= 0)
    {
      /* draw it. i could use a loop here, but i don't see much reason in it,
       * so i don't think i will at this time.  -nenolod
       */
      pn_image_data->surface[0][PN_IMG_INDEX(opaque_data->x, opaque_data->y)]     = value;
      pn_image_data->surface[0][PN_IMG_INDEX(opaque_data->x + 1, opaque_data->y)] = value;
      pn_image_data->surface[0][PN_IMG_INDEX(opaque_data->x - 1, opaque_data->y)] = value;
      pn_image_data->surface[0][PN_IMG_INDEX(opaque_data->x, opaque_data->y + 1)] = value;
      pn_image_data->surface[0][PN_IMG_INDEX(opaque_data->x, opaque_data->y - 1)] = value;
    }

  /* check if we need to change direction yet, and if so, do so. */
  if (pn_new_beat == TRUE)
    opaque_data->dir = (FloaterDirection) rand() % 15; /* sum of all dir values */

  /* now adjust the direction so we stay in boundary */
  if (opaque_data->x - 1 <= 0 && opaque_data->dir & float_left)
    {
      opaque_data->dir &= ~float_left;
      opaque_data->dir |= float_right;
    }

  if (opaque_data->x + 1 >= pn_image_data->width && opaque_data->dir & float_right)
    {
      opaque_data->dir &= ~float_right;
      opaque_data->dir |= float_left;
    }

  if (opaque_data->y - 1 <= 0 && opaque_data->dir & float_up)
    {
      opaque_data->dir &= ~float_up;
      opaque_data->dir |= float_down;
    }

  if (opaque_data->y + 1 >= pn_image_data->height && opaque_data->dir & float_down)
    {
      opaque_data->dir &= ~float_down;
      opaque_data->dir |= float_up;
    }
}

struct pn_actuator_desc builtin_misc_floater =
{
  "misc_floater",
  "Floating Particle",
  "A floating particle.",
  0, misc_floater_opts,
  misc_floater_init, misc_floater_cleanup, misc_floater_exec
};

