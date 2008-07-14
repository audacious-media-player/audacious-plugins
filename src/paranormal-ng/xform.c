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

/* FIXME: allow for only using an xform on part of the img? */
/* FIXME: perhaps combine these into a single vector field
   so that only 1 apply_xform needs to be done for as many
   of these as someone wants to use */

#include <config.h>

#include <math.h>

#include <glib.h>

#include "paranormal.h"
#include "actuators.h"
#include "pn_utils.h"

#include "libcalc/calc.h"

/* **************** xform_adjust **************** */
struct pn_actuator_option_desc xform_adjust_opts[] =
{
  { "x", "adjustment to x", OPT_TYPE_FLOAT, { fval: 1.0 } },
  { "y", "adjustment to y", OPT_TYPE_FLOAT, { fval: 1.0 } },
  { "z", "adjustment to z", OPT_TYPE_FLOAT, { fval: 1.0 } },
  { NULL }
};

static void
xform_adjust_exec (const struct pn_actuator_option *opts,
		 gpointer data)
{
  glTranslatef(opts[0].val.fval, opts[1].val.fval, opts[2].val.fval);
}

struct pn_actuator_desc builtin_xform_adjust =
{
  "xform_adjust", "Adjust", 
  "Rotates and radially scales the image",
  0, xform_adjust_opts,
  NULL, NULL, xform_adjust_exec
};

#if 0
/* **************** xform_movement **************** */
struct pn_actuator_option_desc xform_movement_opts[] =
{
  { "formula", "The formula to evaluate.",
    OPT_TYPE_STRING, { sval: "r = r * cos(r); d = sin(d);" } },
  { "polar", "Whether the coordinates are polar or not.",
    OPT_TYPE_BOOLEAN, { bval: TRUE } },
  { NULL }
};

typedef struct {
  int width, height;                 /* Previous width and height. */
  struct xform_vector *vfield;
} PnMovementData;

static void
xform_movement_init (gpointer *data)
{
    *data = g_new0(PnMovementData, 1);
}

static void
xform_movement_cleanup (gpointer data)
{
    PnMovementData *d = (PnMovementData *) data;

    if (d)
      {
         if (d->vfield)
 	     g_free (d->vfield);
         g_free (d);
      }
}

inline void
xform_trans_polar (struct xform_vector *vfield, gint x, gint y,
	expression_t *expr, symbol_dict_t *dict)
{
  gdouble *rf, *df;
  gdouble xf, yf;
  gint xn, yn;

  rf = dict_variable(dict, "r");
  df = dict_variable(dict, "d");

  /* Points (xf, yf) must be in a (-1..1) square. */
  xf = 2.0 * x / (pn_image_data->width - 1) - 1.0;
  yf = 2.0 * y / (pn_image_data->height - 1) - 1.0;

  /* Now, convert to polar coordinates r and d. */
  *rf = hypot(xf, yf);
  *df = atan2(yf, xf);

  /* Run the script. */
  expr_execute(expr, dict);

  /* Back to (-1..1) square. */
  xf = (*rf) * cos ((*df));
  yf = (*rf) * sin ((*df));

  /* Convert back to physical coordinates. */
  xn = (int)(((xf + 1.0) * (pn_image_data->width - 1) / 2) + 0.5);
  yn = (int)(((yf + 1.0) * (pn_image_data->height - 1) / 2) + 0.5);

  if (xn < 0 || xn >= pn_image_data->width || yn < 0 || yn >= pn_image_data->height)
    {
      xn = x; yn = y;
    }

  xfvec (xn, yn, &vfield[PN_IMG_INDEX (x, y)]);
}

inline void
xform_trans_literal (struct xform_vector *vfield, gint x, gint y,
	expression_t *expr, symbol_dict_t *dict)
{
  gdouble rf, df;
  gdouble *xf, *yf;
  gint xn, yn;

  xf = dict_variable(dict, "x");
  yf = dict_variable(dict, "y");

  /* Points (xf, yf) must be in a (-1..1) square. */
  *xf = 2.0 * x / (pn_image_data->width - 1) - 1.0;
  *yf = 2.0 * y / (pn_image_data->height - 1) - 1.0;

  /* Run the script. */
  expr_execute(expr, dict);

  /* Convert back to physical coordinates. */
  xn = (int)(((*xf + 1.0) * (pn_image_data->width - 1) / 2) + 0.5);
  yn = (int)(((*yf + 1.0) * (pn_image_data->height - 1) / 2) + 0.5);

  if (xn < 0 || xn >= pn_image_data->width || yn < 0 || yn >= pn_image_data->height)
    {
      xn = x; yn = y;
    }

  xfvec (xn, yn, &vfield[PN_IMG_INDEX (x, y)]);
}

