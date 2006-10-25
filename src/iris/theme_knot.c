/*  Iris - visualization plugin for XMMS
 *  Copyright (C) 2000-2002 Cédric DELFOSSE (cdelfosse@free.fr)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <math.h>
#include <GL/gl.h>
#include <audacious/configdb.h>
#include "iris.h"


static struct
{
}
conf_private, conf_private_new;

static config_theme conf = {
  NULL,
  &conf_private
};

static config_theme conf_new = {
  NULL,
  &conf_private_new
};


static void init (void);
static void cleanup (void);
static void init_draw_mode (void);
static GLfloat get_x_angle (void);
static void draw_one_frame (gboolean);


iris_theme theme_knot = {
  "Knot",
  "Dancing knot",
  "Pascal Brochart for Nebulus, adapted for Iris by foser",
  "knot",
  &conf,
  &conf_new,
  sizeof (conf_private),
  NULL,
  NULL,
  NULL,
  NULL,
  init,
  cleanup,
  init_draw_mode,
  get_x_angle,
  draw_one_frame,
};


gboolean reverse = FALSE;
GLfloat knot_time;

typedef struct
{
  GLfloat x, y, z;
}
glcoord;

typedef struct
{
  GLfloat r, g, b;
}
glcolors;

typedef struct
{
  int numfaces, numverts, numsides;
  GLuint faces[16 * 64 * 4 * 4];
  glcoord vertices[16 * 64];
  glcolors Colors[16 * 64];
}
tknotobject;

tknotobject *knotobject;


void
createknot (int scaling_factor1, int scaling_factor2, GLfloat radius1,
	    GLfloat radius2, GLfloat radius3)
{
  int count1, count2;
  GLfloat alpha, beta, distance, mindistance, rotation;
  GLfloat x, y, z, dx, dy, dz;
  GLfloat value, modulus, dist;
  int index1, index2;

  knotobject->numsides = 4;

  knotobject->numverts = 0;
  alpha = 0;
  for (count2 = 0; count2 < scaling_factor2; count2++)
    {
      alpha = alpha + 2 * M_PI / scaling_factor2;
      x = radius2 * cos (2 * alpha) + radius1 * sin (alpha);
      y = radius2 * sin (2 * alpha) + radius1 * cos (alpha);
      z = radius2 * cos (3 * alpha);
      dx = -2 * radius2 * sin (2 * alpha) + radius1 * cos (alpha);
      dy = 2 * radius2 * cos (2 * alpha) - radius1 * sin (alpha);
      dz = -3 * radius2 * sin (3 * alpha);
      value = sqrt (dx * dx + dz * dz);
      modulus = sqrt (dx * dx + dy * dy + dz * dz);

      beta = 0;
      for (count1 = 0; count1 < scaling_factor1; count1++)
	{
	  beta = beta + 2 * M_PI / scaling_factor1;

	  knotobject->vertices[knotobject->numverts].x =
	    x - radius3 * (cos (beta) * dz -
			   sin (beta) * dx * dy / modulus) / value;
	  knotobject->vertices[knotobject->numverts].y =
	    y - radius3 * sin (beta) * value / modulus;
	  knotobject->vertices[knotobject->numverts].z =
	    z + radius3 * (cos (beta) * dx +
			   sin (beta) * dy * dz / modulus) / value;


	  dist =
	    sqrt (knotobject->vertices[knotobject->numverts].x *
		  knotobject->vertices[knotobject->numverts].x +
		  knotobject->vertices[knotobject->numverts].y *
		  knotobject->vertices[knotobject->numverts].y +
		  knotobject->vertices[knotobject->numverts].z *
		  knotobject->vertices[knotobject->numverts].z);

	  knotobject->Colors[knotobject->numverts].r =
	    ((2 / dist) + (0.5 * sin (beta) + 0.4)) / 2.0;
	  knotobject->Colors[knotobject->numverts].g =
	    ((2 / dist) + (0.5 * sin (beta) + 0.4)) / 2.0;
	  knotobject->Colors[knotobject->numverts].b =
	    ((2 / dist) + (0.5 * sin (beta) + 0.4)) / 2.0;

	  knotobject->numverts++;
	}
    }

  for (count1 = 0; count1 < scaling_factor2; count1++)
    {
      index1 = count1 * scaling_factor1;
      index2 = index1 + scaling_factor1;
      index2 = index2 % knotobject->numverts;
      rotation = 0;
      mindistance =
	(knotobject->vertices[index1].x -
	 knotobject->vertices[index2].x) * (knotobject->vertices[index1].x -
					   knotobject->vertices[index2].x) +
	(knotobject->vertices[index1].y -
	 knotobject->vertices[index2].y) * (knotobject->vertices[index1].y -
					   knotobject->vertices[index2].y) +
	(knotobject->vertices[index1].z -
	 knotobject->vertices[index2].z) * (knotobject->vertices[index1].z -
					   knotobject->vertices[index2].z);
      for (count2 = 1; count2 < scaling_factor1; count2++)
	{
	  index2 = count2 + index1 + scaling_factor1;
	  if (count1 == scaling_factor2 - 1)
	    index2 = count2;
	  distance =
	    (knotobject->vertices[index1].x -
	     knotobject->vertices[index2].x) * (knotobject->vertices[index1].x -
					       knotobject->vertices[index2].
					       x) +
	    (knotobject->vertices[index1].y -
	     knotobject->vertices[index2].y) * (knotobject->vertices[index1].y -
					       knotobject->vertices[index2].
					       y) +
	    (knotobject->vertices[index1].z -
	     knotobject->vertices[index2].z) * (knotobject->vertices[index1].z -
					       knotobject->vertices[index2].z);
	  if (distance < mindistance)
	    {
	      mindistance = distance;
	      rotation = count2;
	    }
	}

      for (count2 = 0; count2 < scaling_factor1; count2++)
	{
	  knotobject->faces[4 * (index1 + count2) + 0] = index1 + count2;

	  index2 = count2 + 1;
	  index2 = index2 % scaling_factor1;
	  knotobject->faces[4 * (index1 + count2) + 1] = index1 + index2;

	  index2 = (count2 + rotation + 1);
	  index2 = index2 % scaling_factor1;
	  knotobject->faces[4 * (index1 + count2) + 2] =
	    (index1 + index2 + scaling_factor1) % knotobject->numverts;

	  index2 = (count2 + rotation);
	  index2 = index2 % scaling_factor1;

	  knotobject->faces[4 * (index1 + count2) + 3] =
	    (index1 + index2 + scaling_factor1) % knotobject->numverts;
	  knotobject->numfaces++;
	}
    }
}


static void
init ()
{
  knotobject = (tknotobject *)malloc(sizeof(tknotobject));
  createknot (16, 64, 2, 2.0, 1.0);
}


static void
cleanup()
{
  free(knotobject);
}


static GLfloat
get_x_angle ()
{
  return (10.0 + (int) (80.0 * rand () / (RAND_MAX + 1.0)));
}


static void
init_draw_mode ()
{
  //don't think all of this really is needed here..
  glDepthFunc (GL_LESS);
  glDisable (GL_NORMALIZE);
  glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  glDisable (GL_BLEND);
  glEnable (GL_DEPTH_TEST);
}


void
recalculateknot (int scaling_factor1, int scaling_factor2, GLfloat radius1,
		 GLfloat radius2, GLfloat radius3)
{
  int count1, count2;
  GLfloat alpha, beta;
  GLfloat x, y, z, dx, dy, dz;
  GLfloat value, modulus;
  int index1, index2;
  GLfloat distance, Mindistance, rotation;

  knotobject->numverts = 0;
  alpha = 0;
  for (count2 = 0; count2 < scaling_factor2; count2++)
    {
      alpha = alpha + 2 * M_PI / scaling_factor2;
      x = radius2 * cos (2 * alpha) + radius1 * sin (alpha);
      y = radius2 * sin (2 * alpha) + radius1 * cos (alpha);
      z = radius2 * cos (3 * alpha);
      dx = -2 * radius2 * sin (2 * alpha) + radius1 * cos (alpha);
      dy = 2 * radius2 * cos (2 * alpha) - radius1 * sin (alpha);
      dz = -3 * radius2 * sin (3 * alpha);
      value = sqrt (dx * dx + dz * dz);
      modulus = sqrt (dx * dx + dy * dy + dz * dz);

      beta = 0;
      for (count1 = 0; count1 < scaling_factor1; count1++)
	{
	  beta = beta + 2 * M_PI / scaling_factor1;

	  knotobject->vertices[knotobject->numverts].x =
	    x - radius3 * (cos (beta) * dz -
			   sin (beta) * dx * dy / modulus) / value;
	  knotobject->vertices[knotobject->numverts].y =
	    y - radius3 * sin (beta) * value / modulus;
	  knotobject->vertices[knotobject->numverts].z =
	    z + radius3 * (cos (beta) * dx +
			   sin (beta) * dy * dz / modulus) / value;

	  knotobject->numverts++;
	}
    }

  knotobject->numfaces = 0;
  for (count1 = 0; count1 < scaling_factor2; count1++)
    {
      index1 = count1 * scaling_factor1;
      index2 = index1 + scaling_factor1;
      index2 = index2 % knotobject->numverts;
      rotation = 0;
      Mindistance =
	(knotobject->vertices[index1].x -
	 knotobject->vertices[index2].x) * (knotobject->vertices[index1].x -
					   knotobject->vertices[index2].x) +
	(knotobject->vertices[index1].y -
	 knotobject->vertices[index2].y) * (knotobject->vertices[index1].y -
					   knotobject->vertices[index2].y) +
	(knotobject->vertices[index1].z -
	 knotobject->vertices[index2].z) * (knotobject->vertices[index1].z -
					   knotobject->vertices[index2].z);
      for (count2 = 1; count2 < scaling_factor1; count2++)
	{
	  index2 = count2 + index1 + scaling_factor1;
	  if (count1 == scaling_factor2 - 1)
	    index2 = count2;
	  distance =
	    (knotobject->vertices[index1].x -
	     knotobject->vertices[index2].x) * (knotobject->vertices[index1].x -
					       knotobject->vertices[index2].
					       x) +
	    (knotobject->vertices[index1].y -
	     knotobject->vertices[index2].y) * (knotobject->vertices[index1].y -
					       knotobject->vertices[index2].
					       y) +
	    (knotobject->vertices[index1].z -
	     knotobject->vertices[index2].z) * (knotobject->vertices[index1].z -
					       knotobject->vertices[index2].z);
	  if (distance < Mindistance)
	    {
	      Mindistance = distance;
	      rotation = count2;
	    }
	}

      for (count2 = 0; count2 < scaling_factor1; count2++)
	{
	  knotobject->faces[4 * (index1 + count2) + 0] = index1 + count2;

	  //removes draw artifacts
	  //if (point_general->hide_glue)
	  index2 = index2 % scaling_factor1;

	  knotobject->faces[4 * (index1 + count2) + 2] =
	    (index1 + index2 + scaling_factor1) % knotobject->numverts;

	  index2 = count2 + rotation;
	  index2 = index2 % scaling_factor1;

	  knotobject->faces[4 * (index1 + count2) + 3] =
	    (index1 + index2 + scaling_factor1) % knotobject->numverts;
	  knotobject->numfaces++;
	}
    }
}


void
draw_the_knot (void)
{
  int i, j, num;
  float red, green, blue, peak;

  glBegin (GL_QUADS);
  j = knotobject->numfaces * 4;
  for (i = 0; i < j; i++)
    {
      num = knotobject->faces[i];

      //adapted to use Iris colors
      get_color (&red, &green, &blue, &peak);
      glColor3f (knotobject->Colors[num].r * red,
		 knotobject->Colors[num].g * green, knotobject->Colors[num].b);
      glVertex3f (knotobject->vertices[num].x, knotobject->vertices[num].y,
		  knotobject->vertices[num].z);
    }
  glEnd ();
}


void
createknotpolygons (void)
{
  if (datas.loudness > 12000)
    if (reverse)
      knot_time -= 12000.0f / (16.0f * config.fps);
    else
      knot_time += 12000.0f / (16.0f * config.fps);
  else if (reverse)
    knot_time -= datas.loudness / (16.0f * config.fps);
  else
    knot_time += datas.loudness / (16.0f * config.fps);

  recalculateknot (16, 64, 3 * sin (0.015 * knot_time) + 0.8,
		   3 * cos (0.008 * knot_time) + 0.8, 1.1);

  glTranslatef (0.0f, 0.0f, 0.0f);
  draw_the_knot ();
}


static void
draw_one_frame (gboolean beat)
{
  if (beat)
    reverse = !reverse;
  glPushMatrix ();
  /* scale it all down to Iris sizes */
  glScalef (0.5, 0.5, 0.5);
  createknotpolygons ();
  glPopMatrix ();
}
