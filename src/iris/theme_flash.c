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

static GLuint texture[1];     /* Storage For Our Particle Texture                   */

static gboolean flash_already_initialized = FALSE;

static float flash_timer = 0; /* timer for flash */

static struct
{
  gboolean flash_on_beat;
  int flash_timer;
  GLfloat speed;
  GLfloat r1, r2, r3;
  GLfloat m,n,o;
  GLfloat f,g,h;
  GLfloat t;
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

static char flash_flash_on_beat[] = "flashlight_flash_on_beat";
static char flash_flash_timer[] = "flashlight_flash_timer";
static char flash_speed[] = "flashlight_speed";

static void config_read (ConfigDb *, char *);
static void config_write (ConfigDb *, char *);
static void config_default (void);
static void config_create (GtkWidget *);
static void init_draw_mode (void);
static GLfloat get_x_angle (void);
static void draw_one_frame (gboolean);

iris_theme theme_flash = {
  "Flash light",
  "flash lights moving by a lissajou figure\n",
  "Marc Pompl (marc.pompl@lynorics.de)",
  "flashlight",
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
    GLint   active; /* Active (Yes/No) */
    GLfloat life;   /* Particle Life   */
    GLfloat fade;   /* Fade Speed      */

    GLfloat r;      /* Red Value       */
    GLfloat g;      /* Green Value     */
    GLfloat b;      /* Blue Value      */

    GLfloat fr;      /* Red Flash Value       */
    GLfloat fg;      /* Green Flash Value     */
    GLfloat fb;      /* Blue Flash Value      */

    GLfloat x;      /* X Position      */
    GLfloat y;      /* Y Position      */
    GLfloat z;      /* Z Position      */

    GLfloat xi;     /* X Direction     */
    GLfloat yi;     /* Y Direction     */
    GLfloat zi;     /* Z Direction     */

    GLfloat xg;     /* X Gravity       */
    GLfloat yg;     /* Y Gravity       */
    GLfloat zg;     /* Z Gravity       */
} particle;

/* Rainbow of colors */
static GLfloat flash_colors[16][3] =
{
  { 1.0f,  0.5f,  0.5f},
	{ 1.0f,  0.75f, 0.5f},
	{ 1.0f,  1.0f,  0.5f},

	{ 0.75f, 1.0f,  0.5f},

	{ 0.5f,  1.0f,  0.5f},
	{ 0.5f,  1.0f,  0.75f},
	{ 0.5f,  1.0f,  1.0f},

	{ 0.5f,  0.75f, 1.0f},
  { 0.5f,  0.5f,  1.0f},
	{ 0.75f, 0.5f,  1.0f},

	{ 1.0f,  0.5f,  1.0f},
	{ 1.0f,  0.5f,  0.75f},
	{ 0.75f, 0.5f,  0.75f},

	{ 0.5f,  0.5f,  0.5f},
	{ 0.75f, 0.75f, 0.75f},
	{ 1.0f,  1.0f,  1.0f},
};

/* Our beloved array of particles */
static particle particles[NUM_BANDS];

void FlashColor( int num )
{
	GLfloat dr;
	GLfloat dg;
	GLfloat db;
	dr = 1-particles[num].r;
	dg = 1-particles[num].g;
	db = 1-particles[num].b;
	particles[num].fr = 1-dr*dr;
	particles[num].fg = 1-dg*dg;
	particles[num].fb = 1-db*db;
}

/* function to "load" a GL texture */
void flash_loadTexture( )
{
	    /* Create The Texture */
	    glGenTextures( 1, &texture[0] );

	    /* Typical Texture Generation Using Data From The Bitmap */
	    glBindTexture( GL_TEXTURE_2D, texture[0] );

	    /* Generate The Texture */
	    glTexImage2D( GL_TEXTURE_2D, 0, 3, particle_image.width,
			  particle_image.height, 0, GL_RGBA,
			  GL_UNSIGNED_BYTE, particle_image.pixel_data );

	    /* Linear Filtering */
	    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
}

/* function to reset one particle to initial state */
void ResetFlash( int num, GLfloat xDir, GLfloat yDir, GLfloat zDir )
{
    /* Make the particels active */
    particles[num].active = TRUE;
    /* Give the particles life */
    particles[num].life = 1.0f;
    /* Random Fade Speed */
    particles[num].fade = ( GLfloat )( rand( ) *150 )/ (RAND_MAX + 1.0) / 1000.0f + 0.003f;
    /* Select Red Rainbow Color */
    particles[num].r = flash_colors[num][0];
    /* Select Green Rainbow Color */
    particles[num].g = flash_colors[num][1];
    /* Select Blue Rainbow Color */
    particles[num].b = flash_colors[num][2];
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
		FlashColor(num);
}

static void initFlash ( int num, GLfloat value )
{
	GLfloat xi, yi, zi;
	xi = num;
	yi = 0;
	zi = 0;
	ResetFlash( num, xi, yi, zi );
}

static void
init_draw_mode ()
{
	int loop;
	conf.global->transparency = TRUE;
	//conf.global->wireframe = FALSE;

	flash_loadTexture();
    /* Enable smooth shading */
    glShadeModel( GL_SMOOTH );

    /* Set the background black */
    //glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );

    /* Depth buffer setup */
    //glClearDepth( 1.0f );

    /* Enables Depth Testing */
    glDisable( GL_DEPTH_TEST );

    /* Enable Blending */
    glEnable( GL_BLEND );

			glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

		/* Type Of Blending To Perform */
    glBlendFunc( GL_SRC_ALPHA, GL_ONE );

    /* Really Nice Perspective Calculations */
    glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );
    /* Really Nice Point Smoothing */
    glHint( GL_POINT_SMOOTH_HINT, GL_NICEST );

