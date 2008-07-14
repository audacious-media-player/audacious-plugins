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

/* FIXME: what to name this file? */

#include <config.h>

#include "paranormal.h"
#include "actuators.h"
#include "pn_utils.h"
#include "libcalc/calc.h"

/* **************** general_fade **************** */
static struct pn_actuator_option_desc general_fade_opts[] =
{
  { "amount", "The amount by which the color index of each "
    "pixel should be decreased by each frame (MAX 255)",
    OPT_TYPE_INT, { ival: 3 } },
  { NULL }
};

static void
general_fade_exec (const struct pn_actuator_option *opts,
	   gpointer data)
{
  int amt = opts[0].val.ival > 255 || opts[0].val.ival < 0 ? 3 : opts[0].val.ival;
  int i, j;

  for (j=0; j<pn_image_data->height; j++)
    for (i=0; i<pn_image_data->width; i++)
      pn_image_data->surface[0][PN_IMG_INDEX (i, j)] =
	CAPLO (pn_image_data->surface[0][PN_IMG_INDEX (i, j)]
	       - amt, 0);
}

struct pn_actuator_desc builtin_general_fade =
{
  "general_fade", "Fade-out", "Decreases the color index of each pixel",
  0, general_fade_opts,
  NULL, NULL, general_fade_exec
};

/* **************** general_blur **************** */
/* FIXME: add a variable radius */
/* FIXME: SPEEEED */
static void
general_blur_exec (const struct pn_actuator_option *opts,
	   gpointer data)
{
  int i,j;
  register guchar *srcptr = pn_image_data->surface[0];
  register guchar *destptr = pn_image_data->surface[1];
  register int sum;

  for (j=0; j<pn_image_data->height; j++)
    for (i=0; i<pn_image_data->width; i++)
      {
	sum = *(srcptr)<<2;

	/* top */
	if (j > 0)
	  {
	    sum += *(srcptr-pn_image_data->width)<<1;
	    if (i > 0)
	      sum += *(srcptr-pn_image_data->width-1);
	    if (i < pn_image_data->width-1)
	      sum += *(srcptr-pn_image_data->width+1);
	  }
	/* bottom */
	if (j < pn_image_data->height-1)
	  {
	    sum += *(srcptr+pn_image_data->width)<<1;
	    if (i > 0)
	      sum += *(srcptr+pn_image_data->width-1);
	    if (i < pn_image_data->width-1)
	      sum += *(srcptr+pn_image_data->width+1);
	  }
	/* left */
	if (i > 0)
	  sum += *(srcptr-1)<<1;
	/* right */
	if (i < pn_image_data->width-1)
	  sum += *(srcptr+1)<<1;

	*destptr++ = (guchar)(sum >> 4);
	srcptr++;
      }

  pn_swap_surfaces ();
}

struct pn_actuator_desc builtin_general_blur = 
{
  "general_blur", "Blur", "A simple 1 pixel radius blur",
  0, NULL,
  NULL, NULL, general_blur_exec
};

/* **************** general_mosaic **************** */
/* FIXME: add a variable radius */
/* FIXME: SPEEEED */
static struct pn_actuator_option_desc general_mosaic_opts[] =
{
  { "radius", "The pixel radius that should be used for the effect.",
    OPT_TYPE_INT, { ival: 6 } },
  { NULL }
};

static void
general_mosaic_exec (const struct pn_actuator_option *opts,
	   gpointer data)
{
  int i,j;
  register guchar *srcptr = pn_image_data->surface[0];
  register guchar *destptr = pn_image_data->surface[1];
  int radius = opts[0].val.ival > 255 || opts[0].val.ival < 0 ? 6 : opts[0].val.ival;

  for (j=0; j<pn_image_data->height; j += radius)
    for (i=0; i<pn_image_data->width; i += radius)
      {
        int ii = 0, jj = 0;
        guchar bval = 0;

        /* find the brightest colour */
        for (jj = 0; jj < radius && (j + jj < pn_image_data->height); jj++)
          for (ii = 0; ii < radius && (i + ii < pn_image_data->width); ii++)
            {
               guchar val = srcptr[PN_IMG_INDEX(i + ii,  j + jj)];

               if (val > bval)
                 bval = val;
            }

        for (jj = 0; jj < radius && (j + jj < pn_image_data->height); jj++)
          for (ii = 0; ii < radius && (i + ii < pn_image_data->width); ii++)
            {
               destptr[PN_IMG_INDEX(i + ii, j + jj)] = bval;
            }
      }

  pn_swap_surfaces ();
}

