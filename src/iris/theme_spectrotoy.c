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


iris_theme theme_spectrotoy = {
  "Spectrotoy",
  "Seashell shaped spectrum",
  "Cédric Delfosse (cdelfosse@free.fr)",
  "spectrotoy",
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
  return (65.0 + (int) (25.0 * rand () / (RAND_MAX + 1.0)));
}


static void
draw_one_frame (gboolean beat)
{
  unsigned int step;
  GLfloat x1, x2, y1, y2, x3, y3, x4, y4;
  GLfloat y;
  GLfloat red, green, blue;
  static GLfloat data[16];	// where the spectrum values are stored

  for (step = 0; step < 16; step++)
    {
      if (datas.data1[step] > data[step])
	data[step] = datas.data1[step];
      else
	data[step] -= 0.02;
      if (data[step] < 0.0)
	data[step] = 0.0;
    }
  data[16] = data[0];

  glBegin (GL_TRIANGLES);

  x1 = 0.5 * cos (0);
  y1 = 0.5 * sin (0);

  x2 = (0.5 + data[0] * 3) * cos (0);
  y2 = (0.5 + data[0] * 3) * sin (0);

  for (step = 0; step < 17; step++)
    {
      y = data[step] * 3;

      x3 = 0.5 * cos (step * 2 * M_PI / 16);
      y3 = 0.5 * sin (step * 2 * M_PI / 16);

      x4 = (0.5 + y) * cos (step * 2 * M_PI / 16);
      y4 = (0.5 + y) * sin (step * 2 * M_PI / 16);

      get_color (&red, &green, &blue, &data[step]);

      /* a side */
      glColor4f (red / 2, green / 2, blue / 2, 0.5);
      glVertex3f (x1, 0, y1);
      glVertex3f (x2, 0, y2);
      glVertex3f (x2, y, y2);
      glVertex3f (x2, y, y2);
      glVertex3f (x1, y, y1);
      glVertex3f (x1, 0, y1);

      /* another side */
      glColor4f (red / 2, green / 2, blue / 2, 0.5);
      glVertex3f (x3, 0, y3);
      glVertex3f (x4, 0, y4);
      glVertex3f (x4, y, y4);
      glVertex3f (x4, y, y4);
      glVertex3f (x3, y, y3);
      glVertex3f (x3, 0, y3);

      /* side in */
      glColor4f (red / 2, green / 2, blue / 2, 0.5);
      glVertex3f (x1, 0, y1);
      glVertex3f (x3, 0, y3);
      glVertex3f (x3, y, y3);
      glVertex3f (x3, y, y3);
      glVertex3f (x1, y, y1);
      glVertex3f (x1, 0, y1);

      /* side out */
      glColor4f (red / 2, green / 2, blue / 2, 0.5);
      glVertex3f (x4, 0, y4);
      glVertex3f (x2, 0, y2);
      glVertex3f (x2, y, y2);
      glVertex3f (x2, y, y2);
      glVertex3f (x4, y, y4);
      glVertex3f (x4, 0, y4);

      /* top */
      glColor4f (red, green, blue, 0.5);
      glVertex3f (x1, y, y1);
      glVertex3f (x2, y, y2);
      glVertex3f (x3, y, y3);
      glVertex3f (x3, y, y3);
      glVertex3f (x2, y, y2);
      glVertex3f (x4, y, y4);

      x1 = x3;
      y1 = y3;
      x2 = x4;
      y2 = y4;
    }

  glEnd ();

}
