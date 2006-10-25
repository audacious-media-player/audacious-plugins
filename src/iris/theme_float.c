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
 * Looks best when alpha blending is OFF
 */

/* $Id: theme_float.c,v 1.1 2002/10/27 19:21:06 cedric Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <GL/gl.h>
#include <audacious/configdb.h>
#include "iris.h"

#define HISTORY_SIZE 16
#define HALF_BOX   0.15
#define HALF_BOX_Y (HALF_BOX * 2)
#define FAR_LEFT   -3.0
#define FAR_BACK   -3.0
#define BOTTOM     -2.6


typedef struct
{
    GLfloat xc;
    GLfloat yc;
    GLfloat zc;
} triple;

static struct
{
  int wave_speed;
  gfloat num_blocks;
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
static gfloat speed_to_phase(int speed);
static void config_read (ConfigDb *, char *);
static void config_write (ConfigDb *, char *);
static void config_default (void);
static void config_create (GtkWidget *);


static char float_numblocks[] = "float_numblocks";
static char float_wavespeed[] = "float_wavespeed";


iris_theme theme_float = {
  "Floaters",
  "A spectrum of floating blocks (alpha blend off for best effect)",
  "Ron Lockwood-Childs",
  "float",
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

static GLfloat data2[NUM_BANDS][HISTORY_SIZE];	// previous freq band data
static GLfloat phase[HISTORY_SIZE];	// previous angle data

static GLfloat
get_x_angle ()
{
  return (10.0 + (int) (80.0 * rand () / (RAND_MAX + 1.0)));
}

static void
draw_one_frame (gboolean beat)
{
  int t1, t2;			// loop vars
  GLfloat red, green, blue;
  GLfloat y_color = 0.0;
  triple ulf, urf, llf, lrf, ulb, urb, llb, lrb;
  GLfloat basis[NUM_BANDS];
  int box_height;
  int i;

  /* shift all data when a new datarow arrives */
  for (t2 = (HISTORY_SIZE-1); t2 > 0; t2--)
  {
        for (t1 = 0; t1 < NUM_BANDS; t1++)
        {
            data2[t1][t2] = data2[t1][t2-1];
        }
        phase[t2] = phase[t2-1];
  }
  for (t1 = 0; t1 < NUM_BANDS; t1++)
  {
      data2[t1][0] = datas.data1[t1];
      phase[0] = phase[1] + speed_to_phase(conf_private.wave_speed);
  }

  glBegin (GL_QUADS);

  for (t2 = (HISTORY_SIZE-1); t2 >= 0; t2--)
  {
      for (t1 = 0; t1 < NUM_BANDS; t1++)
	  {
          basis[t1] = BOTTOM + sin( phase[t2] + (speed_to_phase(conf_private.wave_speed) * t1));
          box_height = (int)ceilf( data2[t1][t2] * conf_private.num_blocks );

          for (i = 0; i < box_height; i++)
          {
              if (i < (box_height-1))
              {
                  y_color = (GLfloat)i * (1.0 / (GLfloat)conf_private.num_blocks);
              }
              else
              {
                  y_color = data2[t1][t2];
              }
	          get_color (&red, &green, &blue, &y_color);	// box color
	          glColor4f (red / 2.0f, green / 2.0f, blue / 2.0f, 0.5f);

              ulf.xc = FAR_LEFT + (HALF_BOX * 3 * t1) - HALF_BOX;
              ulf.yc = basis[t1] + (HALF_BOX_Y * 3 * i) + HALF_BOX_Y;
              ulf.zc = (FAR_BACK + t2 * HALF_BOX * 3) - HALF_BOX;

              urf.xc = FAR_LEFT + (HALF_BOX * 3 * t1) + HALF_BOX;
              urf.yc = basis[t1] + (HALF_BOX_Y * 3 * i) + HALF_BOX_Y;
              urf.zc = (FAR_BACK + t2 * HALF_BOX * 3) - HALF_BOX;

              lrf.xc = FAR_LEFT + (HALF_BOX * 3 * t1) + HALF_BOX;
              lrf.yc = basis[t1] + (HALF_BOX_Y * 3 * i) - HALF_BOX_Y;
              lrf.zc = (FAR_BACK + t2 * HALF_BOX * 3) - HALF_BOX;

              llf.xc = FAR_LEFT + (HALF_BOX * 3 * t1) - HALF_BOX;
              llf.yc = basis[t1] + (HALF_BOX_Y * 3 * i) - HALF_BOX_Y;
              llf.zc = (FAR_BACK + t2 * HALF_BOX * 3) - HALF_BOX;

              ulb.xc = FAR_LEFT + (HALF_BOX * 3 * t1) - HALF_BOX;
              ulb.yc = basis[t1] + (HALF_BOX_Y * 3 * i) + HALF_BOX_Y;
              ulb.zc = (FAR_BACK + t2 * HALF_BOX * 3) + HALF_BOX;

              urb.xc = FAR_LEFT + (HALF_BOX * 3 * t1) + HALF_BOX;
              urb.yc = basis[t1] + (HALF_BOX_Y * 3 * i) + HALF_BOX_Y;
              urb.zc = (FAR_BACK + t2 * HALF_BOX * 3) + HALF_BOX;

              lrb.xc = FAR_LEFT + (HALF_BOX * 3 * t1) + HALF_BOX;
              lrb.yc = basis[t1] + (HALF_BOX_Y * 3 * i) - HALF_BOX_Y;
              lrb.zc = (FAR_BACK + t2 * HALF_BOX * 3) + HALF_BOX;

              llb.xc = FAR_LEFT + (HALF_BOX * 3 * t1) - HALF_BOX;
              llb.yc = basis[t1] + (HALF_BOX_Y * 3 * i) - HALF_BOX_Y;
              llb.zc = (FAR_BACK + t2 * HALF_BOX * 3) + HALF_BOX;

	          // now start drawin'
	          // start with the front, then left, back, right, finally the top

	          // "front"
	          glVertex3f (ulf.xc, ulf.yc, ulf.zc);	// "top-left"

	          glVertex3f (urf.xc, urf.yc, urf.zc);	// "top-right"

	          glVertex3f (lrf.xc, lrf.yc, lrf.zc);	// "bottom-right"

	          glVertex3f (llf.xc, llf.yc, llf.zc);	// "bottom-left"

	          // "back"
	          glVertex3f (ulb.xc, ulb.yc, ulb.zc);	// "top-left"

	          glVertex3f (urb.xc, urb.yc, urb.zc);	// "top-right"

	          glVertex3f (lrb.xc, lrb.yc, lrb.zc);	// "bottom-right"

	          glVertex3f (llb.xc, llb.yc, llb.zc);	// "bottom-left"

	          // "left"
              glVertex3f( llb.xc, llb.yc, llb.zc);    // "bottom-in"

              glVertex3f( llf.xc, llf.yc, llf.zc);    // "bottom-out"

              glVertex3f( ulf.xc, ulf.yc, ulf.zc);    // "top-out"

              glVertex3f( ulb.xc, ulb.yc, ulb.zc);    // "top-in"

              // "top"
              glVertex3f( ulb.xc, ulb.yc, ulb.zc);    // "left-in"

              glVertex3f( ulf.xc, ulf.yc, ulf.zc);    // "left-out"

              glVertex3f( urf.xc, urf.yc, urf.zc);    // "right-out"

              glVertex3f( urb.xc, urb.yc, urb.zc);    // "right-in"

              // "right"
              glVertex3f( urb.xc, urb.yc, urb.zc);    // "top-in"

              glVertex3f( urf.xc, urf.yc, urf.zc);    // "top-out"

              glVertex3f( lrf.xc, lrf.yc, lrf.zc);    // "bottom-out"

              glVertex3f( lrb.xc, lrb.yc, lrb.zc);    // "bottom-in"

              // "bottom"
              glVertex3f( lrb.xc, lrb.yc, lrb.zc);    // "right-in"

              glVertex3f( lrf.xc, lrf.yc, lrf.zc);    // "right-out"

              glVertex3f( llf.xc, llf.yc, llf.zc);    // "left-out"

              glVertex3f( llb.xc, llb.yc, llb.zc);    // "left-in"
          }
      }
  }

  glEnd ();

}


