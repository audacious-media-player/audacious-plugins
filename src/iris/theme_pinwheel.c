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

/* $Id: theme_pinwheel.c,v 1.5 2002/05/16 20:38:17 cedric Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <GL/gl.h>
#include <audacious/configdb.h>
#include "iris.h"

#define NUM_PER_RING 8
#define GAP (M_PI_4)

#define RANDOM_HEIGHT_COUNTDOWN 60


static struct
{
  gboolean height_random;
  gfloat height_set;
  gfloat height;
  gfloat num_sections;
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


static char pinwheel_numsec[] = "pinwheel_numsec";
static char pinwheel_hrandom[] = "pinwheel_hrandom";
static char pinwheel_hset[] = "pinwheel_hset";


iris_theme theme_pinwheel = {
  "PinWheel",
  "A spectrum of pinwheels",
  "Ron Lockwood-Childs",
  "pinwheel",
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


static time_t time_random = 0;	// timer for random height mode

static GLfloat data2[NUM_BANDS];	// previous freq band data
static GLfloat angle[NUM_BANDS];	// previous angle data
static GLfloat radii[NUM_BANDS + 2] =
  { 0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f,
  4.0f, 4.5f, 5.0f, 5.5f, 6.0f, 6.5f, 7.0f, 7.5f,
  8.0f
};


static GLfloat
get_x_angle ()
{
  return (10.0 + (int) (80.0 * rand () / (RAND_MAX + 1.0)));
}


static void
set_height (void)
{
  time_t cur_time = time (NULL);	// current time

  if (conf_private.height_random)
    {
      /* we are in random height mode, see if it time to change height */
      if (cur_time - time_random > RANDOM_HEIGHT_COUNTDOWN)
	{
	  /* generate float  1 <= x <= 3 */
	  conf_private.height = ((gfloat) rand () * 2.0 / RAND_MAX) + 1.0;

	  /* reset timer for when to generate new random height */
	  time_random = time (NULL);
	}
    }
  else
    {
      conf_private.height = conf_private.height_set;
    }
}


