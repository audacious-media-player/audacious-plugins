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

/* New Theme created by Ron Lockwood-Childs
 * Looks best when alpha blending is on
 */

/* $Id: theme_pipes.c,v 1.5 2002/05/16 20:38:17 cedric Exp $ */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>
#include <audacious/configdb.h>
#include <stdio.h>
#include "iris.h"

#define NUM_BANDS 16
#define REPEAT 5
#define CIRCLE_POINTS 12
#define PIPE_DIAMETER 0.25f
#define PIPE_THICKNESS 0.10f
#define PIPE_ANGLE (45.0f / (float)NUM_BANDS)
#define PIPE_RING_ANGLE (300.0f / (float)NUM_BANDS)
#define PIPE_RING_RADIUS(side) (side / (2.0f * sin( PIPE_RING_ANGLE / 2.0f )))


static struct
{
  gfloat slope;
  gfloat length;
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
static void config_read (ConfigDb *, char *);
static void config_write (ConfigDb *, char *);
static void config_default (void);
static void config_create (GtkWidget *);


static char pipes_slope[] = "pipes_slope";
static char pipes_length[] = "pipes_length";


iris_theme theme_pipes = {
  "Pipes",
  "Coral spectrum",
  "Ron Lockwood-Childs",
  "pipes",
  &conf,
  &conf_new,
  sizeof (conf_private),
  config_read,
  config_write,
  config_default,
  config_create,
  NULL,
  NULL,
  NULL,
  get_x_angle,
  draw_one_frame,
};


typedef struct
{
  GLfloat x_n;
  GLfloat z_n;
}
rectangular;


GLfloat data2[NUM_BANDS];	// previous freq band data


static GLfloat
get_x_angle ()
{
  return (55.0 + (int) (35.0 * rand () / (RAND_MAX + 1.0)));
}


static void
draw_one_frame (gboolean beat)
{
  int t1, t2, t3;		// loop vars
  GLfloat red, green, blue;
  GLfloat y = 0.0;
//  GLfloat circumference;      // loop var around cylinder
  // store each set of points around the center of the cylinder
  rectangular outer_cylinder[CIRCLE_POINTS];	// each y, z around outer circle
  rectangular inner_cylinder[CIRCLE_POINTS];	// each y, z around inner circle
  rectangular ring_of_pipes[NUM_BANDS];	// for wrapping pipes in a circle
  GLfloat differ;		// holds the angle increment
  GLfloat maxdropoff = 0.05;	// smooth changes in heights
  GLfloat angle_step;
//  GLfloat adj_x;              // bar height adjustment
  GLfloat repeat_index_adj;	// adj for repeating NUM_BANDS bars around spiral
  int next_point;
  static int first_time = 0;

  for (t1 = 0, angle_step = 0;
       t1 < CIRCLE_POINTS;
       t1++, angle_step += ((M_PI * 2) / (float) CIRCLE_POINTS))
    {				// get each point of each "circle" (more like a polygon)
      outer_cylinder[t1].x_n = cos (angle_step) * (PIPE_DIAMETER / 2.0f);
      outer_cylinder[t1].z_n = sin (angle_step) * (PIPE_DIAMETER / 2.0f);
      inner_cylinder[t1].x_n = cos (angle_step) *
	((PIPE_DIAMETER - PIPE_THICKNESS) / 2.0f);
      inner_cylinder[t1].z_n = sin (angle_step) *
	((PIPE_DIAMETER - PIPE_THICKNESS) / 2.0f);
    }
  for (t1 = 0; t1 < NUM_BANDS; t1++)
    {
      // smooth out the pipe length changes
      differ = datas.data1[t1] - data2[t1];	// calculate change in amplitude
      if (fabs (differ) > maxdropoff)
	{
	  if (differ > 0)
	    data2[t1] += maxdropoff;
	  else if (differ < 0)
	    data2[t1] -= maxdropoff;
	}
      else
	{
	  data2[t1] += differ;
	}
      // work out the pipe placement
      ring_of_pipes[t1].x_n = cos (t1 * PIPE_RING_ANGLE) * 2;
      ring_of_pipes[t1].z_n = sin (t1 * PIPE_RING_ANGLE) * 2;
    }

  for (t3 = 0; t3 < REPEAT; t3++)
    {
      for (t1 = 0; t1 < NUM_BANDS; t1++)
	{			// iterate thru each pipe

	  glPushMatrix ();
	  repeat_index_adj = t1 + (t3 * NUM_BANDS);
	  // now alter the model view matrix to put the pipe in the right position
	  glRotatef (PIPE_RING_ANGLE * repeat_index_adj, 0.0f, 0.5f, 0.0f);
	  glTranslatef (0.5f + (0.05f * repeat_index_adj),
			(conf_private.slope * repeat_index_adj), 0);
	  glRotatef (-1 * PIPE_ANGLE * repeat_index_adj, 0.0f, 0.0f, 1.0f);

	  glBegin (GL_QUADS);
	  // draw each cylinder segment, from one end of the pipe to the other
	  for (t2 = 0; t2 < CIRCLE_POINTS; t2++)
	    {
	      if (t2 >= (CIRCLE_POINTS - 1))
		next_point = 0;
	      else
		next_point = t2 + 1;
	      // current angle to angle + PI / # of points
	      // can draw both inner and outer cylinder segments per angle
	      // also: split pipe height so we can do 
	      // color 2 -> color 1 -> color 2 along its height

	      // top half of outer pipe segment
	      get_color (&red, &green, &blue, &data2[t1]);	// top color
	      glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	      // "top-left"
	      glVertex3f (outer_cylinder[t2].x_n,
			  data2[t1] * conf_private.length,
			  outer_cylinder[t2].z_n);

	      get_color (&red, &green, &blue, &y);	// bottom color
	      glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	      // "middle-left"
	      glVertex3f (outer_cylinder[t2].x_n,
			  0.0f, outer_cylinder[t2].z_n);
	      // "middle-right"
	      glVertex3f (outer_cylinder[next_point].x_n,
			  0.0f, outer_cylinder[next_point].z_n);

	      get_color (&red, &green, &blue, &data2[t1]);	// top color
	      glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	      // "top-right"
	      glVertex3f (outer_cylinder[next_point].x_n,
			  data2[t1] * conf_private.length,
			  outer_cylinder[next_point].z_n);

	      // top half of inner pipe segment
	      get_color (&red, &green, &blue, &data2[t1]);	// top color
	      glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	      // "top-left"
	      glVertex3f (inner_cylinder[t2].x_n,
			  data2[t1] * conf_private.length,
			  inner_cylinder[t2].z_n);

	      get_color (&red, &green, &blue, &y);	// bottom color
	      glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	      // "middle-left"
	      glVertex3f (inner_cylinder[t2].x_n,
			  0.0f, inner_cylinder[t2].z_n);
	      // "middle-right"
	      glVertex3f (inner_cylinder[next_point].x_n,
			  0.0f, inner_cylinder[next_point].z_n);

	      get_color (&red, &green, &blue, &data2[t1]);	// top color
	      glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	      // "top-right"
	      glVertex3f (inner_cylinder[next_point].x_n,
			  data2[t1] * conf_private.length,
			  inner_cylinder[next_point].z_n);

	      // now "cap" the ends of the pipe

	      // cap "top" of pipe
	      get_color (&red, &green, &blue, &data2[t1]);	// top color
	      glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	      // "outer-bottom"
	      glVertex3f (outer_cylinder[t2].x_n,
			  data2[t1] * conf_private.length,
			  outer_cylinder[t2].z_n);
	      // "inner-bottom"
	      glVertex3f (inner_cylinder[t2].x_n,
			  data2[t1] * conf_private.length,
			  inner_cylinder[t2].z_n);
	      // "inner-top"
	      glVertex3f (inner_cylinder[next_point].x_n,
			  data2[t1] * conf_private.length,
			  inner_cylinder[next_point].z_n);
	      // "outer-top"
	      glVertex3f (outer_cylinder[next_point].x_n,
			  data2[t1] * conf_private.length,
			  outer_cylinder[next_point].z_n);

	      // cap "bottom" of pipe
	      get_color (&red, &green, &blue, &y);	// bottom color
	      glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	      // "outer-bottom"
	      glVertex3f (outer_cylinder[t2].x_n,
			  0.0f, outer_cylinder[t2].z_n);
	      // "inner-bottom"
	      glVertex3f (inner_cylinder[t2].x_n,
			  0.0f, inner_cylinder[t2].z_n);
	      // "inner-top"
	      glVertex3f (inner_cylinder[next_point].x_n,
			  0.0f, inner_cylinder[next_point].z_n);
	      // "outer-top"
	      glVertex3f (outer_cylinder[next_point].x_n,
			  0.0f, outer_cylinder[next_point].z_n);
	    }
	  glEnd ();		// specify each pipe separately
	  glPopMatrix ();
	}
    }


  first_time++;
}


static void
config_read (ConfigDb * db, char *section_name)
{
  bmp_cfg_db_get_float (db, section_name, pipes_slope, &conf_private.slope);
  bmp_cfg_db_get_float (db, section_name, pipes_length, &conf_private.length);
}


static void
config_write (ConfigDb * db, char *section_name)
{
  bmp_cfg_db_set_float (db, section_name, pipes_slope, conf_private.slope);
  bmp_cfg_db_set_float (db, section_name, pipes_length, conf_private.length);
}


static void
config_default ()
{
  conf_private.slope = -0.05;
  conf_private.length = 1.5;
}


static void
slope_changed (GtkAdjustment * adj)
{
  conf_private_new.slope = (float) adj->value;
}


static void
length_changed (GtkAdjustment * adj)
{
  conf_private_new.length = (float) adj->value;
}


static void
config_create (GtkWidget * vbox)
{
  GtkWidget *hbox;
  GtkWidget *label;
  GtkObject *adjustment;
  GtkWidget *hscale;

  memcpy (&conf_private_new, &conf_private, sizeof (conf_private));

  /* slope in vertical direction */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Rate of descent/ascent (vertical slope)");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  adjustment =
    gtk_adjustment_new (conf_private_new.slope, -0.05, 0.05, 0.001, 0.01, 0);
  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
  gtk_scale_set_digits (GTK_SCALE (hscale), 2);
  gtk_widget_set_usize (GTK_WIDGET (hscale), 200, 25);
  gtk_box_pack_start (GTK_BOX (hbox), hscale, FALSE, FALSE, 4);
  gtk_widget_show (hscale);
  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      GTK_SIGNAL_FUNC (slope_changed), NULL);

  /* length of each pipe */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Pipe length");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  adjustment =
    gtk_adjustment_new (conf_private_new.length, 0.5, 3.5, 0.1, 0.5, 0);
  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
  gtk_scale_set_digits (GTK_SCALE (hscale), 1);
  gtk_widget_set_usize (GTK_WIDGET (hscale), 200, 25);
  gtk_box_pack_start (GTK_BOX (hbox), hscale, FALSE, FALSE, 4);
  gtk_widget_show (hscale);
  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      GTK_SIGNAL_FUNC (length_changed), NULL);
}