    /* Enable Texture Mapping */
  glEnable( GL_TEXTURE_2D );
    /* Select Our Texture */
    glBindTexture( GL_TEXTURE_2D, texture[0] );
		if (!flash_already_initialized)
		{
			for (loop=0; loop<NUM_BANDS; loop++)
			{
				initFlash(loop,0);
			}
			conf_private.f = 1;
			conf_private.g = 1;
			conf_private.h = 2;
			conf_private.m = conf_private.n = conf_private.o = 0;
			conf_private.t = 0.0f;
		}
		flash_already_initialized = TRUE;
}


static GLfloat
get_x_angle ()
{
  return (15.0 + (int) (40.0 * rand () / (RAND_MAX + 1.0)));
}

static void
draw_one_frame (gboolean beat)
{
	int loop;
	if (beat)
		flash_timer = conf_private.flash_timer;
	conf_private.t += conf_private.speed;
		/* Modify each of the particles */
    for ( loop = 0; loop < NUM_BANDS; loop++ )
	{
		    GLfloat x;
		    GLfloat y;
		    GLfloat z;
				/* compute the position by a lissajou */
			GLfloat t1 = (GLfloat)((conf_private.t+loop)/2);
			conf_private.r1 = (GLfloat)(cos(t1)*sin(t1-loop)*5.0f);
			conf_private.r2 = (GLfloat)(sin(t1)*cos(t1)*5.0f);
			conf_private.r3 = (GLfloat)(cos(t1+conf_private.t)*sin(t1+loop)*5.0f);
			x = (conf_private.r1*cos(conf_private.m*t1+conf_private.f));
			y = (conf_private.r2*sin(conf_private.n*t1+conf_private.g));
			z = (conf_private.r3*sin(conf_private.o*t1+conf_private.h));

	    if (particles[loop].active)
		{
				/* Draw The Particle Using Our RGB Values,
		     * Fade The Particle Based On It's Life
		     */
				if ((conf_private.flash_on_beat) && (flash_timer>0))
		    	glColor4f( particles[loop].fr,
			       particles[loop].fg,
			       particles[loop].fb,
			       1.0f );
				else
				{
		    	glColor4f( particles[loop].r,
			       particles[loop].g,
			       particles[loop].b,
			       1.0f );
				}

				/* Build Quad From A Triangle Strip */
		    glBegin( GL_TRIANGLE_STRIP );
		      /* Top Right */
		      glTexCoord2d( 1, 1 );
		      glVertex3f( x + datas.data1[loop], y + datas.data1[loop], z );
		      /* Top Left */
		      glTexCoord2d( 0, 1 );
		      glVertex3f( x - datas.data1[loop], y + datas.data1[loop], z );
		      /* Bottom Right */
		      glTexCoord2d( 1, 0 );
		      glVertex3f( x + datas.data1[loop], y - datas.data1[loop], z );
		      /* Bottom Left */
		      glTexCoord2d( 0, 0 );
		      glVertex3f( x - datas.data1[loop], y - datas.data1[loop], z );
		    glEnd( );

		    glBegin( GL_TRIANGLE_STRIP );
		      /* Top Right */
		      glTexCoord2d( 1, 1 );
		      glVertex3f( x, y + datas.data1[loop], z+datas.data1[loop] );
		      /* Top Left */
		      glTexCoord2d( 0, 1 );
		      glVertex3f( x, y + datas.data1[loop], z-datas.data1[loop] );
		      /* Bottom Right */
		      glTexCoord2d( 1, 0 );
		      glVertex3f( x, y - datas.data1[loop], z+datas.data1[loop] );
		      /* Bottom Left */
		      glTexCoord2d( 0, 0 );
		      glVertex3f( x, y - datas.data1[loop], z-datas.data1[loop] );
		    glEnd( );

		    glBegin( GL_TRIANGLE_STRIP );
		      /* Top Right */
		      glTexCoord2d( 1, 1 );
		      glVertex3f( x+datas.data1[loop], y, z+datas.data1[loop] );
		      /* Top Left */
		      glTexCoord2d( 0, 1 );
		      glVertex3f( x+datas.data1[loop], y, z-datas.data1[loop] );
		      /* Bottom Right */
		      glTexCoord2d( 1, 0 );
		      glVertex3f( x-datas.data1[loop], y, z+datas.data1[loop] );
		      /* Bottom Left */
		      glTexCoord2d( 0, 0 );
		      glVertex3f( x-datas.data1[loop], y, z-datas.data1[loop] );
		    glEnd( );
			}
		}

	if (flash_timer>0)
		flash_timer --;
}


