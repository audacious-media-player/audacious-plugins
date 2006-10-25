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


static char spectrum_proportional[] = "spectrum_proportional";

static GLfloat peak[16];	// where the peak values are stored

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


static void config_read (ConfigDb *, char *);
static void config_write (ConfigDb *, char *);
static void config_default (void);
static void config_create (GtkWidget *);
static void init_draw_mode (void);
static GLfloat get_x_angle (void);
static void draw_one_frame (gboolean);


iris_theme theme_spectrum = {
  "Spectrum",
  "A simple spectrum",
  "Cédric Delfosse (cdelfosse@free.fr)",
  "spectrum",
  &conf,
  &conf_new,
  sizeof (conf_private),
  config_read,
  config_write,
  config_default,
  config_create,
  NULL,
  NULL,
  init_draw_mode,
  get_x_angle,
  draw_one_frame,
};


static void
init_draw_mode ()
{
  memset (peak, 0x00, 16);
}


static GLfloat
get_x_angle ()
{
  return (10.0 + (int) (80.0 * rand () / (RAND_MAX + 1.0)));
}


static void
draw_one_frame (gboolean beat)
{
  GLfloat x1, x2;
  GLfloat bar_length = 0.30;
  GLfloat bar_distance = 0.1;
  GLfloat bar_deep = 1.5;
  GLfloat y, y2;
  GLfloat red, green, blue;
  GLfloat z = 0.0;
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

  glBegin (GL_TRIANGLES);

  x1 = -(bar_length * 16.0 + bar_distance * 15.0) / 2.0;
  x2 = x1 + bar_length;

  for (l = 0; l < 16; l++)
    {
      GLfloat width;
      y = data[l] * 4;

      if (peak[l] < data[l])
	{
	  peak[l] = data[l];
	  if (beat)
	    peak[l] += 0.2;
	}
      else
	peak[l] -= 0.007;
      if (conf_private.proportional)
	width = peak[l];
      else
	width = bar_deep / 2;
      if (peak[l] < 0.0)
	peak[l] = 0.0;
      else
	{
	  y2 = peak[l] * 4 + 0.1;
	  get_color (&red, &green, &blue, &peak[l]);
	  glColor4f (red * 1.5, green * 1.5, blue * 1.5, 0.8);
	  glVertex3f (x1, y2, -width);
	  glVertex3f (x2, y2, -width);
	  glVertex3f (x2, y2, +width);
	  glVertex3f (x2, y2, +width);
	  glVertex3f (x1, y2, -width);
	  glVertex3f (x1, y2, +width);
	}

      if (y > 0.0)
	{
	  /* one side */
	  get_color (&red, &green, &blue, &data[l]);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f (x1, y, -width);
	  glVertex3f (x2, y, -width);

	  get_color (&red, &green, &blue, &z);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f (x1, 0, -width);
	  glVertex3f (x1, 0, -width);
	  glVertex3f (x2, 0, -width);

	  get_color (&red, &green, &blue, &data[l]);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f (x2, y, -width);

	  /* top */
	  glColor4f (red, green, blue, 0.5);
	  glVertex3f (x1, y, -width);
	  glVertex3f (x2, y, -width);
	  glVertex3f (x2, y, +width);
	  glVertex3f (x2, y, +width);
	  glVertex3f (x1, y, -width);
	  glVertex3f (x1, y, +width);

	  /* other side */
	  get_color (&red, &green, &blue, &data[l]);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f (x1, y, width);
	  glVertex3f (x2, y, width);

	  get_color (&red, &green, &blue, &z);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f (x1, 0, width);
	  glVertex3f (x1, 0, width);
	  glVertex3f (x2, 0, width);

	  get_color (&red, &green, &blue, &data[l]);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f (x2, y, width);

	   /**/ glVertex3f (x1, y, +width);
	  glVertex3f (x1, y, -width);

	  get_color (&red, &green, &blue, &z);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f (x1, 0, -width);

	  glVertex3f (x1, 0, -width);
	  glVertex3f (x1, 0, +width);

	  get_color (&red, &green, &blue, &data[l]);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f (x1, y, +width);

	   /**/ glVertex3f (x2, y, +width);
	  glVertex3f (x2, y, -width);

	  get_color (&red, &green, &blue, &z);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f (x2, 0, -width);

	  glVertex3f (x2, 0, -width);
	  glVertex3f (x2, 0, +width);

	  get_color (&red, &green, &blue, &data[l]);
	  glColor4f (red / 2, green / 2, blue / 2, 0.5);
	  glVertex3f (x2, y, +width);
	}

      x1 += bar_length + bar_distance;
      x2 += bar_length + bar_distance;

    }

  glEnd ();

}


static void
config_read (ConfigDb * db, char *section_name)
{
  bmp_cfg_db_get_bool (db, section_name, spectrum_proportional, &conf_private.proportional);
}

static void
config_write (ConfigDb * db, char *section_name)
{
  bmp_cfg_db_set_bool (db, section_name, spectrum_proportional, conf_private.proportional);
}

static void
config_default ()
{
  conf_private.proportional = TRUE;
}


static void
proportional_toggled (GtkWidget * widget, gpointer data)
{
  conf_private_new.proportional = !conf_private_new.proportional;
}


static void
config_create (GtkWidget * vbox)
{
  GtkWidget *hbox;
  GtkWidget *button;

  memcpy (&conf_private_new, &conf_private, sizeof (conf_private_new));

  /* proportional mode */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  button = gtk_check_button_new_with_label ("Proportional mode");
  gtk_widget_show (button);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 4);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				conf_private_new.proportional);
  gtk_signal_connect (GTK_OBJECT (button), "toggled",
		      GTK_SIGNAL_FUNC (proportional_toggled), NULL);
}
