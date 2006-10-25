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


iris_theme theme_squarefield = {
  "SquareField",
  "Waving square field",
  "Marinus Schraal (foser@sesmar.eu.org)",
  "squarefield",
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
  GLfloat z = 0.0;
  GLfloat scale = 4.5;		// scales the whole field - lower is bigger
  GLfloat scaleh = 3;		// scales height of the bar - lower is smaller
  GLfloat maxfalloff = 0.05;

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
     turned out to be too big thats why i added scaling vars
   */
  x1 = 11.75f;
  y1 = 11.75f;

  glBegin (GL_QUADS);

  for (t1 = 0; t1 < 16; t1++)
    {
      x1 = 11.75f;
      for (t2 = 0; t2 < 16; t2++)
	{
	  /* bottom : turned off */

/*			get_color (&red, &green, &blue, &z);
			glColor4f (red/2 , green/2 , blue/2 , 0.5); 
			glVertex3f( (x1-1)/scale , 0, (y1-1)/scale);			// Top Right Of The Quad (Bottom)
			glVertex3f( x1/scale, 0, (y1-1)/scale);			// Top Left Of The Quad (Bottom)
			glVertex3f( x1/scale, 0, y1/scale);			// Bottom Left Of The Quad (Bottom)
			glVertex3f( (x1-1)/scale, 0, y1/scale);			// Bottom Right Of The Quad (Bottom)
*/

	  /* sides */
	  get_color (&red, &green, &blue, &dataSquare[t1][t2]);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f ((x1 - 1) / scale, dataSquare[t1][t2] * scaleh, (y1 - 1) / scale);	// top right (front)
	  get_color (&red, &green, &blue, &z);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f ((x1 - 1) / scale, 0, (y1 - 1) / scale);	//bottom right
	  glVertex3f (x1 / scale, 0, (y1 - 1) / scale);	//bottom left
	  get_color (&red, &green, &blue, &dataSquare[t1][t2]);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f (x1 / scale, dataSquare[t1][t2] * scaleh, (y1 - 1) / scale);	// top left

	  get_color (&red, &green, &blue, &dataSquare[t1][t2]);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f (x1 / scale, dataSquare[t1][t2] * scaleh, (y1 - 1) / scale);	// top fron (left)
	  get_color (&red, &green, &blue, &z);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f (x1 / scale, 0, (y1 - 1) / scale);	// bottom fron (left)
	  glVertex3f (x1 / scale, 0, y1 / scale);	// back bottom (left)
	  get_color (&red, &green, &blue, &dataSquare[t1][t2]);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f (x1 / scale, dataSquare[t1][t2] * scaleh, y1 / scale);	// top back (left)

	  get_color (&red, &green, &blue, &dataSquare[t1][t2]);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f ((x1 - 1) / scale, dataSquare[t1][t2] * scaleh, (y1 - 1) / scale);	// top front (right)
	  get_color (&red, &green, &blue, &z);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f ((x1 - 1) / scale, 0, (y1 - 1) / scale);	// bottom front (right)
	  glVertex3f ((x1 - 1) / scale, 0, y1 / scale);	// bottom back (right)
	  get_color (&red, &green, &blue, &dataSquare[t1][t2]);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f ((x1 - 1) / scale, dataSquare[t1][t2] * scaleh, y1 / scale);	// top back(right)

	  get_color (&red, &green, &blue, &dataSquare[t1][t2]);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f ((x1 - 1) / scale, dataSquare[t1][t2] * scaleh, y1 / scale);	// right top (back)
	  get_color (&red, &green, &blue, &z);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f ((x1 - 1) / scale, 0, y1 / scale);	// right bottom (back)
	  glVertex3f (x1 / scale, 0, y1 / scale);	// left bottom (back)
	  get_color (&red, &green, &blue, &dataSquare[t1][t2]);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f (x1 / scale, dataSquare[t1][t2] * scaleh, y1 / scale);	// left top (back)

	  /* top */
	  get_color (&red, &green, &blue, &dataSquare[t1][t2]);
	  glColor4f (red, green, blue, 0.5);
	  glVertex3f ((x1 - 1) / scale, dataSquare[t1][t2] * scaleh, (y1 - 1) / scale);	// Top Right Of The Quad (Bottom)
	  glVertex3f (x1 / scale, dataSquare[t1][t2] * scaleh, (y1 - 1) / scale);	// Top Left Of The Quad (Bottom)
	  glVertex3f (x1 / scale, dataSquare[t1][t2] * scaleh, y1 / scale);	// Bottom Left Of The Quad (Bottom)
	  glVertex3f ((x1 - 1) / scale, dataSquare[t1][t2] * scaleh, y1 / scale);	// Bottom Right Of The Quad (Bottom)

	  x1 -= 1.5;
	}
      y1 -= 1.5;
    }

  glEnd ();

}
