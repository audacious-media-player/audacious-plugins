/* FIXME: allow for only using an xform on part of the img? */
/* FIXME: perhaps combine these into a single vector field
   so that only 1 apply_xform needs to be done for as many
   of these as someone wants to use */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <math.h>

#include <glib.h>

#include "paranormal.h"
#include "actuators.h"
#include "pn_utils.h"

struct xform_vector
{
  guint offset; /* the offset of the top left pixel */
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

