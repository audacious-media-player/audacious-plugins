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

#include "config.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <gtk/gtk.h>
#include <math.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/extensions/xf86vmode.h>
#include <pthread.h>
#ifdef HAVE_SCHED_SETSCHEDULER
#include <sched.h>
#endif
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include <audacious/plugin.h>
#include <audacious/util.h>
#include <audacious/beepctrl.h>
#include <audacious/configdb.h>
#include "iris.h"

#define BEAT_MAX 100

GLfloat y_angle = 45.0, y_speed = 0.5;
GLfloat x_angle = 20.0, x_speed = 0.0;
GLfloat z_angle = 0.0, z_speed = 0.0;
static GLfloat scale;
static GLfloat x_angle_wanted;

GLWindow GLWin;

/* attributes for a single buffered visual in RGBA format with at least
 * 4 bits per color and a 16 bit depth buffer */
static int attrListSgl[] = { GLX_RGBA, GLX_RED_SIZE, 4,
  GLX_GREEN_SIZE, 4,
  GLX_BLUE_SIZE, 4,
  GLX_DEPTH_SIZE, 16,
  None
};

/* attributes for a double buffered visual in RGBA format with at least
 * 4 bits per color and a 16 bit depth buffer */
static int attrListDbl[] = { GLX_RGBA, GLX_DOUBLEBUFFER,
  GLX_RED_SIZE, 4,
  GLX_GREEN_SIZE, 4,
  GLX_BLUE_SIZE, 4,
  GLX_DEPTH_SIZE, 16,
  None
};

static gboolean going = FALSE;
static gboolean initialized = FALSE;
static gboolean grabbed_pointer = FALSE, firsttime = TRUE;
static Atom wmDelete;
static pthread_t draw_thread;

datas_t datas;

static int num_bands = NUM_BANDS;
static int beat = 0;

unsigned int transition_frames = 0;
unsigned int max_transition_frames = 0;

static void iris_init (void);
static void iris_cleanup (void);
static void iris_about (void);
static void iris_playback_start (void);
static void iris_playback_stop (void);
static void iris_render_freq (gint16 data[][256]);

static float dps = 0;

gint beat_before = -1;

extern iris_config newconfig;

VisPlugin iris_vp = {
  NULL,
  NULL,
  0,
  NULL,				/* Description */
  0,
  1,
  iris_init,			/* init */
  iris_cleanup,			/* cleanup */
  iris_about,			/* about */
  iris_configure,		/* configure */
  NULL,				/* disable_plugin */
  iris_playback_start,		/* playback_start */
  iris_playback_stop,		/* playback_stop */
  NULL,				/* render_pcm */
  iris_render_freq		/* render_freq */
};


void
about_close_clicked (GtkWidget * w, GtkWidget ** window)
{
  gtk_widget_destroy (*window);
  *window = NULL;
}


void
about_closed (GtkWidget * w, GdkEvent * e, GtkWidget ** window)
{
  about_close_clicked (w, window);
}


static void
iris_about (void)
{
  static GtkWidget *window_about = NULL;
  GtkWidget *vbox, *button, *close, *label;

  if (window_about)
    return;

  window_about = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window_about), "About IRIS plugin");
  gtk_window_set_policy (GTK_WINDOW (window_about), FALSE, FALSE, FALSE);
  gtk_window_set_position (GTK_WINDOW (window_about), GTK_WIN_POS_MOUSE);
  vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER (window_about), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
  gtk_widget_show (vbox);

  label = gtk_label_new ("\n\
Iris XMMS Plugin.\n\
Copyright (C) 2001-2003, Cédric Delfosse.\n\
\n\
Email: <cdelfosse@free.fr> \n\
Homepage: <http://cdelfosse.free.fr/xmms-iris/>\n\
Development: <http://savannah.gnu.org/projects/iris/>\n\
\n\
Authors:\n\
Cédric Delfosse\n\
Marinus Schraal\n\
Ron Lockwood-Childs\n\
Ported to Audacious by Arthur Taylor\n\
\n\
This program is free software; you can redistribute it and/or modify\n\
it under the terms of the GNU General Public License as published by\n\
the Free Software Foundation; either version 2 of the License, or\n\
(at your option) any later version.\n\
\n\
This program is distributed in the hope that it will be useful,\n\
but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
GNU General Public License for more details.\n\
\n\
You should have received a copy of the GNU General Public License\n\
along with this program; if not, write to the Free Software\n\
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307\n\
USA");
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 8);
  gtk_widget_show (label);

  button = gtk_hbutton_box_new ();
  gtk_box_pack_end (GTK_BOX (vbox), button, FALSE, FALSE, 8);
  gtk_widget_show (button);

  close = gtk_button_new_with_label ("Close");
  GTK_WIDGET_SET_FLAGS (close, GTK_CAN_DEFAULT);
  gtk_window_set_default (GTK_WINDOW (window_about), close);
  gtk_hbutton_box_set_layout_default (GTK_BUTTONBOX_END);
  gtk_box_pack_end (GTK_BOX (button), close, FALSE, FALSE, 8);
  gtk_widget_show (close);

  gtk_signal_connect (GTK_OBJECT (close), "clicked",
		      GTK_SIGNAL_FUNC (about_close_clicked), &window_about);
  gtk_signal_connect (GTK_OBJECT (window_about), "delete-event",
		      GTK_SIGNAL_FUNC (about_closed), &window_about);

  gtk_widget_show (window_about);
}