static void
config_read (ConfigDb * db, char *section_name)
{
  bmp_cfg_db_get_float (db, section_name, float_numblocks, &conf_private.num_blocks);
  bmp_cfg_db_get_int (db, section_name, float_wavespeed, &conf_private.wave_speed);
}


static void
config_write (ConfigDb * db, char *section_name)
{
  bmp_cfg_db_set_float (db, section_name, float_numblocks, conf_private.num_blocks);
  bmp_cfg_db_set_int (db, section_name, float_wavespeed, conf_private.wave_speed);
}


static void
config_default ()
{
  conf_private.num_blocks = 8.0;
  conf_private.wave_speed = 4;
}


static void
blocks_changed (GtkAdjustment * adj)
{
  conf_private_new.num_blocks = (int) adj->value;
}


// speeds: pi/8, pi/12, pi/16, pi/20, pi/24, pi/28, pi/32,0
static gfloat speed_to_phase(int speed)
{
  gfloat phase;

  if (speed == 0)
  {
    phase = 0;
  }
  else
  {
    phase = M_PI_4/(9-speed);
  }

  return phase;
}

static void
speed_changed (GtkWidget *menuitem, gpointer data)
{
  conf_private_new.wave_speed = GPOINTER_TO_INT(data);
}



static void
config_create (GtkWidget * vbox)
{
  GtkWidget *hbox;
  GtkWidget *label;
  GtkObject *adjustment;
  GtkWidget *hscale;
  GtkWidget *menu;
  GtkWidget *menuitem;
  GtkWidget *optionmenu;
  gchar *speeds[8] = {"0","1","2","3","4","5","6","7"};
  int i;

  memcpy (&conf_private_new, &conf_private, sizeof (conf_private));

  /* number blocks */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Max number blocks per stack");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  adjustment =
    gtk_adjustment_new (conf_private_new.num_blocks, 4, 8, 1, 2, 0);
  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  gtk_widget_set_usize (GTK_WIDGET (hscale), 200, 25);
  gtk_box_pack_start (GTK_BOX (hbox), hscale, FALSE, FALSE, 4);
  gtk_widget_show (hscale);
  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      GTK_SIGNAL_FUNC (blocks_changed), NULL);

  /* set wave speed */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Wave speed");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  menu = gtk_menu_new ();
  for (i=0; i<8; i++)
  {
    menuitem =  gtk_menu_item_new_with_label(speeds[i]);
    gtk_menu_append(GTK_MENU(menu), menuitem);
    gtk_widget_show (menuitem);
    gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
           GTK_SIGNAL_FUNC (speed_changed), GINT_TO_POINTER(i));
  }
  optionmenu = gtk_option_menu_new();
  gtk_menu_set_active(GTK_MENU(menu), conf_private.wave_speed);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(optionmenu), menu);
  gtk_box_pack_start (GTK_BOX (hbox), optionmenu, FALSE, FALSE, 4);
  gtk_widget_show (optionmenu);

}
