/*  Iris - visualization plugin for XMMS
 *  Copyright (C) 2004 Marc Pompl (marc.pompl@lynorics.de)
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
#include <stdio.h>
#include <math.h>
#include <GL/gl.h>
#include <audacious/configdb.h>
#include "iris.h"
#include "particle.h"

/* Max number of particles */
#define MAX_PARTICLES 10000

static GLuint texture[1];	/* Storage For Our Particle Texture                   */

static int draw_mode = 0;	// the drawing mode

static GLuint col = 0;		/* Current Color Selection                            */

static gboolean fountain_already_initialized = FALSE;

static float flash_timer = 0;	/* timer for flash */

static struct
{
  int draw_mode;
  gboolean flash_on_beat;
  int flash_timer;

  gboolean rainbow;		/* Toggle rainbow effect                              */

  GLfloat slowdown;		/* Slow Down Particles (1.0 - 4.0) */
  GLfloat xspeed;		/* Base X Speed */
  GLfloat yspeed;		/* Base Y Speed */
  GLfloat up;			/* Particles firing upways (0.0 - 1.0) */
  GLfloat side;			/* Particles firing sideways (0.0 - 1.0) */
  GLfloat strength;		/* Strength of particle blast (0.0 - 1.0) */
  gboolean hide_inactive;	/* Disable inactive ("just falling") particles? */
  GLfloat size;			/* Size of particles */
  int particles;		/* number of particles */
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

static char fountain_flash_on_beat[] = "fountain_flash_on_beat";
static char fountain_flash_timer[] = "fountain_flash_timer";
static char fountain_rainbow[] = "fountain_rainbow";
static char fountain_slowdown[] = "fountain_slowdown";
static char fountain_xspeed[] = "fountain_xspeed";
static char fountain_yspeed[] = "fountain_yspeed";
static char fountain_up[] = "fountain_up";
static char fountain_side[] = "fountain_side";
static char fountain_strength[] = "fountain_strength";
static char fountain_hide_inactive[] = "fountain_hide_inactive";
static char fountain_size[] = "fountain_size";
static char fountain_particles[] = "fountain_particles";

static void config_read (ConfigDb *, char *);
static void config_write (ConfigDb *, char *);
static void config_default (void);
static void config_create (GtkWidget *);
static void init_draw_mode (void);
static GLfloat get_x_angle (void);
static void draw_one_frame (gboolean);

iris_theme theme_fountain = {
  "fountain",
  "an atmospheric particle fountain\nno support  for wireframe or solid surfaces\nbased upon NeHe tutorial #19",
  "Marc Pompl (marc.pompl@lynorics.de)",
  "fountain",
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

/* Create our particle structure */
typedef struct
{
  GLint active;			/* Active (Yes/No) */
  GLfloat life;			/* Particle Life   */
  GLfloat fade;			/* Fade Speed      */

  GLfloat r;			/* Red Value       */
  GLfloat g;			/* Green Value     */
  GLfloat b;			/* Blue Value      */

  GLfloat fr;			/* Red Flash Value       */
  GLfloat fg;			/* Green Flash Value     */
  GLfloat fb;			/* Blue Flash Value      */

  GLfloat x;			/* X Position      */
  GLfloat y;			/* Y Position      */
  GLfloat z;			/* Z Position      */

  GLfloat xi;			/* X Direction     */
  GLfloat yi;			/* Y Direction     */
  GLfloat zi;			/* Z Direction     */

  GLfloat xg;			/* X Gravity       */
  GLfloat yg;			/* Y Gravity       */
  GLfloat zg;			/* Z Gravity       */
} particle;

/* Rainbow of colors */
static GLfloat fountain_colors[16][3] = {
  {1.0f, 0.5f, 0.5f},
  {1.0f, 0.75f, 0.5f},
  {1.0f, 1.0f, 0.5f},

  {0.75f, 1.0f, 0.5f},

  {0.5f, 1.0f, 0.5f},
  {0.5f, 1.0f, 0.75f},
  {0.5f, 1.0f, 1.0f},

  {0.5f, 0.75f, 1.0f},
  {0.5f, 0.5f, 1.0f},
  {0.75f, 0.5f, 1.0f},

  {1.0f, 0.5f, 1.0f},
  {1.0f, 0.5f, 0.75f},
  {0.75f, 0.5f, 0.75f},

  {0.5f, 0.5f, 0.5f},
  {0.75f, 0.75f, 0.75f},
  {1.0f, 1.0f, 1.0f},
};

/* Our beloved array of particles */
static particle particles[MAX_PARTICLES];

void
flashParticleColor (int num)
{
  GLfloat dr;
  GLfloat dg;
  GLfloat db;
  dr = 1 - particles[num].r;
  dg = 1 - particles[num].g;
  db = 1 - particles[num].b;
  particles[num].fr = 1 - dr * dr;
  particles[num].fg = 1 - dg * dg;
  particles[num].fb = 1 - db * db;
}

/* function to "load" a GL texture */
void
loadTexture ()
{
  /* Create The Texture */
  glGenTextures (1, &texture[0]);

  /* Typical Texture Generation Using Data From The Bitmap */
  glBindTexture (GL_TEXTURE_2D, texture[0]);

  /* Generate The Texture */
  glTexImage2D (GL_TEXTURE_2D, 0, 3, particle_image.width,
		particle_image.height, 0, GL_RGBA,
		GL_UNSIGNED_BYTE, particle_image.pixel_data);

  /* Linear Filtering */
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

/* function to reset one particle to initial state */
void
ResetParticle (int num, GLint color, GLfloat xDir, GLfloat yDir, GLfloat zDir)
{
  /* Make the particels active */
  particles[num].active = TRUE;
  /* Give the particles life */
  particles[num].life = 1.0f;
  /* Random Fade Speed */
  particles[num].fade =
    (GLfloat) (rand () * 150) / (RAND_MAX + 1.0) / 1000.0f + 0.003f;
  /* Select Red Rainbow Color */
  particles[num].r = fountain_colors[color][0];
  /* Select Green Rainbow Color */
  particles[num].g = fountain_colors[color][1];
  /* Select Blue Rainbow Color */
  particles[num].b = fountain_colors[color][2];
  /* Set the position on the X axis */
  particles[num].x = 0.0f;
  /* Set the position on the Y axis */
  particles[num].y = 0.0f;
  /* Set the position on the Z axis */
  particles[num].z = 0.0f;
  /* Random Speed On X Axis */
  particles[num].xi = xDir;
  /* Random Speed On Y Axi */
  particles[num].yi = yDir;
  /* Random Speed On Z Axis */
  particles[num].zi = zDir;
  /* Set Horizontal Pull To Zero */
  particles[num].xg = 0.0f;
  /* Set Vertical Pull Downward */
  particles[num].yg = -0.8f;
  /* Set Pull On Z Axis To Zero */
  particles[num].zg = 0.0f;
  flashParticleColor (num);
}

static void
initParticle (int num, GLint color, GLfloat value)
{
  GLfloat xi, yi, zi;
  xi = conf_private.xspeed +
    (GLfloat) ((rand () * 60 - 30.0f) / (RAND_MAX +
					 1.0) * 500 * conf_private.side *
	       (value + conf_private.strength));
  yi =
    conf_private.yspeed +
    (GLfloat) ((rand () * 100) / (RAND_MAX + 1.0) * 500 * conf_private.up *
	       (value + conf_private.strength));
  zi =
    (GLfloat) ((rand () * 60 - 30.0f) / (RAND_MAX +
					 1.0) * 500 * conf_private.side *
	       (value + conf_private.strength));
  ResetParticle (num, color, xi, yi, zi);
}

static void
init_draw_mode ()
{
  GLfloat ratio;
  int loop;

  conf.global->transparency = TRUE;
  conf.global->wireframe = FALSE;

  draw_mode = 1 + (int) (3.0 * rand () / (RAND_MAX + 1.0));

  ratio = (GLfloat) GLWin.window_width / (GLfloat) GLWin.window_height;

  loadTexture ();
  /* Enable smooth shading */
  glShadeModel (GL_SMOOTH);

  /* Set the background black */
  glClearColor (0.0f, 0.0f, 0.0f, 0.0f);

  /* Depth buffer setup */
  glClearDepth (1.0f);

  /* Enables Depth Testing */
  glDisable (GL_DEPTH_TEST);

  /* Enable Blending */
  glEnable (GL_BLEND);

  glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

  /* Type Of Blending To Perform */
  glBlendFunc (GL_SRC_ALPHA, GL_ONE);

  /* Really Nice Perspective Calculations */
  glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
  /* Really Nice Point Smoothing */
  glHint (GL_POINT_SMOOTH_HINT, GL_NICEST);

  /* Enable Texture Mapping */
  glEnable (GL_TEXTURE_2D);
  /* Select Our Texture */
  glBindTexture (GL_TEXTURE_2D, texture[0]);

  /* Reset all the particles */
  if (!fountain_already_initialized)
    {
      for (loop = 0; loop < MAX_PARTICLES; loop++)
	{
	  GLint color = (loop + 1) / (MAX_PARTICLES / 16);
	  initParticle (loop, color, (rand () * 0.5f) / (RAND_MAX + 1.0));
	}
    }
  fountain_already_initialized = TRUE;
}


static GLfloat
get_x_angle ()
{
  return (5.0 + (int) (30.0 * rand () / (RAND_MAX + 1.0)));
}

static void
draw_one_frame (gboolean beat)
{
  int loop;
  if ((beat) && (conf_private.flash_on_beat))
    flash_timer = conf_private.flash_timer;

  /* If rainbow coloring is turned on, cycle the colors */
  if (conf_private.rainbow)	// && ( delay > 25 ) )
    {
      col++;
      col = col % 16;
    }

  /* Modify each of the particles */
  for (loop = 0; loop < MAX_PARTICLES; loop++)
    {
      /* Grab Our Particle X Position */
      GLfloat x = particles[loop].x;
      /* Grab Our Particle Y Position */
      GLfloat y = particles[loop].y;
      /* Particle Z Position + Zoom */
      GLfloat z = particles[loop].z;

      if ((loop < conf_private.particles) && (particles[loop].active))
	{
	  /* Draw The Particle Using Our RGB Values,
	   * Fade The Particle Based On It's Life
	   */
	  if ((conf_private.flash_on_beat) && (flash_timer > 0))
	    glColor4f (particles[loop].fr,
		       particles[loop].fg,
		       particles[loop].fb, particles[loop].life);
	  else if (conf_private.rainbow)
	    glColor4f (particles[loop].r,
		       particles[loop].g,
		       particles[loop].b, particles[loop].life);
	  else
	    {
	      GLfloat r, g, b;
	      get_color (&r, &g, &b, &datas.data1[loop % NUM_BANDS]);
	      glColor4f (r, g, b, particles[loop].life);
	    }

	  /* Build Quad From A Triangle Strip */
	  glBegin (GL_TRIANGLE_STRIP);
	  /* Top Right */
	  glTexCoord2d (1, 1);
	  glVertex3f (x + conf_private.size, y + conf_private.size, z);
	  /* Top Left */
	  glTexCoord2d (0, 1);
	  glVertex3f (x - conf_private.size, y + conf_private.size, z);
	  /* Bottom Right */
	  glTexCoord2d (1, 0);
	  glVertex3f (x + conf_private.size, y - conf_private.size, z);
	  /* Bottom Left */
	  glTexCoord2d (0, 0);
	  glVertex3f (x - conf_private.size, y - conf_private.size, z);
	  glEnd ();

	  glBegin (GL_TRIANGLE_STRIP);
	  /* Top Right */
	  glTexCoord2d (1, 1);
	  glVertex3f (x, y + conf_private.size, z + conf_private.size);
	  /* Top Left */
	  glTexCoord2d (0, 1);
	  glVertex3f (x, y + conf_private.size, z - conf_private.size);
	  /* Bottom Right */
	  glTexCoord2d (1, 0);
	  glVertex3f (x, y - conf_private.size, z + conf_private.size);
	  /* Bottom Left */
	  glTexCoord2d (0, 0);
	  glVertex3f (x, y - conf_private.size, z - conf_private.size);
	  glEnd ();

	  glBegin (GL_TRIANGLE_STRIP);
	  /* Top Right */
	  glTexCoord2d (1, 1);
	  glVertex3f (x + conf_private.size, y, z + conf_private.size);
	  /* Top Left */
	  glTexCoord2d (0, 1);
	  glVertex3f (x + conf_private.size, y, z - conf_private.size);
	  /* Bottom Right */
	  glTexCoord2d (1, 0);
	  glVertex3f (x - conf_private.size, y, z + conf_private.size);
	  /* Bottom Left */
	  glTexCoord2d (0, 0);
	  glVertex3f (x - conf_private.size, y, z - conf_private.size);
	  glEnd ();
	}

      if (flash_timer > 0)
	{
	  /* Move On The X Axis By X Speed */
	  particles[loop].x += particles[loop].xi /
	    (conf_private.slowdown * 2000);
	  /* Move On The Y Axis By Y Speed */
	  particles[loop].y += particles[loop].yi /
	    (conf_private.slowdown * 2000);
	  /* Move On The Z Axis By Z Speed */
	  particles[loop].z += particles[loop].zi /
	    (conf_private.slowdown * 2000);
	}
      else
	{
	  /* Move On The X Axis By X Speed */
	  particles[loop].x += particles[loop].xi /
	    (conf_private.slowdown * 1000);
	  /* Move On The Y Axis By Y Speed */
	  particles[loop].y += particles[loop].yi /
	    (conf_private.slowdown * 1000);
	  /* Move On The Z Axis By Z Speed */
	  particles[loop].z += particles[loop].zi /
	    (conf_private.slowdown * 1000);
	}
      /* Take Pull On X Axis Into Account */
      particles[loop].xi += particles[loop].xg;
      /* Take Pull On Y Axis Into Account */
      particles[loop].yi += particles[loop].yg;
      /* Take Pull On Z Axis Into Account */
      particles[loop].zi += particles[loop].zg;
      /* If the particle dies, revive it */
      /* Reduce Particles Life By 'Fade' */
      particles[loop].life -= particles[loop].fade;

      if (particles[loop].life < 0.0f)
	{
	  if (conf_private.rainbow)
	    initParticle (loop, col, datas.data1[col]);
	  else
	    initParticle (loop, col,
			  datas.
			  data1[(int)
				((rand () * NUM_BANDS) / (RAND_MAX + 1.0))]);
	  if ((conf_private.hide_inactive) && (datas.data1[col] == 0.0f))
	    particles[loop].active = FALSE;

	}
    }
  if (flash_timer > 0)
    flash_timer--;
}


static void
config_read (ConfigDb * db, char *section_name)
{
  bmp_cfg_db_get_bool (db, section_name, fountain_flash_on_beat, &conf_private.flash_on_beat);
  bmp_cfg_db_get_int (db, section_name, fountain_flash_timer, &conf_private.flash_timer);
  bmp_cfg_db_get_bool (db, section_name, fountain_rainbow, &conf_private.rainbow);
  bmp_cfg_db_get_float (db, section_name, fountain_slowdown, &conf_private.slowdown);
  bmp_cfg_db_get_float (db, section_name, fountain_xspeed, &conf_private.xspeed);
  bmp_cfg_db_get_float (db, section_name, fountain_yspeed, &conf_private.yspeed);
  bmp_cfg_db_get_float (db, section_name, fountain_up, &conf_private.up);
  bmp_cfg_db_get_float (db, section_name, fountain_side, &conf_private.side);
  bmp_cfg_db_get_float (db, section_name, fountain_strength, &conf_private.strength);
  bmp_cfg_db_get_bool (db, section_name, fountain_hide_inactive, &conf_private.hide_inactive);
  bmp_cfg_db_get_float (db, section_name, fountain_size, &conf_private.size);
  bmp_cfg_db_get_int (db, section_name, fountain_particles, &conf_private.particles);
}


static void
config_write (ConfigDb * db, char *section_name)
{
  bmp_cfg_db_set_bool (db, section_name, fountain_flash_on_beat, conf_private.flash_on_beat);
  bmp_cfg_db_set_int (db, section_name, fountain_flash_timer, conf_private.flash_timer);
  bmp_cfg_db_set_bool (db, section_name, fountain_rainbow, conf_private.rainbow);
  bmp_cfg_db_set_float (db, section_name, fountain_slowdown, conf_private.slowdown);
  bmp_cfg_db_set_float (db, section_name, fountain_xspeed, conf_private.xspeed);
  bmp_cfg_db_set_float (db, section_name, fountain_yspeed, conf_private.yspeed);
  bmp_cfg_db_set_float (db, section_name, fountain_up, conf_private.up);
  bmp_cfg_db_set_float (db, section_name, fountain_side, conf_private.side);
  bmp_cfg_db_set_float (db, section_name, fountain_strength, conf_private.strength);
  bmp_cfg_db_set_bool (db, section_name, fountain_hide_inactive, conf_private.hide_inactive);
  bmp_cfg_db_set_float (db, section_name, fountain_size, conf_private.size);
  bmp_cfg_db_set_int (db, section_name, fountain_particles, conf_private.particles);
}


static void
config_default ()
{
  conf_private.flash_on_beat = TRUE;
  conf_private.flash_timer = 8;

  conf_private.rainbow = TRUE;
  conf_private.slowdown = 2.0f;
  conf_private.xspeed = 0.0f;
  conf_private.yspeed = 1.0f;
  conf_private.up = 0.8f;
  conf_private.side = 0.2f;
  conf_private.strength = 0.1f;
  conf_private.hide_inactive = TRUE;
  conf_private.size = 0.16f;
  conf_private.particles = 1000;
}

void
flash_toggled (GtkWidget * widget, gpointer data)
{
  conf_private_new.flash_on_beat = !conf_private_new.flash_on_beat;
}

static void
value_flash (GtkAdjustment * adj)
{
  conf_private_new.flash_timer = (float) adj->value;
}

void
hide_inactive_toggled (GtkWidget * widget, gpointer data)
{
  conf_private_new.hide_inactive = !conf_private_new.hide_inactive;
}

void
rainbow_toggled (GtkWidget * widget, gpointer data)
{
  conf_private_new.rainbow = !conf_private_new.rainbow;
}

static void
value_slowdown (GtkAdjustment * adj)
{
  conf_private_new.slowdown = (float) adj->value / 100;
}

static void
value_up (GtkAdjustment * adj)
{
  conf_private_new.up = (float) adj->value / 100;
}

static void
value_side (GtkAdjustment * adj)
{
  conf_private_new.side = (float) adj->value / 100;
}

static void
value_strength (GtkAdjustment * adj)
{
  conf_private_new.strength = (float) adj->value / 100;
}

static void
value_size (GtkAdjustment * adj)
{
  conf_private_new.size = (float) adj->value / 100;
}

static void
value_particles (GtkAdjustment * adj)
{
  conf_private_new.particles = (float) adj->value;
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

  /* Number of particles */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Number of particles");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  adjustment =
    gtk_adjustment_new (conf_private_new.particles, 1, MAX_PARTICLES, 1, 10,
			0);
  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  gtk_widget_set_usize (GTK_WIDGET (hscale), 200, 25);
  gtk_box_pack_start (GTK_BOX (hbox), hscale, FALSE, FALSE, 4);
  gtk_widget_show (hscale);
  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      GTK_SIGNAL_FUNC (value_particles), NULL);

  /* wave on beat */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  button = gtk_check_button_new_with_label ("Flash on beats");
  gtk_widget_show (button);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 4);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				conf_private_new.flash_on_beat);
  gtk_signal_connect (GTK_OBJECT (button), "toggled",
		      GTK_SIGNAL_FUNC (flash_toggled), NULL);