struct pn_actuator_desc builtin_general_mosaic = 
{
  "general_mosaic", "Mosaic", "A simple mosaic effect.",
  0, general_mosaic_opts,
  NULL, NULL, general_mosaic_exec
};

/* **************** general_clear **************** */
static void
general_clear_exec (const struct pn_actuator_option *opts,
	   gpointer data)
{
   memset(pn_image_data->surface[0], '\0',
	  (pn_image_data->height * pn_image_data->width));
}

struct pn_actuator_desc builtin_general_clear =
{
  "general_clear", "Clear Surface", "Clears the surface.",
  0, NULL,
  NULL, NULL, general_clear_exec
};

/* **************** general_noop **************** */
static void
general_noop_exec (const struct pn_actuator_option *opts,
	   gpointer data)
{
  return;
}

struct pn_actuator_desc builtin_general_noop =
{
  "general_noop", "Do Nothing", "Does absolutely nothing.",
  0, NULL,
  NULL, NULL, general_noop_exec
};

/* **************** general_invert **************** */
static void
general_invert_exec (const struct pn_actuator_option *opts,
	   gpointer data)
{
  int i, j;

  for (j=0; j < pn_image_data->height; j++)
    for (i=0; i < pn_image_data->width; i++)
      pn_image_data->surface[0][PN_IMG_INDEX (i, j)] =
	255 - pn_image_data->surface[0][PN_IMG_INDEX (i, j)];
}

struct pn_actuator_desc builtin_general_invert =
{
  "general_invert", "Value Invert", "Performs a value invert.",
  0, NULL,
  NULL, NULL, general_invert_exec
};

/* **************** general_replace **************** */
static struct pn_actuator_option_desc general_replace_opts[] =
{
  { "start", "The beginning colour value that should be replaced by the value of out.",
    OPT_TYPE_INT, { ival: 250 } },
  { "end", "The ending colour value that should be replaced by the value of out.",
    OPT_TYPE_INT, { ival: 255 } },
  { "out", "The colour value that in is replaced with.",
    OPT_TYPE_INT, { ival: 0 } },
  { NULL }
};

static void
general_replace_exec (const struct pn_actuator_option *opts,
	   gpointer data)
{
  register int i, j;
  register guchar val;
  guchar begin = opts[0].val.ival > 255 || opts[0].val.ival < 0 ? 250 : opts[0].val.ival;
  guchar end = opts[1].val.ival > 255 || opts[1].val.ival < 0 ? 255 : opts[1].val.ival;
  guchar out = opts[2].val.ival > 255 || opts[2].val.ival < 0 ? 0 : opts[2].val.ival;

  for (j=0; j < pn_image_data->height; j++)
    for (i=0; i < pn_image_data->width; i++)
      {
        val = pn_image_data->surface[0][PN_IMG_INDEX (i, j)];
        if (val >= begin && val <= end)
          pn_image_data->surface[0][PN_IMG_INDEX (i, j)] = out;
      }
}

struct pn_actuator_desc builtin_general_replace =
{
  "general_replace", "Value Replace", "Performs a value replace on a range of values.",
  0, general_replace_opts,
  NULL, NULL, general_replace_exec
};

/* **************** general_swap **************** */
static void
general_swap_exec (const struct pn_actuator_option *opts,
	   gpointer data)
{
  pn_swap_surfaces ();
}

struct pn_actuator_desc builtin_general_swap =
{
  "general_swap", "Swap Surface", "Swaps the surface.",
  0, NULL,
  NULL, NULL, general_swap_exec
};