static void
draw_one_frame (gboolean beat)
{
  int t1, t2;			// loop vars
  GLfloat red, green, blue;
  GLfloat z = 0.0;
  GLfloat scaler = 2.0f;	// scales the whole field - lower is bigger
  GLfloat scaleh;		// height of the bar - lower is smaller
  GLfloat xin_up, zin_up, xin_dn, zin_dn, xot_up, zot_up, xot_dn, zot_dn;
  GLfloat rin, rot;		// radius of inner and outer edges of p-gram
  GLfloat angle_up, angle_dn;	// angles of top and bottom of p-gram
  GLfloat differ;		// holds the angle increment
  GLfloat maxdropoff = 0.05;	// smooth changes in heights
  GLfloat angle_step;

  /* update bar height (could be changing randomly over time) */
  set_height ();
  scaleh = conf_private.height;

  /* internal routine to shift all data when a new datarow arrives */
  for (t1 = 0; t1 < 16; t1++)
    {
      differ = data2[t1] - datas.data1[t1];
      // Rotate counter c.w. if differ positive, c.w. if negative
      angle[t1] += (differ * M_PI_4 / 3.0f);	// max angle change = +/- PI/12
      if (angle[t1] > (2.0f * M_PI))
	{			// cap angle at 360
	  angle[t1] -= (2.0f * M_PI);
	}
      else if (angle[t1] < 0)
	{			// keep angle positive
	  angle[t1] += (2.0f * M_PI);
	}

      // smooth bar height changes
      if (datas.data1[t1] > data2[t1])
	{
	  if ((datas.data1[t1] - data2[t1]) > maxdropoff)
	    data2[t1] += maxdropoff;
	  else
	    data2[t1] += datas.data1[t1];
	}
      else if (datas.data1[t1] < data2[t1])
	{
	  if ((data2[t1] - datas.data1[t1]) > maxdropoff)
	    data2[t1] -= maxdropoff;
	  else
	    data2[t1] -= datas.data1[t1];
	}
    }

  glBegin (GL_QUADS);

  for (t1 = 0; t1 < 16; t1++)
    {				// iterate thru rings

      // calculate inner and outer radius for this ring of p-grams
      rin = (radii[t1 + 1] - ((radii[1] - radii[0]) / 2.0f)) / scaler;
      rot = (radii[t1 + 1] + ((radii[1] - radii[0]) / 2.0f)) / scaler;
      for (t2 = 0; t2 < conf_private.num_sections; t2++)
	{			// iterate thru bars in a single ring

	  angle_step =
	    angle[t1] + (t2 * 2 * M_PI / conf_private.num_sections);
	  if (angle_step > (M_PI * 2))
	    angle_step -= (M_PI * 2);
	  else if (angle_step < 0)
	    angle_step += (M_PI * 2);
	  // calculate upper and lower angles for this p-gram
	  angle_up = angle_step + (M_PI_2 / 9.0f);
	  angle_dn = angle_step - (M_PI_2 / 9.0f);
	  // now figure out all the p-gram vertices
	  xin_dn = cos (angle_dn) * rin;
	  zin_dn = sin (angle_dn) * rin;
	  xin_up = cos (angle_up) * rin;
	  zin_up = sin (angle_up) * rin;
	  xot_dn = cos (angle_dn) * rot;
	  zot_dn = sin (angle_dn) * rot;
	  xot_up = cos (angle_up) * rot;
	  zot_up = sin (angle_up) * rot;

	  // now start drawin'
	  // start with the front, then left, back, right, finally the top

	  // "front"
	  get_color (&red, &green, &blue, &data2[t1]);	// top color
	  glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	  glVertex3f (xot_dn, data2[t1] * scaleh, zot_dn);	// "top-right"

	  get_color (&red, &green, &blue, &z);	// bottom color
	  glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	  glVertex3f (xot_dn, z, zot_dn);	// "bottom-right"
	  glVertex3f (xin_dn, z, zin_dn);	// "bottom-left"

	  get_color (&red, &green, &blue, &data2[t1]);	// top color
	  glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	  glVertex3f (xin_dn, data2[t1] * scaleh, zin_dn);	// "top-left"

	  // "left"
	  get_color (&red, &green, &blue, &data2[t1]);	// top color
	  glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	  glVertex3f (xin_dn, data2[t1] * scaleh, zin_dn);	// "top-out"

	  get_color (&red, &green, &blue, &z);	// bottom color
	  glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	  glVertex3f (xin_dn, z, zin_dn);	// "bottom-out"
	  glVertex3f (xin_up, z, zin_up);	// "bottom-in"

	  get_color (&red, &green, &blue, &data2[t1]);	// top color
	  glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	  glVertex3f (xin_up, data2[t1] * scaleh, zin_up);	// "top-in"

	  // "back"
	  get_color (&red, &green, &blue, &data2[t1]);	// top color
	  glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	  glVertex3f (xin_up, data2[t1] * scaleh, zin_up);	// "top-left"

	  get_color (&red, &green, &blue, &z);	// bottom color
	  glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	  glVertex3f (xin_up, z, zin_up);	// "bottom-left"
	  glVertex3f (xot_up, z, zot_up);	// "bottom-right"

	  get_color (&red, &green, &blue, &data2[t1]);	// top color
	  glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	  glVertex3f (xot_up, data2[t1] * scaleh, zot_up);	// "top-right"

	  // "right"
	  get_color (&red, &green, &blue, &data2[t1]);	// top color
	  glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	  glVertex3f (xot_up, data2[t1] * scaleh, zot_up);	// "top-in"

	  get_color (&red, &green, &blue, &z);	// bottom color
	  glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	  glVertex3f (xot_up, z, zot_up);	// "bottom-in"
	  glVertex3f (xot_dn, z, zot_dn);	// "bottom-out"

	  get_color (&red, &green, &blue, &data2[t1]);	// top color
	  glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	  glVertex3f (xot_dn, data2[t1] * scaleh, zot_dn);	// "top-out"

	  // "top"
	  get_color (&red, &green, &blue, &data2[t1]);	// top color
	  glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);
	  glVertex3f (xot_dn, data2[t1] * scaleh, zot_dn);	// "out-right"
	  glVertex3f (xin_dn, data2[t1] * scaleh, zin_dn);	// "out-left"
	  glVertex3f (xin_up, data2[t1] * scaleh, zin_up);	// "in-left"
	  glVertex3f (xot_up, data2[t1] * scaleh, zot_up);	// "in-right"

	}
    }

  glEnd ();

}


