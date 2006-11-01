/* FIXME: what to name this file? */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "paranormal.h"
#include "actuators.h"
#include "pn_utils.h"

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
  register int sum;
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

