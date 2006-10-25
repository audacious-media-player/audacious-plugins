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
#include <string.h>
#include <math.h>
#include <GL/gl.h>
#include <audacious/configdb.h>
#include "iris.h"


static struct
{
  gboolean proportional;
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


iris_theme theme_waves = {
  "Waves",
  "Pulsating waves",
  "Marinus Schraal (foser@sesmar.eu.org)",
  "waves",
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


GLfloat dataSquare[NUM_BANDS][NUM_BANDS];	// internal sounddata structure


static GLfloat
get_x_angle ()
{
  return (10.0 + (int) (80.0 * rand () / (RAND_MAX + 1.0)));
}


static void
draw_one_frame (gboolean beat)
{
  int t1, t2;			// loop vars
  GLfloat x1, y1;
  GLfloat red, green, blue;
  GLfloat scale = 3;		// scales the whole field - lower is bigger
  GLfloat scaleh = 2.5;		// scales height of the bar - lower is smaller
  GLfloat maxfalloff = 0.05;
  GLfloat intensity = 0.5;

  /* internal routine to shift all data when a new datarow arrives */
  for (t1 = 15; t1 > 0; t1--)
    {
      for (t2 = 0; t2 < 16; t2++)
	{
	  dataSquare[t1][t2] = dataSquare[t1 - 1][t2];
	}
    }
  for (t2 = 0; t2 < 16; t2++)
    {
      if (dataSquare[0][t2] > datas.data1[t2]
	  && ((dataSquare[0][t2] - datas.data1[t2]) > maxfalloff))
	{
	  dataSquare[0][t2] = dataSquare[0][t2] - maxfalloff;
	}
      else
	{
	  dataSquare[0][t2] = datas.data1[t2];
	}
    }

  /* some kinda random base i started out on
   * turned out to be too big thats why i added scaling vars
   */
  x1 = 7.5f;
  y1 = 7.5f;

  for (t1 = 0; t1 < 15; t1++)
    {
      x1 = 7.5f;
      for (t2 = 0; t2 < 15; t2++)
	{

	  glBegin (GL_TRIANGLES);
	  intensity = 0.75;

	  /* triangle 1 */
	  get_color (&red, &green, &blue, &dataSquare[t1 + 1][t2 + 1]);
	  glColor4f (red, green, blue, intensity);
	  glVertex3f ((x1 - 1) / scale, dataSquare[t1 + 1][t2 + 1] * scaleh, (y1 - 1) / scale);	// Top Right Of The Quad (Bottom)

	  get_color (&red, &green, &blue, &dataSquare[t1 + 1][t2]);
	  glColor4f (red, green, blue, intensity);
	  glVertex3f (x1 / scale, dataSquare[t1 + 1][t2] * scaleh, (y1 - 1) / scale);	// Top Left Of The Quad (Bottom)

	  get_color (&red, &green, &blue, &dataSquare[t1][t2]);
	  glColor4f (red, green, blue, intensity);
	  glVertex3f (x1 / scale, dataSquare[t1][t2] * scaleh, y1 / scale);	// Bottom Left Of The Quad (Bottom)

	  /* triangle 2 */
	  get_color (&red, &green, &blue, &dataSquare[t1 + 1][t2 + 1]);
	  glColor4f (red, green, blue, intensity);
	  glVertex3f ((x1 - 1) / scale, dataSquare[t1 + 1][t2 + 1] * scaleh, (y1 - 1) / scale);	// Top Right Of The Quad (Bottom)

	  get_color (&red, &green, &blue, &dataSquare[t1][t2 + 1]);
	  glColor4f (red, green, blue, intensity);
	  glVertex3f ((x1 - 1) / scale, dataSquare[t1][t2 + 1] * scaleh, y1 / scale);	// Bottom Right Of The Quad (Bottom)

	  get_color (&red, &green, &blue, &dataSquare[t1][t2]);
	  glColor4f (red, green, blue, intensity);
	  glVertex3f (x1 / scale, dataSquare[t1][t2] * scaleh, y1 / scale);	// Bottom Left Of The Quad (Bottom)

	  glEnd ();

	  x1 -= 1;
	}
      y1 -= 1;
    }

  glEnd ();

}