static void
config_read (ConfigDb * db, char *section_name)
{
  bmp_cfg_db_get_float (db, section_name, pinwheel_numsec, &conf_private.num_sections);
  bmp_cfg_db_get_bool (db, section_name, pinwheel_hrandom, &conf_private.height_random);
  bmp_cfg_db_get_float (db, section_name, pinwheel_hset, &conf_private.height_set);
}


static void
config_write (ConfigDb * db, char *section_name)
{
  bmp_cfg_db_set_float (db, section_name, pinwheel_numsec, conf_private.num_sections);
  bmp_cfg_db_set_bool (db, section_name, pinwheel_hrandom, conf_private.height_random);
  bmp_cfg_db_set_float (db, section_name, pinwheel_hset, conf_private.height_set);
}


static void
config_default ()
{
  conf_private.num_sections = 8.0;
  conf_private.height_random = FALSE;
  conf_private.height_set = 2.0;
  conf_private.height = 2.0;
}


static void
sections_changed (GtkAdjustment * adj)
{
  conf_private_new.num_sections = (int) adj->value;
}


static void
height_changed (GtkAdjustment * adj)
{
  conf_private_new.height_set = (int) adj->value;
}


static void
height_toggled (GtkWidget * widget, GtkScale * height_scale)
{
  conf_private_new.height_random = !conf_private_new.height_random;
  gtk_widget_set_sensitive (GTK_WIDGET (height_scale),
			    !conf_private_new.height_random);
}


static void
config_create (GtkWidget * vbox)
{
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *label;
  GtkObject *adjustment;
  GtkWidget *hscale;

  memcpy (&conf_private_new, &conf_private, sizeof (conf_private));

  /* number sections per ring */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Number sections per ring");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  adjustment =
    gtk_adjustment_new (conf_private_new.num_sections, 4, 20, 1, 5, 0);
  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  gtk_widget_set_usize (GTK_WIDGET (hscale), 200, 25);
  gtk_box_pack_start (GTK_BOX (hbox), hscale, FALSE, FALSE, 4);
  gtk_widget_show (hscale);
  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      GTK_SIGNAL_FUNC (sections_changed), NULL);

  /* random max height */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  button = gtk_check_button_new_with_label ("Random height");
  gtk_widget_show (button);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 4);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				conf_private_new.height_random);

  /* explicitly set max height */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Maximum height");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  adjustment =
    gtk_adjustment_new (conf_private_new.height_set, 1, 3, 0.1, 1, 0);
  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
  gtk_scale_set_digits (GTK_SCALE (hscale), 1);
  gtk_widget_set_usize (GTK_WIDGET (hscale), 200, 25);
  gtk_box_pack_start (GTK_BOX (hbox), hscale, FALSE, FALSE, 4);
  gtk_widget_set_sensitive (GTK_WIDGET (hscale),
			    !conf_private_new.height_random);
  gtk_widget_show (hscale);
  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      GTK_SIGNAL_FUNC (height_changed), NULL);

  /* slider gets enabled/disabled according to random setting */
  gtk_signal_connect (GTK_OBJECT (button), "toggled",
		      GTK_SIGNAL_FUNC (height_toggled), GTK_SCALE (hscale));
}
