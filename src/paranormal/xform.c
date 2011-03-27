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

struct xform_vector
{
  gint32 offset; /* the offset of the top left pixel */
  guint16 w; /* 4:4:4:4 NE, NW, SE, SW pixel weights
		  The total should be 16 */

  /* if offset < 0 then w is the color index to
     which the pixel will be set */
};

static void
xfvec (float x, float y, struct xform_vector *v)
{
  float xd, yd;
  int weight[4];

  if (x >= pn_image_data->width-1 || y >= pn_image_data->height-1
      || x < 0 || y < 0)
    {
      v->offset = -1;
      v->w = 0;
      return;
    }

  v->offset = PN_IMG_INDEX (floor(x), floor(y));

  xd = x - floor (x);
  yd = y - floor (y);

  weight[3] = xd * yd * 16;
  weight[2] = (1-xd) * yd * 16;
  weight[1] = xd * (1-yd) * 16;
  weight[0] = 16 - weight[3] - weight[2] - weight[1]; /* just in case */

  v->w = (weight[0]<<12) | (weight[1]<<8) | (weight[2]<<4) | weight[3];
}

static void
apply_xform (struct xform_vector *vfield)
{
  int i;
  struct xform_vector *v;
  register guchar *srcptr;
  register guchar *destptr;
  register int color;

  if (vfield == NULL)
      return;

  for (i=0, v=vfield, destptr=pn_image_data->surface[1];
       i<pn_image_data->width*pn_image_data->height;
       i++, v++, destptr++)
    {
      /* off the screen */
      if (v->offset < 0)
	{
	  *destptr = (guchar)v->w;
	  continue;
	}

      srcptr = pn_image_data->surface[0] + v->offset;

      /* exactly on the pixel */
      if (v->w == 0)
	  *destptr = *srcptr;

      /* gotta blend the points */
      else
	{
	  color =  *srcptr * (v->w>>12);
	  color += *++srcptr * ((v->w>>8) & 0x0f);
	  color += *(srcptr+=pn_image_data->width) * (v->w & 0x0f);
	  color += *(--srcptr) * ((v->w>>4) & 0x0f);
	  color >>= 4;
	  *destptr = (guchar)color;
	}
    }
}

/* **************** xform_spin **************** */
/* FIXME: Describe these better, how they are backwards */
/* FIXME: better name? */
struct pn_actuator_option_desc xform_spin_opts[] =
{
  { "angle", "The angle of rotation", OPT_TYPE_FLOAT, { fval: -8.0 } },
  { "r_add", "The number of pixels by which the r coordinate will be "
    "increased (before scaling)", OPT_TYPE_FLOAT, { fval: 0.0 } },
  { "r_scale", "The amount by which the r coordinate of each pixel will "
    "be scaled", OPT_TYPE_FLOAT, { fval: 1.0 } },
  { NULL }
};

struct xform_spin_data
{
  int width, height;
  struct xform_vector *vfield;
};

static void
xform_spin_init (gpointer *data)
{
  *data = g_new0 (struct xform_spin_data, 1);
}

static void
xform_spin_cleanup (gpointer data)
{
  struct xform_spin_data *d = (struct xform_spin_data *) data;


  if (d)
    {
      if (d->vfield)
	g_free (d->vfield);
      g_free (d);
    }
}

static void
xform_spin_exec (const struct pn_actuator_option *opts,
		 gpointer data)
{
  struct xform_spin_data *d = (struct xform_spin_data*)data;
  float i, j;

  if (d->width != pn_image_data->width
      || d->height != pn_image_data->height)
    {
      d->width = pn_image_data->width;
      d->height = pn_image_data->height;

      if (d->vfield)
	g_free (d->vfield);

      d->vfield = g_malloc0 (sizeof(struct xform_vector)
			    * d->width * d->height);

      for (j=-(pn_image_data->height>>1)+1; j<=pn_image_data->height>>1; j++)
	for (i=-(pn_image_data->width>>1); i<pn_image_data->width>>1; i++)
	  {
	    float r, t = 0;
	    float x, y;

	    r = sqrt (i*i + j*j);
	    if (r)
	      t = asin (j/r);
	    if (i < 0)
	      t = M_PI - t;

	    t += opts[0].val.fval * M_PI/180.0;
	    r += opts[1].val.fval;
	    r *= opts[2].val.fval;

	    x = (r * cos (t)) + (pn_image_data->width>>1);
	    y = (pn_image_data->height>>1) - (r * sin (t));

	    xfvec (x, y, &d->vfield
		   [PN_IMG_INDEX ((pn_image_data->width>>1)+(int)rint(i),
				  ((pn_image_data->height>>1)-(int)rint(j)))]);
	  }
    }

  apply_xform (d->vfield);
  pn_swap_surfaces ();
}