static void
config_read (ConfigDb * db, char *section_name)
{
	config_default();
  bmp_cfg_db_get_bool (db, section_name, flash_flash_on_beat, &conf_private.flash_on_beat);
  bmp_cfg_db_get_int (db, section_name, flash_flash_timer, &conf_private.flash_timer);
  bmp_cfg_db_get_float (db, section_name, flash_speed, &conf_private.speed);
}


static void
config_write (ConfigDb * db, char *section_name)
{
  bmp_cfg_db_set_bool (db, section_name, flash_flash_on_beat, conf_private.flash_on_beat);
  bmp_cfg_db_set_int (db, section_name, flash_flash_timer, conf_private.flash_timer);
  bmp_cfg_db_set_float (db, section_name, flash_speed, conf_private.speed);
}


static void
config_default ()
{
	conf_private.flash_on_beat = TRUE;
	conf_private.flash_timer = 8;
	conf_private.speed = 0.01f;
}

void
flash_flash_toggled (GtkWidget * widget, gpointer data)
{
  conf_private_new.flash_on_beat = !conf_private_new.flash_on_beat;
}

static void
flash_value_flash (GtkAdjustment * adj)
{
  conf_private_new.flash_timer = (int)adj->value;
}

static void
value_speed (GtkAdjustment * adj)
{
  conf_private_new.speed = (GLfloat) (adj->value/1000);
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

/* flash on beat */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  button = gtk_check_button_new_with_label ("Flash on beats");
  gtk_widget_show (button);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 4);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				conf_private_new.flash_on_beat);
  gtk_signal_connect (GTK_OBJECT (button), "toggled",
		      GTK_SIGNAL_FUNC (flash_flash_toggled), NULL);

  /* number of frame for the flash to propagate */
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
		      GTK_SIGNAL_FUNC (flash_value_flash), NULL);

  /* speed */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new ("Speed");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  adjustment =
    gtk_adjustment_new (conf_private_new.speed*1000, 1, 100, 1, 5, 0);
  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  gtk_widget_set_usize (GTK_WIDGET (hscale), 200, 25);
  gtk_box_pack_start (GTK_BOX (hbox), hscale, FALSE, FALSE, 4);
  gtk_widget_show (hscale);
  gtk_signal_connect (GTK_OBJECT (adjustment), "value_changed",
		      GTK_SIGNAL_FUNC (value_speed), NULL);
}
