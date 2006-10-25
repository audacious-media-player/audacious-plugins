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

#ifndef IRIS_H

#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <audacious/configdb.h>
#include <X11/Xlib.h>
#include <X11/extensions/xf86vmode.h>

typedef struct
{

  GLfloat bgc_red;
  GLfloat bgc_green;
  GLfloat bgc_blue;

  GLfloat color_red;
  GLfloat color_green;
  GLfloat color_blue;

  GLfloat color1_red;
  GLfloat color1_green;
  GLfloat color1_blue;

  GLfloat color2_red;
  GLfloat color2_green;
  GLfloat color2_blue;

  GLfloat color_flash_red;
  GLfloat color_flash_green;
  GLfloat color_flash_blue;

  unsigned int color_mode;
  unsigned int flash_speed;
  unsigned int fps;

  // in fullscreen, we are only interested in width and height
  unsigned int fs_width;
  unsigned int fs_height;

  unsigned int window_width;
  unsigned int window_height;

  gboolean bgc_random;
  gboolean color_random;
  gboolean color12_random;
  gboolean flash_random;
  gboolean color_beat;
  gboolean change_theme_on_beat;

  gboolean fullscreen;
  gboolean wireframe;

  gboolean transition;
  gfloat trans_duration;
}
iris_config;

typedef struct
{ 
  gfloat priority; /* priority of the theme */
  int transparency; /* transparency can be off (0), on (1) or random (-1) */
  int wireframe; /* wireframe can be off (0), on (1) or random (-1) */
}
config_global; /* options implemented by all the themes */

typedef struct
{
  config_global *global;
  void *private; /* private config of the theme */
}
config_theme;

typedef struct
{
  char *name; /* the name of the theme */
  char *description; /* what the theme does */
  char *author; /* who did the theme */
  char *key; /* key used to get config items in ~/.xmms/config */
  config_theme *config; /* where the theme config is */
  config_theme *config_new; /* copy of the theme config used by the configuration widgets */
  int config_private_size; /* size of the private theme config structure (we can't get it dynamically) */
  void (*config_read) (ConfigDb * db, char *); /* read the private theme config and put it into the privateconfig */
  void (*config_write) (ConfigDb * db, char *); /* write the private config into ~/.xmms/config */
  void (*config_default) (void); /* put default values into private config */
  void (*config_create) (GtkWidget *); /* create the Gtk widget (into a vbox) to configure the theme */
  void (*init) (void); /* called once at iris init */
  void (*cleanup) (void); /* called once at iris cleanup */
  void (*init_draw_mode) (void); /* called once when iris switch to this theme */
  GLfloat (*get_x_angle) (void); /* called once when iris switch to this theme to get a camera position to move */
  void (*draw_one_frame) (gboolean beat); /* draw one frame of the theme */
}
iris_theme;

#define NUM_BANDS 16
typedef struct {
  GLfloat data360[360][NUM_BANDS];
  GLfloat data1[NUM_BANDS];
  GLfloat loudness;
}
datas_t;

typedef struct
{
  GLfloat x;
  GLfloat z;
}
xz;

/* stuff about our window grouped together */
typedef struct
{
  // X stuff
  Display *dpy;
  int screen;
  Window window;
  GLXContext ctx;
  XSetWindowAttributes attr;
  Bool fs;
  XF86VidModeModeInfo deskMode;
  unsigned int window_x, window_y;
  unsigned int window_width, window_height;
  unsigned int fs_width, fs_height;

  // rendering info
  int glxMajorVersion;
  int glxMinorVersion;
  int vidModeMajorVersion;
  int vidModeMinorVersion;
  XF86VidModeModeInfo **modes;
  int modeNum;
  unsigned int depth;
  gboolean DRI;
  gboolean DoubleBuffered;
  GList *glist;
}
GLWindow;

/* config.c */
extern GtkWidget *config_window;
extern iris_config config;
extern iris_config newconfig;
extern GLWindow GLWin;
extern datas_t datas;
extern void iris_first_init (void);
extern void iris_configure (void);
extern void iris_config_read (void);
extern void iris_config_write (iris_config *);
extern void iris_set_default_prefs (void);
extern void iris_save_window_attributes (void);
extern GLvoid init_gl (GLvoid);

/* color.c */
extern void get_color (GLfloat *, GLfloat *, GLfloat *, GLfloat *);

/* 3Dstuff.c */
extern void bar_top_or_bottom (GLfloat height, xz * xz1, xz * xz2, xz * xz3,
			       xz * xz4);
extern void bar_side (GLfloat height, xz * xz1, xz * xz2);
extern void bar_full (GLfloat height, xz * xz1, xz * xz2, xz * xz3, xz * xz4);

/* theme.c */
#define THEME_NUMBER 12
extern void theme_register (void);
extern iris_theme theme[THEME_NUMBER];
extern void theme_config_init (void);
extern void theme_config_global_default (int);
extern void theme_config_global_read (ConfigDb *, char *, int);
extern void theme_config_global_write (ConfigDb *, char *, int);
extern void theme_config_apply(int num);
extern void theme_config_global_widgets(GtkWidget *, int);
extern void theme_about(GtkWidget *, int);

/* transition.c */
extern void init_theme_transition ();
extern void theme_transition ();

#endif