  /* number of frame for the wave to propagate */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Flash propagation timer (in frames)");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  adjustment =
    gtk_adjustment_new (conf_private_new.flash_timer, 1, 50, 1, 5, 0);
  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  gtk_widget_set_usize (GTK_WIDGET (hscale), 200, 25);
  gtk_box_pack_start (GTK_BOX (hbox), hscale, FALSE, FALSE, 4);
  gtk_widget_show (hscale);
  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      GTK_SIGNAL_FUNC (value_flash), NULL);

  /* strength for up firing */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Slowdown");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  adjustment =
    gtk_adjustment_new (conf_private_new.slowdown * 100, 100, 400, 1, 5, 0);
  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  gtk_widget_set_usize (GTK_WIDGET (hscale), 200, 25);
  gtk_box_pack_start (GTK_BOX (hbox), hscale, FALSE, FALSE, 4);
  gtk_widget_show (hscale);
  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      GTK_SIGNAL_FUNC (value_slowdown), NULL);

  /* strength for up firing */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Strength of up firing");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  adjustment =
    gtk_adjustment_new (conf_private_new.up * 100, 1, 100, 1, 5, 0);
  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  gtk_widget_set_usize (GTK_WIDGET (hscale), 200, 25);
  gtk_box_pack_start (GTK_BOX (hbox), hscale, FALSE, FALSE, 4);
  gtk_widget_show (hscale);
  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      GTK_SIGNAL_FUNC (value_up), NULL);

  /* strength for side firing */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Strength of side firing");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  adjustment =
    gtk_adjustment_new (conf_private_new.side * 100, 1, 100, 1, 5, 0);
  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  gtk_widget_set_usize (GTK_WIDGET (hscale), 200, 25);
  gtk_box_pack_start (GTK_BOX (hbox), hscale, FALSE, FALSE, 4);
  gtk_widget_show (hscale);
  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      GTK_SIGNAL_FUNC (value_side), NULL);

  /* overall strength */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Strength");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  adjustment =
    gtk_adjustment_new (conf_private_new.strength * 100, 1, 100, 1, 5, 0);
  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  gtk_widget_set_usize (GTK_WIDGET (hscale), 200, 25);
  gtk_box_pack_start (GTK_BOX (hbox), hscale, FALSE, FALSE, 4);
  gtk_widget_show (hscale);
  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      GTK_SIGNAL_FUNC (value_strength), NULL);

  /* Toggle inactive particles */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  button = gtk_check_button_new_with_label ("Hide inactive particles");
  gtk_widget_show (button);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 4);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				conf_private_new.hide_inactive);
  gtk_signal_connect (GTK_OBJECT (button), "toggled",
		      GTK_SIGNAL_FUNC (hide_inactive_toggled), NULL);

  /* Toggle rainbow */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  button = gtk_check_button_new_with_label ("Rainbow colors");
  gtk_widget_show (button);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 4);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				conf_private_new.rainbow);
  gtk_signal_connect (GTK_OBJECT (button), "toggled",
		      GTK_SIGNAL_FUNC (rainbow_toggled), NULL);

  /* size of particles */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Size of particles");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  adjustment =
    gtk_adjustment_new (conf_private_new.size * 100, 1, 100, 1, 5, 0);
  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  gtk_widget_set_usize (GTK_WIDGET (hscale), 200, 25);
  gtk_box_pack_start (GTK_BOX (hbox), hscale, FALSE, FALSE, 4);
  gtk_widget_show (hscale);
  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      GTK_SIGNAL_FUNC (value_size), NULL);

}
