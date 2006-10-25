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

GLfloat jump[16] =
  { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
  0.0
};

xz xz1, xz2, xz3, xz4;

static int draw_mode = 0;	// the drawing mode

static struct
{
  int draw_mode;
  int angle_step;
  gboolean wave_on_beat;
  GLfloat wave_height;
  int wave_timer;
  GLfloat radius;
  GLfloat interband;
  GLfloat linelength;
}
conf_private, conf_private_new;

static config_theme conf = {
  (config_global *) NULL,
  &conf_private
};

static config_theme conf_new = {
  (config_global *) NULL,
  &conf_private_new
};

static char original_draw_mode[] = "original_draw_mode";
static char original_angle_step[] = "original_angle_step";
static char original_wave_on_beat[] = "original_wave_on_beat";
static char original_wave_height[] = "original_wave_height";
static char original_wave_timer[] = "original_wave_timer";
static char original_radius[] = "original_radius";
static char original_interband[] = "original_interband";
static char original_linelength[] = "original_linelength";

static void config_read (ConfigDb *, char *);
static void config_write (ConfigDb *, char *);
static void config_default (void);
static void config_create (GtkWidget *);
static void init_draw_mode (void);
static GLfloat get_x_angle (void);
static void draw_one_frame (gboolean);

iris_theme theme_original = {
  "Original",
  "A 360 degrees spectrum analyzer",
  "Cédric Delfosse (cdelfosse@free.fr)",
  "original",
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
  draw_one_frame
};


static void
shift_jump ()
{
  int i;

  for (i = 15; i > 0; i--)
    jump[i] = jump[i - 1];
  jump[0] = 0.0;
}


static void
init_draw_mode ()
{
  draw_mode = 1 + (int) (3.0 * rand () / (RAND_MAX + 1.0));
}


static GLfloat
get_x_angle ()
{
  if (draw_mode == 3)
    return (45.0 + (int) (30.0 * rand () / (RAND_MAX + 1.0)));
  else
    return (55.0 + (int) (35.0 * rand () / (RAND_MAX + 1.0)));
}


static void
draw_one_frame (gboolean beat)
{
  static unsigned int cpt = 0;
  static float a = 1.0;
  static float b = 1.0;
  int d = 0;
  float step;
  float rad = (float) ((2 * M_PI * d) / 360);
  float cosv = a * (float) cos (rad);
  float sinv = b * (float) sin (rad);

  if ((conf_private.wave_on_beat) && (beat))
    jump[0] = conf_private.wave_height;


  glBegin (GL_QUADS);
  for (d = 0, step = 0; d < 360; d = d + conf_private.angle_step)
    {
      int l;
      float rad2 = (float) ((2 * M_PI * (d + conf_private.angle_step)) / 360);
      float cosv2 = a * (float) cos (rad2);
      float sinv2 = b * (float) sin (rad2);

      for (l = 0; l < 16; l++)
	{
	  int i;
	  GLfloat red, green, blue;
	  xz xz1, xz2, xz3, xz4;

	  GLfloat y;
	  GLfloat u = (float) (l * conf_private.interband * cosv);
	  GLfloat v = (float) (l * conf_private.interband * sinv);
	  GLfloat u2 = (float) (l * conf_private.interband * cosv2);
	  GLfloat v2 = (float) (l * conf_private.interband * sinv2);

	  xz1.x = u + (conf_private.radius + conf_private.linelength) * cosv;
	  xz1.z = v + (conf_private.radius + conf_private.linelength) * sinv;
	  xz2.x = u + conf_private.radius * cosv;
	  xz2.z = v + conf_private.radius * sinv;
	  xz3.x =
	    u2 + (conf_private.radius + conf_private.linelength) * cosv2;
	  xz3.z =
	    v2 + (conf_private.radius + conf_private.linelength) * sinv2;
	  xz4.x = u2 + conf_private.radius * cosv2;
	  xz4.z = v2 + conf_private.radius * sinv2;

	  // calculate the height of the bar
	  // this makes a fade with the past drawn values
	  for (i = 0, y = 0; i < conf_private.angle_step; i++)
	    y += datas.data360[d + i][l];
	  y /= (float) conf_private.angle_step;

	  // get the color associated with the height of the bar
	  if (y != 0.0)
	    get_color (&red, &green, &blue, &y);
	  else
	    break;		// we draw nothing if the signal is null

	  y *= 4;		// should be in the config ?

	  if (conf_private.wave_on_beat)
	    y += jump[l];

	  switch (draw_mode)
	    {
	    case 1:
	      // draw a bar
	      {
		glColor4f (red / 2, green / 2, blue / 2, 0.3);
		bar_side (y, &xz1, &xz2);
		bar_side (y, &xz3, &xz4);
		bar_side (y, &xz1, &xz3);
		bar_side (y, &xz2, &xz4);
		if (jump[l])
		  glColor4f (config.color_flash_red, config.color_flash_green,
			     config.color_flash_blue, 0.3);
		else
		  glColor4f (red, green, blue, 0.3);
		bar_top_or_bottom (y, &xz1, &xz2, &xz3, &xz4);

		break;
	      }
	    case 2:
	      // draw only the top of a bar
	      {
		if (jump[l])
		  glColor4f (config.color_flash_red, config.color_flash_green,
			     config.color_flash_blue, 0.3);
		else
		  glColor4f (red, green, blue, 0.3);
		bar_top_or_bottom (y, &xz1, &xz2, &xz3, &xz4);

		break;
	      }
	    case 3:
	      // draw only a side of a bar
	      {
		if (jump[l])
		  glColor4f (config.color_flash_red, config.color_flash_green,
			     config.color_flash_blue, 0.3);
		else
		  glColor4f (red, green, blue, 0.3);
		bar_side (y, &xz3, &xz4);

		break;
	      }
	    case 4:
	      //draw only another side of a bar
	      {
		if (jump[l])
		  glColor4f (config.color_flash_red, config.color_flash_green,
			     config.color_flash_blue, 0.3);
		else
		  glColor4f (red, green, blue, 0.3);
		bar_side (y, &xz1, &xz3);

		break;
	      }

	    }			// switch

	}			// for(l=0

      rad = rad2;
      cosv = cosv2;
      sinv = sinv2;

    }				// for(d=0

  if (!cpt)
    {
      shift_jump ();
      cpt = conf_private.wave_timer;
    }
  cpt--;

  glEnd ();

}