struct pn_actuator_desc builtin_xform_spin =
{
  "xform_spin", "Spin Transform",
  "Rotates and radially scales the image",
  0, xform_spin_opts,
  xform_spin_init, xform_spin_cleanup, xform_spin_exec
};

/* **************** xform_ripple **************** */
struct pn_actuator_option_desc xform_ripple_opts[] =
{
  { "angle", "The angle of rotation", OPT_TYPE_FLOAT, { fval: 0 } },
  { "ripples", "The number of ripples that fit on the screen "
    "(horizontally)", OPT_TYPE_FLOAT, { fval: 8 } },
  { "base_speed", "The minimum number of pixels to move each pixel",
    OPT_TYPE_FLOAT, { fval: 1 } },
  { "mod_speed", "The maximum number of pixels by which base_speed"
    " will be modified", OPT_TYPE_FLOAT, { fval: 1 } },
  { NULL }
};

struct xform_ripple_data
{
  int width, height;
  struct xform_vector *vfield;
};

static void
xform_ripple_init (gpointer *data)
{
  *data = g_new0 (struct xform_ripple_data, 1);
}

static void
xform_ripple_cleanup (gpointer data)
{
  struct xform_ripple_data *d = (struct xform_ripple_data*) data;

  if (d)
    {
      if (d->vfield)
	g_free (d->vfield);
      g_free (d);
    }
}

static void
xform_ripple_exec (const struct pn_actuator_option *opts,
		 gpointer data)
{
  struct xform_ripple_data *d = (struct xform_ripple_data*)data;
  float i, j;

  if (d->width != pn_image_data->width
      || d->height != pn_image_data->height)
    {
      d->width = pn_image_data->width;
      d->height = pn_image_data->height;

      if (d->vfield)
	g_free (d->vfield);

      d->vfield = g_malloc (sizeof(struct xform_vector)
			    * d->width * d->height);

      for (j=-(pn_image_data->height>>1)+1; j<=pn_image_data->height>>1; j++)
	for (i=-(pn_image_data->width>>1); i<pn_image_data->width>>1; i++)
	  {
	    float r, t = 0;
	    float x, y;

	    r = sqrt (i*i + j*j);
	    if (r)
	      t = asin (j/r);
	    if (i < 0)
	      t = M_PI - t;

	    t += opts[0].val.fval * M_PI/180.0;

	    if (r > 4)//(pn_image_data->width/(2*opts[1].val.fval)))
	      r -=  opts[2].val.fval + (opts[3].val.fval/2) *
		(1 + sin ((r/(pn_image_data->width/(2*opts[1].val.fval)))*M_PI));
/*  	    else if (r > 4) */
/*  	      r *= r/(pn_image_data->width/opts[1].val.fval); */
	    else /* don't let it explode */
	      r = 1000000;


	    x = (r * cos (t)) + (pn_image_data->width>>1);
	    y = (pn_image_data->height>>1) - (r * sin (t));

	    xfvec (x, y, &d->vfield
		   [PN_IMG_INDEX ((pn_image_data->width>>1)+(int)rint(i),
				  ((pn_image_data->height>>1)-(int)rint(j)))]);
	  }
    }

  apply_xform (d->vfield);
  pn_swap_surfaces ();
}

struct pn_actuator_desc builtin_xform_ripple =
{
  "xform_ripple", "Ripple Transform", "Creates an ripple effect",
  0, xform_ripple_opts,
  xform_ripple_init, xform_ripple_cleanup, xform_ripple_exec
};

/* **************** xform_bump_spin **************** */
struct pn_actuator_option_desc xform_bump_spin_opts[] =
{
  { "angle", "The angle of rotation", OPT_TYPE_FLOAT, { fval: 0 } },
  { "bumps", "The number of bumps that on the image",
    OPT_TYPE_FLOAT, { fval: 8 } },
  { "base_scale", "The base radial scale",
    OPT_TYPE_FLOAT, { fval: 0.95 } },
  { "mod_scale", "The maximum amount that should be "
    "added to the base_scale to create the 'bump' effect",
    OPT_TYPE_FLOAT, { fval: .1 } },
  { NULL }
};

struct xform_bump_spin_data
{
  int width, height;
  struct xform_vector *vfield;
};