/* **************** general_copy **************** */
static void
general_copy_exec (const struct pn_actuator_option *opts,
	   gpointer data)
{
  memcpy(pn_image_data->surface[1], pn_image_data->surface[0], 
         (pn_image_data->width * pn_image_data->height));
}

struct pn_actuator_desc builtin_general_copy =
{
  "general_copy", "Copy Surface", "Copies the surface to the other surface.",
  0, NULL,
  NULL, NULL, general_copy_exec
};

/* **************** general_flip **************** */
static struct pn_actuator_option_desc general_flip_opts[] =
{
  { "direction", "Negative is horizontal, positive is vertical.",
    OPT_TYPE_INT, { ival: -1 } },
  { NULL }
};

static void
general_flip_exec (const struct pn_actuator_option *opts,
	   gpointer data)
{
  gint x, y;

  if (opts[0].val.ival < 0)
    {
      for (y = 0; y < pn_image_data->height; y++)
        for (x = 0; x < pn_image_data->width; x++)
          {
             pn_image_data->surface[1][PN_IMG_INDEX(pn_image_data->width - x, y)] = 
               pn_image_data->surface[0][PN_IMG_INDEX(x, y)];
          }
    }
  else
    {
      for (y = 0; y < pn_image_data->height; y++)
        for (x = 0; x < pn_image_data->width; x++)
          {
             pn_image_data->surface[1][PN_IMG_INDEX(x, pn_image_data->height - y)] = 
               pn_image_data->surface[0][PN_IMG_INDEX(x, y)];
          }
    }

    pn_swap_surfaces ();
}

struct pn_actuator_desc builtin_general_flip =
{
  "general_flip", "Flip Surface", "Flips the surface.",
  0, general_flip_opts,
  NULL, NULL, general_flip_exec
};

/* ***************** general_evaluate ***************** */

static struct pn_actuator_option_desc general_evaluate_opts[] =
{
  { "init_script", "Script to run on start.", OPT_TYPE_STRING, {sval: "global_reg0 = 27;"} },
  { "frame_script", "Script to run.", OPT_TYPE_STRING, {sval: "global_reg0 = global_reg0 + 1;"} },
  { NULL }
};

struct pn_evaluate_ctx
{
  expression_t *expr_on_init, *expr_on_frame;
  symbol_dict_t *dict;
  gboolean reset;
};

static void
general_evaluate_init(gpointer *data)
{
  *data = g_new0(struct pn_evaluate_ctx, 1);

  ((struct pn_evaluate_ctx *)*data)->reset = TRUE;
}

static void
general_evaluate_cleanup(gpointer op_data)
{
  struct pn_evaluate_ctx *data = (struct pn_evaluate_ctx *) op_data;

  g_return_if_fail(data != NULL);

  if (data->expr_on_init)
    expr_free(data->expr_on_init);

  if (data->expr_on_frame)
    expr_free(data->expr_on_frame);

  if (data->dict)
    dict_free(data->dict);

  if (data)
    g_free(data);
}

static void
general_evaluate_exec(const struct pn_actuator_option *opts,
                      gpointer op_data)
{
  struct pn_evaluate_ctx *data = (struct pn_evaluate_ctx *) op_data;

  if (data->reset)
    {
       if (data->dict)
         dict_free(data->dict);

       data->dict = dict_new();

       if (opts[0].val.sval != NULL);
         data->expr_on_init = expr_compile_string(opts[0].val.sval, data->dict);

       if (opts[1].val.sval != NULL);
         data->expr_on_frame = expr_compile_string(opts[1].val.sval, data->dict);

       if (data->expr_on_init != NULL)
         expr_execute(data->expr_on_init, data->dict);

       data->reset = FALSE;
    }

  if (data->expr_on_frame != NULL)
    expr_execute(data->expr_on_frame, data->dict);
}

struct pn_actuator_desc builtin_general_evaluate =
{
  "general_evaluate", "Evalulate VM Code",
  "Evaluates arbitrary VM code. Does not draw anything.",
  0, general_evaluate_opts,
  general_evaluate_init, general_evaluate_cleanup, general_evaluate_exec
};

