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


static GLfloat get_x_angle (void);
static void draw_one_frame (gboolean);


iris_theme theme_pyramid = {
  "Pyramid",
  "Pyramid shaped spectrum",
  "Marinus Schraal (foser@sesmar.eu.org)",
  "pyramid",
  &conf,
  &conf_new,
  sizeof (conf_private),
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  get_x_angle,
  draw_one_frame,
};


static GLfloat
get_x_angle ()
{
  return (10.0 + (int) (30.0 * rand () / (RAND_MAX + 1.0)));
}


static void
draw_one_frame (gboolean beat)
{
  GLfloat scale = 2.0;		// scales the whole squares, higher is bigger
  GLfloat x, height = -4, oldheight;
  GLfloat red, green, blue;
  static GLfloat data[16];	// where the spectrum values are stored
  int l;

  for (l = 0; l < 16; l++)
    {
      if (datas.data1[l] > data[l])
	data[l] = datas.data1[l];
      else
	data[l] -= 0.015;
      if (data[l] < 0.0)
	data[l] = 0.0;
    }

  glBegin (GL_QUADS);
  for (l = 0; l < 16; l++)
    {
      x = data[l] * scale;
      oldheight = height;
      height = height + 0.4;

      if (data[l] > 0.0)
	{
	  get_color (&red, &green, &blue, &data[l]);
	  glColor4f (red, green, blue, 0.75);

	  /* sides */
	  //top right
	  glVertex3f (x, height, x);	// top right (front)
	  glVertex3f (x, oldheight, x);	//bottom right
	  glVertex3f (x, oldheight, -x);	//bottom left
	  glVertex3f (x, height, -x);	// top left 
	  //top left
	  glVertex3f (x, height, -x);	// top right (front)
	  glVertex3f (x, oldheight, -x);	//bottom right
	  glVertex3f (-x, oldheight, -x);	//bottom left
	  glVertex3f (-x, height, -x);	// top left 
	  //bottom left
	  glVertex3f (-x, height, -x);	// top right (front)
	  glVertex3f (-x, oldheight, -x);	//bottom right
	  glVertex3f (-x, oldheight, x);	//bottom left
	  glVertex3f (-x, height, x);	// top left

	  //bottom right
	  glVertex3f (-x, height, x);	// top right (front)
	  glVertex3f (-x, oldheight, x);	//bottom right
	  glVertex3f (x, oldheight, x);	//bottom left
	  glVertex3f (x, height, x);	// top left

	  get_color (&red, &green, &blue, &data[l]);
	  glColor4f (red, green, blue, 0.5);
	  /* top */
	  glVertex3f (x, height, x);	// Top Right Of The Quad (Bottom)
	  glVertex3f (x, height, -x);	// Top Left Of The Quad (Bottom)
	  glVertex3f (-x, height, -x);	// Bottom Left Of The Quad (Bottom)
	  glVertex3f (-x, height, x);	// Bottom Right Of The Quad (Bottom)

	}
    }

  glEnd ();

}