/* Creates an empty cursor icon for when hovering over our window content */
void
hide_cursor ()
{
  Cursor no_ptr;
  Pixmap bm_no;
  Colormap cmap;
  XColor black, dummy;

  static unsigned char bm_no_data[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

  cmap = DefaultColormap (GLWin.dpy, DefaultScreen (GLWin.dpy));
  XAllocNamedColor (GLWin.dpy, cmap, "black", &black, &dummy);
  bm_no = XCreateBitmapFromData (GLWin.dpy, GLWin.window, bm_no_data, 8, 8);
  no_ptr =
    XCreatePixmapCursor (GLWin.dpy, bm_no, bm_no, &black, &black, 0, 0);
  XDefineCursor (GLWin.dpy, GLWin.window, no_ptr);
}


/* this function creates our window and sets it up properly */
static Window
create_window (char *title)
{
  XVisualInfo *vi;
  int i;

  /* get a connection */
  GLWin.dpy = XOpenDisplay (0);
  if (GLWin.dpy == NULL)
    g_log (NULL, G_LOG_LEVEL_CRITICAL,
	   __FILE__ ": XOpenDisplay returns NULL");
  GLWin.screen = DefaultScreen (GLWin.dpy);

  /* get an appropriate visual */
  vi = glXChooseVisual (GLWin.dpy, GLWin.screen, attrListDbl);
  if (vi == NULL)
    {
      vi = glXChooseVisual (GLWin.dpy, GLWin.screen, attrListSgl);
      GLWin.DoubleBuffered = 0;
    }
  else
    GLWin.DoubleBuffered = 1;

  if (vi == NULL)
    g_log (NULL, G_LOG_LEVEL_CRITICAL,
	   __FILE__ ": glXChooseVisual returns NULL");

  glXQueryVersion (GLWin.dpy, &GLWin.glxMajorVersion, &GLWin.glxMinorVersion);
  /* create a GLX context */
  GLWin.ctx = glXCreateContext (GLWin.dpy, vi, 0, GL_TRUE);
  if (GLWin.ctx == NULL)
    g_log (NULL, G_LOG_LEVEL_CRITICAL,
	   __FILE__ ": glXCreateContext can\'t create a rendering context");

  /* create a color map */
  GLWin.attr.colormap =
    XCreateColormap (GLWin.dpy, RootWindow (GLWin.dpy, vi->screen),
		     vi->visual, AllocNone);
  GLWin.attr.border_pixel = 0;

  if ((config.fullscreen && firsttime) || GLWin.fs)
    {
      int bestMode = 0;

      GLWin.fs = True;
      GLWin.fs_width = config.fs_width;
      GLWin.fs_height = config.fs_height;
      /* look for mode with requested resolution */
      for (i = 0; i < GLWin.modeNum; i++)
	{
	  if ((GLWin.modes[i]->hdisplay == GLWin.fs_width)
	      && (GLWin.modes[i]->vdisplay == GLWin.fs_height))
	    {
	      bestMode = i;
	    }
	}
      XF86VidModeSwitchToMode (GLWin.dpy, GLWin.screen,
			       GLWin.modes[bestMode]);
      XF86VidModeSetViewPort (GLWin.dpy, GLWin.screen, 0, 0);
      GLWin.fs_width = GLWin.modes[bestMode]->hdisplay;
      GLWin.fs_height = GLWin.modes[bestMode]->vdisplay;

      /* create a fullscreen window */
      GLWin.attr.override_redirect = True;
      GLWin.attr.event_mask =
	ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask;
      GLWin.window =
	XCreateWindow (GLWin.dpy,
		       RootWindow (GLWin.dpy, vi->screen), 0,
		       0, GLWin.fs_width, GLWin.fs_height, 0, vi->depth,
		       InputOutput, vi->visual,
		       CWBorderPixel | CWColormap |
		       CWEventMask | CWOverrideRedirect, &GLWin.attr);
      XWarpPointer (GLWin.dpy, None, GLWin.window, 0, 0, 0, 0, 0, 0);
      XMapRaised (GLWin.dpy, GLWin.window);
      XGrabKeyboard (GLWin.dpy, GLWin.window, True, GrabModeAsync,
		     GrabModeAsync, CurrentTime);
      XGrabPointer (GLWin.dpy, GLWin.window, True, ButtonPressMask,
		    GrabModeAsync, GrabModeAsync, GLWin.window,
		    FALSE, CurrentTime);
    }
  else
    {
      XClassHint xclasshint={"xmms","visplugin"};
      GLWin.window_width = config.window_width;
      GLWin.window_height = config.window_height;
      /* needed if those aren't set yet - ideally need a way to get the default values for these */
      if (config.window_height == 0 || config.window_width == 0)
	{
	  config.window_width = 640;
	  config.window_height = 480;
	}

      /* create a window in window mode */
      GLWin.attr.event_mask =
	ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask;
      GLWin.window =
	XCreateWindow (GLWin.dpy,
		       RootWindow (GLWin.dpy, vi->screen), 0,
		       0, GLWin.window_width, GLWin.window_height, 0,
		       vi->depth, InputOutput, vi->visual,
		       CWBorderPixel | CWColormap | CWEventMask, &GLWin.attr);
      XmbSetWMProperties(GLWin.dpy, GLWin.window, "iris","iris", NULL, 0, NULL, NULL, &xclasshint);

      /* only set window title and handle wm_delete_events if in windowed mode */
      wmDelete = XInternAtom (GLWin.dpy, "WM_DELETE_WINDOW", True);
      XSetWMProtocols (GLWin.dpy, GLWin.window, &wmDelete, 1);
      XSetStandardProperties (GLWin.dpy, GLWin.window, title,
			      title, None, NULL, 0, NULL);
      XMapRaised (GLWin.dpy, GLWin.window);
    }
  /* connect the glx-context to the window */
  if (!glXMakeCurrent (GLWin.dpy, GLWin.window, GLWin.ctx))
    g_log (NULL, G_LOG_LEVEL_CRITICAL,
	   __FILE__ ": glXMakeCurrent returns an error");
  if (glXIsDirect (GLWin.dpy, GLWin.ctx))
    GLWin.DRI = 1;

  hide_cursor ();

  XFree (vi);
  firsttime = FALSE;
  return GLWin.window;
}


VisPlugin *
get_vplugin_info (void)
{
  iris_vp.description = g_strdup_printf ("iris 3D analyzer %s", VERSION);
  return &iris_vp;
}


static int
detect_beat (gint32 loudness)
{
  static gint32 aged;		/* smoothed out oudness */
  static gint32 lowest;		/* quietest point in current beat */
  static int elapsed;		/* frames since last beat */
  static int isquiet;		/* was previous frame quiet */
  int detected_beat;

  /* Incorporate the current loudness into history */
  aged = (aged * 7 + loudness) >> 3;
  elapsed++;

  /* If silent, then clobber the beat */
  if (aged < 2000 || elapsed > BEAT_MAX)
    {
      elapsed = 0;
      lowest = aged;
    }
  else if (aged < lowest)
    lowest = aged;

  /* Beats are detected by looking for a sudden loudness after a lull.
   * They are also limited to occur no more than once every 15 frames,
   * so the beat flashes don't get too annoying.
   */
  detected_beat = (loudness * 4 > aged * 3 && aged * 2 > lowest * 3
		   && elapsed > 15);
  if (detected_beat)
    lowest = aged, elapsed = 0;

  /* Silence is computed from the aged loudness.  The quietref value is
   * set to TRUE only at the start of silence, not throughout the silent
   * period.  Also, there is some hysteresis so that silence followed
   * by a slight noise and more silence won't count as two silent
   * periods -- that sort of thing happens during many fade edits, so
   * we have to account for it.
   */
  if (aged < (isquiet ? 1500 : 500))
    /* Quiet now -- is this the start of quiet? */
    isquiet = TRUE;
  else
    isquiet = FALSE;

  /* return the result */
  return detected_beat;
}


/* limit_rotation_speed keep rotation speed to at least dps_config
   by second */
static void
limit_rotation_speed (gboolean init)
{
  static struct timeval tv_past;
  struct timezone tz;
  struct timeval tv_now;
  float dps_config = 15;

  if (!init)
    {
      long t;

      gettimeofday (&tv_now, &tz);
      t =
	(tv_now.tv_sec - tv_past.tv_sec) * 10000000 + (tv_now.tv_usec -
						       tv_past.tv_usec);
      dps = y_speed * ((float) 1000000 / (float) t);
      if (dps >= (float) dps_config)
	y_speed -= 0.02;
      else
	y_speed += 0.02;

      memcpy (&tv_past, &tv_now, sizeof (struct timeval));
    }
  else
    gettimeofday (&tv_past, &tz);
}


/* limit_fps tries to keep the number of
   frame per seconds to config.fps */
static void
limit_fps (gboolean init)
{
  static float fps = 0;
  static struct timeval tv_past;
  struct timezone tz;
  struct timeval tv_now;
  static int usec = 0;

  if (init)
    gettimeofday (&tv_past, &tz);
  else
    {
      long t;

      gettimeofday (&tv_now, &tz);
      t =
	(tv_now.tv_sec - tv_past.tv_sec) * 10000000 + (tv_now.tv_usec -
						       tv_past.tv_usec);
      fps = (float) 1000000 / (float) t;

      if (fps >= (float) config.fps)
	usec += 100;
      else
	{
	  if (usec > 0)
	    usec -= 100;
	}
      xmms_usleep (usec);
      memcpy (&tv_past, &tv_now, sizeof (struct timeval));
    }
}

static void
init_general_draw_mode (int num)
{
  int transparency;
  int wireframe;

  if (theme[num].config->global->transparency != -1)
    transparency = theme[num].config->global->transparency;
  else
    transparency = (int) (2.0 * rand () / (RAND_MAX + 1.0));

  if (theme[num].config->global->wireframe != -1)
    wireframe = theme[num].config->global->wireframe;
  else
    wireframe = (int) (2.0 * rand () / (RAND_MAX + 1.0));

  if (transparency)
    {
      if (!glIsEnabled (GL_BLEND))
	{
	  glBlendFunc (GL_SRC_ALPHA, GL_ONE);
	  glEnable (GL_BLEND);
	}
      glDisable (GL_DEPTH_TEST);
    }
  else
    {
      if (glIsEnabled (GL_BLEND))
	{
	  glDisable (GL_BLEND);
	}
      glEnable (GL_DEPTH_TEST);
    }

  if (wireframe)
    glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
  else
    glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

  if (glIsEnabled(GL_TEXTURE_2D))
    glDisable(GL_TEXTURE_2D);
}


static int
compute_theme ()
{
  gfloat f_rand, f;
  gint i;

  for (i = 0, f_rand = 0.0; i < THEME_NUMBER; i++)
    f_rand += theme[i].config->global->priority;
  f_rand = f_rand * rand () / (RAND_MAX + 1.0);

  i = 0;
  f = 0;
  while (i < THEME_NUMBER)
    {
      gfloat f_theme;
      f_theme = theme[i].config->global->priority;
      if (f_theme)
	{
	  f += f_theme;
	  if (f_rand < f)
	    break;
	}
      i++;
    }

  if (!f)
    return (int) ((gfloat) THEME_NUMBER * rand () / (RAND_MAX + 1.0));
  else
    return i;
}


static int
choose_theme (gboolean init)
{
  static long sec_btw_theme = 10 * 10000000;
  static struct timeval tv_past;
  struct timezone tz;
  struct timeval tv_now;
  static int th, th_tmp;

  if (!init)
    {
      long t;

      gettimeofday (&tv_now, &tz);
      t =
	(tv_now.tv_sec - tv_past.tv_sec) * 10000000 + (tv_now.tv_usec -
						       tv_past.tv_usec);
      if ((t > sec_btw_theme) || (beat && config.change_theme_on_beat))
	{
	  /* we come here if:
	     - time for the theme is expired
	     - a beat is detected and the change_theme_on_beat option is on
	   */
	  if (config.transition)
	    {
	      if (transition_frames == 0)
		{
		  th_tmp = compute_theme ();
		  if (th != th_tmp)
		    {
		      transition_frames =
			(unsigned int) config.fps * config.trans_duration /
			10;
		      max_transition_frames = transition_frames;
		      memcpy (&tv_past, &tv_now, sizeof (struct timeval));
		      init_theme_transition (transition_frames,
					     max_transition_frames);
		    }
		}
	    }
	  else
	    {
	      th = compute_theme ();
	      init_general_draw_mode (th);
	      if (theme[th].init_draw_mode != NULL)
		theme[th].init_draw_mode ();
	      memcpy (&tv_past, &tv_now, sizeof (struct timeval));
	      x_angle_wanted = theme[th].get_x_angle ();
	      x_speed = copysign (0.08, x_angle_wanted - x_angle);
	    }
	}
      /* change theme after half of the set frames have passed */
      else if ((((int) max_transition_frames / 2) == transition_frames)
	       && config.transition && (transition_frames != 0))
	{
	  th = th_tmp;
	  init_general_draw_mode (th);
	  if (theme[th].init_draw_mode != NULL)
	    theme[th].init_draw_mode ();
	  x_angle_wanted = theme[th].get_x_angle ();
	  x_speed = copysign (0.08, x_angle_wanted - x_angle);
	}
    }
  else
    {
      /* the first time choose_theme is called,
         tv_past is initialized */
      gettimeofday (&tv_past, &tz);
      th = compute_theme ();
      init_general_draw_mode (th);
      if (theme[th].init_draw_mode != NULL)
	theme[th].init_draw_mode ();
    }

  return (th);
}


static void
draw_iris (void)
{
  int th;

  limit_fps (FALSE);
  th = choose_theme (FALSE);

  if ((config.color_beat) && (beat_before > 0))
    glClearColor (config.color_flash_red, config.color_flash_green,
		  config.color_flash_blue, 1);
  else
    glClearColor (config.bgc_red, config.bgc_green, config.bgc_blue, 1);
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix ();
  glTranslatef (0.0, -0.5, -7.0);
  glRotatef (x_angle, 1.0, 0.0, 0.0);
  glRotatef (y_angle, 0.0, 1.0, 0.0);
  glRotatef (z_angle, 0.0, 0.0, 1.0);

  if (transition_frames > 0 && config.transition)
    {
      theme_transition ();
      transition_frames--;
    }

  theme[th].draw_one_frame (beat);

  glEnd ();
  glPopMatrix ();

  glXSwapBuffers (GLWin.dpy, GLWin.window);
}


static gint
disable_func (gpointer data)
{
  iris_vp.disable_plugin (&iris_vp);
  return FALSE;
}


GLvoid
init_gl (GLvoid)
{
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glFrustum (-1, 1, -1.5, 1, 1.5, 12);
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();
}


GLvoid
kill_gl_window (GLvoid)
{
  if (GLWin.ctx)
    {
      glXMakeCurrent (GLWin.dpy, None, NULL);
      glXDestroyContext (GLWin.dpy, GLWin.ctx);
      GLWin.ctx = NULL;
    }
  /* switch back to original desktop resolution if we were in fs */
  if (GLWin.fs)
    {
      XF86VidModeSwitchToMode (GLWin.dpy, GLWin.screen, &GLWin.deskMode);
      XF86VidModeSetViewPort (GLWin.dpy, GLWin.screen, 0, 0);
    }
}


void *
draw_thread_func (void *arg)
{
  Bool configured = FALSE;

  g_log (NULL, G_LOG_LEVEL_DEBUG,  __FILE__ ": draw_thread_func: Starting.");

  if ((GLWin.window = create_window ("Iris")) == 0)
    {
      g_log (NULL, G_LOG_LEVEL_CRITICAL,
	     __FILE__ ": unable to create window");
      pthread_exit (NULL);
    }

  init_gl ();
  choose_theme (TRUE);


#ifdef HAVE_SCHED_SETSCHEDULER
  if (xmms_check_realtime_priority ())
    {
      struct sched_param sparam;
      sparam.sched_priority = sched_get_priority_max (SCHED_OTHER);
      pthread_setschedparam (pthread_self (), SCHED_OTHER, &sparam);
    }
#endif

  while (going)			/* plugin enabled */
    {
      while (XPending (GLWin.dpy))
	{
	  XEvent event;
	  KeySym keysym;
	  char buf[16];

	  XNextEvent (GLWin.dpy, &event);
	  switch (event.type)
	    {
	    case Expose:
	      if (event.xexpose.count != 0)
		break;
	      configured = TRUE;
	      break;
	    case ConfigureNotify:
	      glViewport (0, 0, event.xconfigure.width,
			  event.xconfigure.height);
	      configured = TRUE;
	      break;
	    case KeyPress:

	      XLookupString (&event.xkey, buf, 16, &keysym, NULL);
	      switch (keysym)
		{
		case XK_Escape:

		  /* Ugly hack to get the disable_plugin call in the main thread. */
		  GDK_THREADS_ENTER ();
		  gtk_idle_add (disable_func, NULL);
		  GDK_THREADS_LEAVE ();
		  break;
		case XK_z:
		  xmms_remote_playlist_prev (iris_vp.xmms_session);
		  break;
		case XK_x:
		  xmms_remote_play (iris_vp.xmms_session);
		  break;
		case XK_c:
		  xmms_remote_pause (iris_vp.xmms_session);
		  break;
		case XK_v:
		  xmms_remote_stop (iris_vp.xmms_session);
		  break;
		case XK_b:
		  xmms_remote_playlist_next (iris_vp.xmms_session);
		  break;
		case XK_Left:
		  y_speed -= 0.1;
		  if (y_speed < -3.0)
		    y_speed = -3.0;
		  break;
		case XK_Right:
		  y_speed += 0.1;
		  if (y_speed > 3.0)
		    y_speed = 3.0;
		  break;
		case XK_Return:
		  x_speed = 0.0;
		  y_speed = 0.3;
		  z_speed = 0.0;
		  x_angle = 70.0;
		  y_angle = 45.0;
		  z_angle = 0.0;
		  break;
		case XK_Tab:
		  iris_configure ();
		  break;
		case XK_f:
		  //iris_save_window_attributes ();
		  kill_gl_window ();
		  XCloseDisplay (GLWin.dpy);
		  GLWin.fs = !GLWin.fs;
		  create_window ("Iris");
		  init_gl ();
		  choose_theme (TRUE);
		  break;
		}

	      break;
	    case ClientMessage:
	      if ((Atom) event.xclient.data.l[0] == wmDelete)
		{
		  GDK_THREADS_ENTER ();
		  gtk_idle_add (disable_func, NULL);
		  GDK_THREADS_LEAVE ();
		}
	      break;
	    }
	}
      if (configured)
	{
	  limit_rotation_speed (FALSE);

	  if ((x_angle > x_angle_wanted) && (x_speed > 0))
	    x_angle = x_angle_wanted;
	  else if ((x_angle < x_angle_wanted) && (x_speed < 0))
	    x_angle = x_angle_wanted;

	  x_angle += x_speed;
	  if (x_angle > 85.0)
	    x_angle = 85.0;
	  else if (x_angle < 0.0)
	    x_angle = 0.0;

	  y_angle += y_speed;
	  if (y_angle >= 360.0)
	    y_angle -= 360.0;

	  z_angle += z_speed;
	  if (z_angle >= 360.0)
	    z_angle -= 360.0;

	  draw_iris ();
	}
    }

  g_log (NULL, G_LOG_LEVEL_DEBUG,  __FILE__ ": draw_thread_func: Exiting.");
  pthread_exit (NULL);
}


static void
start_display (void)
{
  scale = 1.0 / log (256.0);

  x_speed = 0.0;
  y_speed = 0.3;
  z_speed = 0.0;
  x_angle = 45.0;
  y_angle = 45.0;
  z_angle = 0.0;

  going = TRUE;
  limit_fps (TRUE);
  limit_rotation_speed (TRUE);
  if (pthread_create (&draw_thread, NULL, draw_thread_func, NULL))
    g_log (NULL, G_LOG_LEVEL_CRITICAL,  __FILE__ ": pthread_create: Can't create drawing thread.");
   
}


static void
stop_display (void)
{
  if (going)
    {
      going = FALSE;
      pthread_join (draw_thread, NULL);
    }

  kill_gl_window ();

  if (GLWin.window)
    {
      if (grabbed_pointer)
	{
	  XUngrabPointer (GLWin.dpy, CurrentTime);
	  grabbed_pointer = FALSE;
	}

      XDestroyWindow (GLWin.dpy, GLWin.window);
      GLWin.window = 0;
    }
  XCloseDisplay (GLWin.dpy);
}


static void
iris_init (void)
{
  int i;
  
  initialized = TRUE;
  iris_first_init ();
  datas.loudness = 0;
  /* if the config window is open, we don't want to trash the config the user
   * has done */
  if (!config_window)
    iris_config_read ();
  for (i = 0; i < THEME_NUMBER; i++)
    if (theme[i].init != NULL)
      theme[i].init ();
  srand (666);
  start_display ();
}


static void
iris_cleanup (void)
{
  int i;

  if(initialized)
  {
    stop_display ();
    for (i = 0; i < THEME_NUMBER; i++)
      if (theme[i].cleanup != NULL)
	theme[i].cleanup ();
  }
}


static void
iris_playback_start (void)
{
}


static void
iris_playback_stop (void)
{
}


static void
iris_render_freq (gint16 data[2][256])
{
  GLfloat val;
  static int angle = 0;
  int i;

  for (i = 0; i < num_bands; i++)
    {
      int y, c;
      gint xscale[] =
	{ 0, 1, 2, 3, 5, 7, 10, 14, 20, 28, 40, 54, 74, 101, 137, 156,
	255
      };
      gint xscale8[] = { 0, 2, 5, 10, 20, 40, 74, 137, 255 };

      if (num_bands == 16)
	for (c = xscale[i], y = 0; c < xscale[i + 1]; c++)
	  {
	    if (data[0][c] > y)
	      y = data[0][c];
	  }
      else
	for (c = xscale8[i], y = 0; c < xscale8[i + 1]; c++)
	  {
	    if (data[0][c] > y)
	      y = data[0][c];
	  }

      datas.loudness +=
	(y / (xscale[i + 1] - xscale[i] + 1)) * (abs (i - NUM_BANDS / 2) +
						 NUM_BANDS / 2) * (4 + i);

      y >>= 7;
      if (y > 0)
	val = (log (y) * scale);
      else
	val = 0;
      datas.data360[angle][i] = val;
      datas.data1[i] = val;
    }

  datas.loudness /= (NUM_BANDS * 4);

  beat = detect_beat (datas.loudness);
  if (beat)
    {
      beat_before = config.flash_speed;
      if (dps <= 90.0)
	y_speed += 0.7;

      if (config.bgc_random)
	{
	  config.bgc_red = 1.0 * rand () / (RAND_MAX + 1.0);
	  config.bgc_green = 1.0 * rand () / (RAND_MAX + 1.0);
	  config.bgc_blue = 1.0 * rand () / (RAND_MAX + 1.0);
	}

      if (config.color12_random)
	{
	  config.color1_red = 1.0 * rand () / (RAND_MAX + 1.0);
	  config.color1_green = 1.0 * rand () / (RAND_MAX + 1.0);
	  config.color1_blue = 1.0 * rand () / (RAND_MAX + 1.0);
	  config.color2_red = 1.0 * rand () / (RAND_MAX + 1.0);
	  config.color2_green = 1.0 * rand () / (RAND_MAX + 1.0);
	  config.color2_blue = 1.0 * rand () / (RAND_MAX + 1.0);
	}

      if (config.color_random)
	{
	  config.color_red = 1.0 * rand () / (RAND_MAX + 1.0);
	  config.color_green = 1.0 * rand () / (RAND_MAX + 1.0);
	  config.color_blue = 1.0 * rand () / (RAND_MAX + 1.0);
	}
    }

  if (beat_before > 0)
    beat_before--;

  angle++;
  if (angle == 360)
    angle = 0;
}