static void
xform_movement_exec (const struct pn_actuator_option *opts,
		 gpointer odata)
{
  PnMovementData *d = (PnMovementData *) odata;
  void (*transform_func)(struct xform_vector *, gint, gint, expression_t *, symbol_dict_t *) = 
        opts[1].val.bval == TRUE ? xform_trans_polar : xform_trans_literal;

  if (d->width != pn_image_data->width
      || d->height != pn_image_data->height)
    {
      gint i, j;
      gdouble *rf, *df;
      gdouble xf, yf;
      gint xn, yn;
      expression_t *expr;
      symbol_dict_t *dict;

      d->width = pn_image_data->width;
      d->height = pn_image_data->height;

      if (d->vfield)
        {
  	  g_free (d->vfield);
          d->vfield = NULL;
        }

      if (opts[0].val.sval == NULL)
        return;

      dict = dict_new();
      expr = expr_compile_string(opts[0].val.sval, dict);
      if (!expr)
        {
           dict_free(dict);
           return;
        }

      rf = dict_variable(dict, "r");
      df = dict_variable(dict, "d");

      d->vfield = g_malloc (sizeof(struct xform_vector)
			    * d->width * d->height);

      for (j = 0; j < pn_image_data->height; j++)
	for (i = 0; i < pn_image_data->width; i++)
	  {
            transform_func(d->vfield, i, j, expr, dict);
	  }
    }

  apply_xform (d->vfield);
  pn_swap_surfaces ();
}

struct pn_actuator_desc builtin_xform_movement =
{
  "xform_movement", "Movement Transform",
  "A customizable blitter.",
  0, xform_movement_opts,
  xform_movement_init, xform_movement_cleanup, xform_movement_exec
};

/* **************** xform_dynmovement **************** */
/* FIXME: really slow */
struct pn_actuator_option_desc xform_dynmovement_opts[] =
{
  { "init_script", "The formula to evaluate on init.",
    OPT_TYPE_STRING, { sval: "" } },
  { "beat_script", "The formula to evaluate on each beat.",
    OPT_TYPE_STRING, { sval: "" } },
  { "frame_script", "The formula to evaluate on each frame.",
    OPT_TYPE_STRING, { sval: "" } },
  { "point_script", "The formula to evaluate.",
    OPT_TYPE_STRING, { sval: "d = 0.15;" } },
  { "polar", "Whether or not the coordinates to use are polar.",
    OPT_TYPE_BOOLEAN, { bval: TRUE } },
  { NULL }
};

typedef struct {
  int width, height;                 /* Previous width and height. */
  expression_t *expr_init;
  expression_t *expr_frame;
  expression_t *expr_beat;
  expression_t *expr_point;
  symbol_dict_t *dict;
  struct xform_vector *vfield;
} PnDynMovementData;

static void
xform_dynmovement_init (gpointer *data)
{
    *data = g_new0(PnDynMovementData, 1);
}

static void
xform_dynmovement_cleanup (gpointer data)
{
    PnDynMovementData *d = (PnDynMovementData *) data;

    if (d)
      {
         if (d->expr_init)
             expr_free (d->expr_init);
         if (d->expr_beat)
             expr_free (d->expr_beat);
         if (d->expr_frame)
             expr_free (d->expr_frame);
         if (d->expr_point)
             expr_free (d->expr_point);
         if (d->dict)
             dict_free (d->dict);
         if (d->vfield)
 	     g_free (d->vfield);
         g_free (d);
      }
}

static void
xform_dynmovement_exec (const struct pn_actuator_option *opts,
		 gpointer odata)
{
  PnDynMovementData *d = (PnDynMovementData *) odata;
  gint i, j;
  gdouble *rf, *df;
  gdouble xf, yf;
  gint xn, yn;
  void (*transform_func)(struct xform_vector *, gint, gint, expression_t *, symbol_dict_t *) = 
        opts[4].val.bval == TRUE ? xform_trans_polar : xform_trans_literal;
  gboolean make_table = FALSE;

  if (d->width != pn_image_data->width
      || d->height != pn_image_data->height)
    {
      d->width = pn_image_data->width;
      d->height = pn_image_data->height;

      if (d->vfield)
        {
  	  g_free (d->vfield);
          d->vfield = NULL;
        }

      if (opts[3].val.sval == NULL)
          return;

      if (!d->dict)
          d->dict = dict_new();
      else
        {
          dict_free(d->dict);
          d->dict = dict_new();
        }

      if (d->expr_init)
        {
          expr_free(d->expr_init);
          d->expr_init = NULL;
        }

      /* initialize */
      d->expr_init = expr_compile_string(opts[0].val.sval, d->dict);

      if (d->expr_init != NULL)
        {
           expr_execute(d->expr_init, d->dict);
        }

      d->expr_beat = expr_compile_string(opts[1].val.sval, d->dict);
      d->expr_frame = expr_compile_string(opts[2].val.sval, d->dict);
      d->expr_point = expr_compile_string(opts[3].val.sval, d->dict);

      d->vfield = g_malloc (sizeof(struct xform_vector)
			    * d->width * d->height);

      make_table = TRUE;
   }

   rf = dict_variable(d->dict, "r");
   df = dict_variable(d->dict, "d");

   if (*opts[2].val.sval != '\0' || pn_new_beat)
       make_table = TRUE;

   /* run the on-frame script. */
   if (make_table == TRUE)
     {
       if (d->expr_beat != NULL)
         expr_execute(d->expr_beat, d->dict);

       if (d->expr_frame != NULL)
         expr_execute(d->expr_frame, d->dict);

       for (j = 0; j < pn_image_data->height; j++)
         for (i = 0; i < pn_image_data->width; i++)
           {
             transform_func(d->vfield, i, j, d->expr_point, d->dict);
           }
     }

  apply_xform (d->vfield);
  pn_swap_surfaces ();
}

struct pn_actuator_desc builtin_xform_dynmovement =
{
  "xform_dynmovement", "Dynamic Movement Transform",
  "A customizable blitter.",
  0, xform_dynmovement_opts,
  xform_dynmovement_init, xform_dynmovement_cleanup, xform_dynmovement_exec
};
#endif
