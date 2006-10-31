#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <glib.h>

#include "paranormal.h"
#include "actuators.h"

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
  { 0 }
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
  { 0 }
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
  "Sets the colormap to a gradient going from black to "
  "while, via an intermediate color",
  0, cmap_bwgradient_opts,
  NULL, NULL, cmap_bwgradient_exec
};
