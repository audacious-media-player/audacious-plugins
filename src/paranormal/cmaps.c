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

#include <glib.h>

#include "paranormal.h"
#include "actuators.h"

#include "libcalc/calc.h"

#define STD_CMAP_OPTS { "low_index", "The lowest index of the \
color map that should be altered", OPT_TYPE_COLOR_INDEX, { ival: 0 } },\
{ "high_index", "The highest index of the color map that should be \
altered", OPT_TYPE_COLOR_INDEX, { ival: 255 } }

static struct pn_color black = {0, 0, 0};
static struct pn_color white = {255, 255, 255};

/* **************** cmap generation funcs **************** */
static void
cmap_gen_gradient (int step, const struct pn_color *a,
		   const struct pn_color *b,
		   struct pn_color *c)
{
  c->r = a->r + step * ((((float)b->r) - ((float)a->r)) / 256.0);
  c->g = a->g + step * ((((float)b->g) - ((float)a->g)) / 256.0);
  c->b = a->b + step * ((((float)b->b) - ((float)a->b)) / 256.0);
}

/* **************** cmap_gradient **************** */
static struct pn_actuator_option_desc cmap_gradient_opts[] =
{
  STD_CMAP_OPTS,
  { "lcolor", "The low color used in the gradient generation",
    OPT_TYPE_COLOR, { cval: {0, 0, 0} } },
  { "hcolor", "The high color used in the gradient generation",
    OPT_TYPE_COLOR, { cval: {0, 0, 0} } },
  { NULL }
};

static void
cmap_gradient_exec (const struct pn_actuator_option *opts,
		    gpointer data)
{
  int i;

  for (i=opts[0].val.ival; i<=opts[1].val.ival; i++)
    cmap_gen_gradient (((i-opts[0].val.ival)<<8)/(opts[1].val.ival
						  - opts[0].val.ival),
		       &opts[2].val.cval, &opts[3].val.cval,
		       &pn_image_data->cmap[i]);
}

struct pn_actuator_desc builtin_cmap_gradient =
{
  "cmap_gradient",
  "Normal colourmap",
  "Sets the colormap to a gradient going from <lcolor> to "
  "<hcolor>",
  0, cmap_gradient_opts,
  NULL, NULL, cmap_gradient_exec
};

/* **************** cmap_bwgradient **************** */
static struct pn_actuator_option_desc cmap_bwgradient_opts[] =
{
  STD_CMAP_OPTS,
  { "color", "The intermediate color to use in the gradient",
    OPT_TYPE_COLOR, { cval: {191, 191, 191} } },
  { NULL }
};

static void
cmap_bwgradient_exec (const struct pn_actuator_option *opts,
		      gpointer data)
{
  int i;

  for (i=opts[0].val.ival; i<128 && i<=opts[1].val.ival; i++)
    cmap_gen_gradient (i<<1, &black, &opts[2].val.cval,
		       &pn_image_data->cmap[i]);

  for (i=128; i<256 && i<=opts[1].val.ival; i++)
    cmap_gen_gradient ((i-128)<<1, &opts[2].val.cval, &white,
		       &pn_image_data->cmap[i]);
}
    
struct pn_actuator_desc builtin_cmap_bwgradient =
{
  "cmap_bwgradient",
  "Value-based colourmap",
  "Sets the colormap to a gradient going from black to "
  "white, via an intermediate color",
  0, cmap_bwgradient_opts,
  NULL, NULL, cmap_bwgradient_exec
};

/* **************** cmap_dynamic **************** */
static struct pn_actuator_option_desc cmap_dynamic_opts[] =
{
  STD_CMAP_OPTS,
  { "script", "The script to run on each step.",
    OPT_TYPE_STRING, { sval: "red = red + 0.01; blue = blue + 0.01; green = green + 0.01;" } },
  { NULL }
};

typedef struct {
  expression_t *expr;
  symbol_dict_t *dict;
} PnDynamicColourmapData;

static void
cmap_dynamic_init(gpointer *data)
{
  *data = g_new0(PnDynamicColourmapData, 1);
}

static void
cmap_dynamic_cleanup(gpointer data)
{
  PnDynamicColourmapData *d = (PnDynamicColourmapData *) data;

  if (d->expr)
    expr_free(d->expr);
  if (d->dict)
    dict_free(d->dict);

  g_free(d);
}

static void
cmap_dynamic_exec(const struct pn_actuator_option *opts,
		  gpointer data)
{
  PnDynamicColourmapData *d = (PnDynamicColourmapData *) data;
  gint i;
  gdouble *rf, *bf, *gf, *inf;
  gint rn, bn, gn;

  if (!d->dict && !d->expr)
    {
       d->dict = dict_new();
       if (!d->dict)
         return;

       d->expr = expr_compile_string(opts[2].val.sval, d->dict);
       if (!d->expr)
         {
           dict_free(d->dict);
           d->dict = NULL;
           return;
         }
    }

  rf = dict_variable(d->dict, "red");
  gf = dict_variable(d->dict, "green");
  bf = dict_variable(d->dict, "blue");
  inf = dict_variable(d->dict, "index");

  for (i = opts[0].val.ival; i < 255 && i <= opts[1].val.ival; i++)
    {
       *inf = ((gdouble)i / 255.0);

       expr_execute(d->expr, d->dict);

       /* Convert rf/bf/gf to realworld values. */
       rn = (gdouble)(*rf * 255);
       gn = (gdouble)(*gf * 255);
       bn = (gdouble)(*bf * 255);

       pn_image_data->cmap[i].r = rn;
       pn_image_data->cmap[i].g = gn;
       pn_image_data->cmap[i].b = bn;
    }
}

struct pn_actuator_desc builtin_cmap_dynamic =
{
  "cmap_dynamic",
  "Dynamic Colourmap",
  "Scriptable colourmap modifier.",
  0, cmap_dynamic_opts,
  cmap_dynamic_init, cmap_dynamic_cleanup, cmap_dynamic_exec
};