static void
xform_bump_spin_init (gpointer *data)
{
  *data = g_new0 (struct xform_bump_spin_data, 1);
}

static void
xform_bump_spin_cleanup (gpointer data)
{
  struct xform_bump_spin_data *d = (struct xform_bump_spin_data*) data;

  if (d)
    {
      if (d->vfield)
	g_free (d->vfield);
      g_free (d);
    }
}

static void
xform_bump_spin_exec (const struct pn_actuator_option *opts,
		 gpointer data)
{
  struct xform_bump_spin_data *d = (struct xform_bump_spin_data*)data;
  float i, j;

  if (d->width != pn_image_data->width
      || d->height != pn_image_data->height)
    {
      d->width = pn_image_data->width;
      d->height = pn_image_data->height;

      if (d->vfield)
	g_free (d->vfield);

      d->vfield = g_malloc (sizeof(struct xform_vector)
			    * d->width * d->height);

      for (j=-(pn_image_data->height>>1)+1; j<=pn_image_data->height>>1; j++)
	for (i=-(pn_image_data->width>>1); i<pn_image_data->width>>1; i++)
	  {
	    float r, t = 0;
	    float x, y;

	    r = sqrt (i*i + j*j);
	    if (r)
	      t = asin (j/r);
	    if (i < 0)
	      t = M_PI - t;

	    t += opts[0].val.fval * M_PI/180.0;

	    r *=  opts[2].val.fval + opts[3].val.fval
	      * (1 + sin (t*opts[1].val.fval));

	    x = (r * cos (t)) + (pn_image_data->width>>1);
	    y = (pn_image_data->height>>1) - (r * sin (t));

	    xfvec (x, y, &d->vfield
		   [PN_IMG_INDEX ((pn_image_data->width>>1)+(int)rint(i),
				  ((pn_image_data->height>>1)-(int)rint(j)))]);
	  }
    }

  apply_xform (d->vfield);
  pn_swap_surfaces ();
}

struct pn_actuator_desc builtin_xform_bump_spin =
{
  "xform_bump_spin", "Bump Transform",
  "Rotate the image at a varying speed to create "
  "the illusion of bumps",
  0, xform_bump_spin_opts,
  xform_bump_spin_init, xform_bump_spin_cleanup, xform_bump_spin_exec
};

/* **************** xform_halfrender **************** */
struct pn_actuator_option_desc xform_halfrender_opts[] =
{
  { "direction", "Negative is horizontal, positive is vertical.",
    OPT_TYPE_INT, { ival: 1 } },
  { "render_twice", "Render the second image.",
    OPT_TYPE_BOOLEAN, { bval: TRUE } },
  { NULL }
};

static void
xform_halfrender_exec (const struct pn_actuator_option *opts,
		 gpointer data)
{
  gint x, y;

  if (opts[0].val.ival < 0)
    {
       for (y = 0; y < pn_image_data->height; y += 2)
         {
            for (x = 0; x < pn_image_data->width; x++)
              {
                 pn_image_data->surface[1][PN_IMG_INDEX(x, y / 2)] =
                   pn_image_data->surface[0][PN_IMG_INDEX(x, y)];
	         if (opts[1].val.bval)
                   {
                     pn_image_data->surface[1][PN_IMG_INDEX(x, (y / 2) + (pn_image_data->height / 2))] =
                       pn_image_data->surface[0][PN_IMG_INDEX(x, y)];
                   }
              }
         }
    }
  else
    {
       for (y = 0; y < pn_image_data->height; y++)
         {
            for (x = 0; x < pn_image_data->width; x += 2)
              {
                 pn_image_data->surface[1][PN_IMG_INDEX(x / 2, y)] =
                   pn_image_data->surface[0][PN_IMG_INDEX(x, y)];
                 if (opts[1].val.bval)
                   {
                     pn_image_data->surface[1][PN_IMG_INDEX((x / 2) + (pn_image_data->width / 2), y)] =
                       pn_image_data->surface[0][PN_IMG_INDEX(x, y)];
                   }
              }
         }
    }

    pn_swap_surfaces ();
}

struct pn_actuator_desc builtin_xform_halfrender =
{
  "xform_halfrender", "Halfrender Transform",
  "Divides the surface in half and renders it twice.",
  0, xform_halfrender_opts,
  NULL, NULL, xform_halfrender_exec
};

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
  gint xn, yn;
  gdouble *xf = dict_variable(dict, "x");
  gdouble *yf = dict_variable(dict, "y");

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