static void
config_read (ConfigDb * db, char *section_name)
{
  bmp_cfg_db_get_int (db, section_name, original_draw_mode, &conf_private.draw_mode);
  bmp_cfg_db_get_int (db, section_name, original_angle_step, &conf_private.angle_step);
  bmp_cfg_db_get_bool (db, section_name, original_wave_on_beat, &conf_private.wave_on_beat);
  bmp_cfg_db_get_float (db, section_name, original_wave_height, &conf_private.wave_height);
  bmp_cfg_db_get_int (db, section_name, original_wave_timer, &conf_private.wave_timer);
  bmp_cfg_db_get_float (db, section_name, original_radius, &conf_private.radius);
  bmp_cfg_db_get_float (db, section_name, original_interband, &conf_private.interband);
  bmp_cfg_db_get_float (db, section_name, original_linelength, &conf_private.linelength);
}


static void
config_write (ConfigDb * db, char *section_name)
{
  bmp_cfg_db_set_int (db, section_name, original_draw_mode, conf_private.draw_mode);
  bmp_cfg_db_set_int (db, section_name, original_angle_step, conf_private.angle_step);
  bmp_cfg_db_set_bool (db, section_name, original_wave_on_beat, conf_private.wave_on_beat);
  bmp_cfg_db_set_float (db, section_name, original_wave_height, conf_private.wave_height);
  bmp_cfg_db_set_int (db, section_name, original_wave_timer, conf_private.wave_timer);
  bmp_cfg_db_set_float (db, section_name, original_radius, conf_private.radius);
  bmp_cfg_db_set_float (db, section_name, original_interband, conf_private.interband);
  bmp_cfg_db_set_float (db, section_name, original_linelength, conf_private.linelength);
}


static void
config_default ()
{
  conf_private.draw_mode = 2;
  conf_private.angle_step = 8;
  conf_private.wave_on_beat = TRUE;
  conf_private.wave_height = 0.5;
  conf_private.wave_timer = 5;
  conf_private.radius = 0.4;
  conf_private.interband = 0.25;
  conf_private.linelength = 0.11;
}


void
jumpbeat_toggled (GtkWidget * widget, gpointer data)
{
  conf_private_new.wave_on_beat = !conf_private_new.wave_on_beat;
}


static void
value_jump_speed (GtkAdjustment * adj)
{
  conf_private_new.wave_timer = (int) adj->value;
}


static void
config_create (GtkWidget * vbox)
{
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *label;
  GtkObject *adjustment;
  GtkWidget *hscale;

  memcpy (&conf_private_new, &conf_private, sizeof (conf_private_new));

  /* wave on beat */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  button = gtk_check_button_new_with_label ("Wave on beats");
  gtk_widget_show (button);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 4);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				conf_private_new.wave_on_beat);
  gtk_signal_connect (GTK_OBJECT (button), "toggled",
		      GTK_SIGNAL_FUNC (jumpbeat_toggled), NULL);

  /* number of frame for the wave to propagate */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Wave propagation timer (in frames)");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  adjustment =
    gtk_adjustment_new (conf_private_new.wave_timer, 1, 50, 1, 5, 0);
  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  gtk_widget_set_usize (GTK_WIDGET (hscale), 200, 25);
  gtk_box_pack_start (GTK_BOX (hbox), hscale, FALSE, FALSE, 4);
  gtk_widget_show (hscale);
  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      GTK_SIGNAL_FUNC (value_jump_speed), NULL);
}
