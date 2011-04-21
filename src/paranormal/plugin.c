/*
 * paranormal: iterated pipeline-driven visualization plugin
 * Copyright (c) 2006, 2007 William Pitcock <nenolod@dereferenced.org>
 * Portions copyright (c) 2001 Jamie Gennis <jgennis@mindspring.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* FIXME: issues with not uniniting variables between
   enables?  I wasn't too careful about that, but it
   seems to work fine.  If there are problems perhaps
   look for a bug there?
*/

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <memory.h>
#include <math.h>
#include <setjmp.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <SDL.h>
#include <SDL_thread.h>

#ifdef _POSIX_PRIORITY_SCHEDULING
#include <sched.h>
#endif

#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <audacious/plugins.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "paranormal.h"
#include "actuators.h"
#include "presets.h"
#include "containers.h"

/* Error reporting dlg */
static GtkWidget *err_dialog;

/* Draw thread stuff */
/* FIXME: Do I need mutex for pn_done? */
static SDL_Thread *draw_thread = NULL;
static SDL_mutex *sound_data_mutex;
static SDL_mutex *config_mutex;

static gboolean pn_done = FALSE;
jmp_buf quit_jmp;
gboolean timeout_set = FALSE;
guint quit_timeout;

/* Sound stuffs */
static gboolean new_pcm_data = FALSE;
static gboolean new_freq_data = FALSE;
static gint16 tmp_pcm_data[2][512];
static gint16 tmp_freq_data[2][256];

/* XMMS interface */
static gboolean pn_xmms_init (void);
static void pn_xmms_cleanup (void);
static void pn_xmms_about (void);
static void pn_xmms_configure (void);
static void pn_xmms_render_pcm (gint16 data[2][512]);
static void pn_xmms_render_freq (gint16 data[2][256]);

AUD_VIS_PLUGIN
(
  .name = "Paranormal Visualization Studio",
  .num_pcm_chs_wanted = 2,
  .num_freq_chs_wanted = 2,
  .init = pn_xmms_init,
  .cleanup = pn_xmms_cleanup,
  .about = pn_xmms_about,
  .configure = pn_xmms_configure,
  .render_pcm = pn_xmms_render_pcm,
  .render_freq = pn_xmms_render_freq
)

static void
load_pn_rc (void)
{
  struct pn_actuator *a, *b;

  if (! pn_rc)
      pn_rc = g_new0 (struct pn_rc, 1);

  /* load a default preset */
  pn_rc->actuator = create_actuator ("container_simple");
  if (! pn_rc->actuator) goto ugh;
  a = create_actuator ("container_once");
  if (! a) goto ugh;
  b = create_actuator ("cmap_bwgradient");
  if (! b) goto ugh;
  b->options[2].val.cval.r = 64;
  b->options[2].val.cval.g = 128;
  container_add_actuator (a, b);
  container_add_actuator (pn_rc->actuator, a);

  a = create_actuator ("wave_horizontal");
  if (! a) goto ugh;
  container_add_actuator (pn_rc->actuator, a);

  a = create_actuator ("xform_movement");
  if (! a) goto ugh;
  a->options[0].val.sval = g_strdup("d = cos(d)^2;");
  container_add_actuator (pn_rc->actuator, a);

  a = create_actuator ("general_fade");
  if (! a) goto ugh;
  container_add_actuator (pn_rc->actuator, a);

  a = create_actuator ("general_blur");
  if (! a) goto ugh;
  container_add_actuator (pn_rc->actuator, a);

  return;

 ugh:
  if (pn_rc->actuator)
    destroy_actuator (pn_rc->actuator);
  pn_error ("Error loading default preset");
}

static int
draw_thread_fn (gpointer data)
{
  gfloat fps = 0.0;
  guint last_time = 0, last_second = 0;
  guint this_time;
  pn_init ();

  /* Used when pn_quit is called from this thread */
  if (setjmp (quit_jmp) != 0)
    pn_done = TRUE;

  while (! pn_done)
    {
      SDL_mutexP (sound_data_mutex);
      if (new_freq_data)
	{
	  memcpy (pn_sound_data->freq_data, tmp_freq_data,
		  sizeof (gint16) * 2 * 256);
	  new_freq_data = FALSE;
	}
      if (new_pcm_data)
	{
	  memcpy (pn_sound_data->pcm_data, tmp_pcm_data,
		  sizeof (gint16) * 2 * 512);
	  new_freq_data = FALSE;
	}
      SDL_mutexV (sound_data_mutex);
      SDL_mutexP (config_mutex);
      pn_render ();
      SDL_mutexV (config_mutex);

      /* Compute the FPS */
      this_time = SDL_GetTicks ();

      fps = fps * .95 + (1000. / (gfloat) (this_time - last_time)) * .05;
      if (this_time > 2000 + last_second)
        {
          last_second = this_time;
          g_print ("FPS: %f\n", fps);
        }
      last_time = this_time;

#ifdef _POSIX_PRIORITY_SCHEDULING
      sched_yield();
#endif
    }

  /* Just in case a pn_quit () was called in the loop */
/*    SDL_mutexV (sound_data_mutex); */

  pn_cleanup ();

  return 0;
}

/* Is there a better way to do this? this = messy
   It appears that calling disable_plugin () in some
   thread other than the one that called pn_xmms_init ()
   causes a seg fault :( */
static int
quit_timeout_fn (gpointer data)
{
  if (pn_done)
    {
      aud_plugin_enable (aud_plugin_by_header (& _aud_plugin_self), FALSE);
      return FALSE;
    }

  return TRUE;
}

static gboolean pn_xmms_init (void)
{
  /* If it isn't already loaded, load the run control */
  if (! pn_rc)
	  load_pn_rc ();

  sound_data_mutex = SDL_CreateMutex ();
  config_mutex = SDL_CreateMutex ();
  if (! sound_data_mutex)
    pn_fatal_error ("Unable to create a new mutex: %s",
		    SDL_GetError ());

  pn_done = FALSE;
  draw_thread = SDL_CreateThread (draw_thread_fn, NULL);
  if (! draw_thread)
    pn_fatal_error ("Unable to create a new thread: %s",
		    SDL_GetError ());

  /* Add a gtk timeout to test for quits */
  quit_timeout = gtk_timeout_add (1000, quit_timeout_fn, NULL);
  timeout_set = TRUE;

  return TRUE;
}

static void
pn_xmms_cleanup (void)
{
  if (timeout_set)
    {
      gtk_timeout_remove (quit_timeout);
      timeout_set = FALSE;
    }

  if (draw_thread)
    {
      pn_done = TRUE;
      SDL_WaitThread (draw_thread, NULL);
      draw_thread = NULL;
    }

  if (sound_data_mutex)
    {
      SDL_DestroyMutex (sound_data_mutex);
      sound_data_mutex = NULL;
    }

  if (config_mutex)
    {
      SDL_DestroyMutex (config_mutex);
      config_mutex = NULL;
    }
}

static void
pn_xmms_about (void)
{
  static GtkWidget * aboutbox = NULL;
  audgui_simple_message (& aboutbox, GTK_MESSAGE_INFO,
  "About Paranormal Visualization Studio",
"Paranormal Visualization Studio " VERSION "\n\n\
Copyright (C) 2006, William Pitcock <nenolod -at- nenolod.net>\n\
Portions Copyright (C) 2001, Jamie Gennis <jgennis -at- mindspring.com>\n\
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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301\n\
USA");
}

static void
pn_xmms_configure (void)
{
  /* We should already have a GDK_THREADS_ENTER
     but we need to give it config_mutex */
  if (config_mutex)
    SDL_mutexP (config_mutex);

  if (! pn_rc)
	  load_pn_rc ();

  pn_configure ();

  if (config_mutex)
    SDL_mutexV (config_mutex);
}

static void
pn_xmms_render_pcm (gint16 data[2][512])
{
  SDL_mutexP (sound_data_mutex);
  memcpy (tmp_pcm_data, data, sizeof (gint16) * 2 * 512);
  new_pcm_data = TRUE;
  SDL_mutexV (sound_data_mutex);
}

static void
pn_xmms_render_freq (gint16 data[2][256])
{
  SDL_mutexP (sound_data_mutex);
  memcpy (tmp_freq_data, data, sizeof (gint16) * 2 * 256);
  new_freq_data = TRUE;
  SDL_mutexV (sound_data_mutex);
}

/* **************** paranormal.h stuff **************** */

void
pn_set_rc (struct pn_rc *new_rc)
{
  if (config_mutex)
    SDL_mutexP (config_mutex);

  if (! pn_rc)
    load_pn_rc ();

  if (pn_rc->actuator)
    destroy_actuator (pn_rc->actuator);
  pn_rc->actuator = new_rc->actuator;

  if (config_mutex)
    SDL_mutexV (config_mutex);
}

void
pn_fatal_error (const char *fmt, ...)
{
  char *errstr;
  va_list ap;
  GtkWidget *dialog;
  GtkWidget *close, *label;

  /* Don't wanna try to lock GDK if we already have it */
  if (draw_thread && SDL_ThreadID () == SDL_GetThreadID (draw_thread))
    GDK_THREADS_ENTER ();

  /* now report the error... */
  va_start (ap, fmt);
  errstr = g_strdup_vprintf (fmt, ap);
  va_end (ap);

  dialog=gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(dialog), "Error - Paranormal Visualization Studio - " VERSION);
  gtk_container_border_width (GTK_CONTAINER (dialog), 8);

  label=gtk_label_new(errstr);
  fprintf (stderr, "%s\n", errstr);
  g_free (errstr);

  close = gtk_button_new_with_label ("Close");
  gtk_signal_connect_object (GTK_OBJECT (close), "clicked",
			     GTK_SIGNAL_FUNC (gtk_widget_destroy),
			     GTK_OBJECT (dialog));

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), label, FALSE,
		      FALSE, 0);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->action_area), close,
		      FALSE, FALSE, 0);
  gtk_widget_show (label);
  gtk_widget_show (close);

  gtk_widget_show (dialog);
  gtk_widget_grab_focus (dialog);

  if (draw_thread && SDL_ThreadID () == SDL_GetThreadID (draw_thread))
    GDK_THREADS_LEAVE ();

  pn_quit ();
}


void
pn_error (const char *fmt, ...)
{
  char *errstr;
  va_list ap;
  static GtkWidget *text;
  static GtkTextBuffer *textbuf;

  /* now report the error... */
  va_start (ap, fmt);
  errstr = g_strdup_vprintf (fmt, ap);
  va_end (ap);
  fprintf (stderr, "Paranormal-CRITICAL **: %s\n", errstr);

  /* This is the easiest way of making sure we don't
     get stuck trying to lock a mutex that this thread
     already owns since this fn can be called from either
     thread */
  if (draw_thread && SDL_ThreadID () == SDL_GetThreadID (draw_thread))
    GDK_THREADS_ENTER ();

  if (! err_dialog)
    {
      GtkWidget *close;

      err_dialog=gtk_dialog_new();
      gtk_window_set_title (GTK_WINDOW (err_dialog), "Error - Paranormal Visualization Studio - " VERSION);
      gtk_window_set_policy (GTK_WINDOW (err_dialog), FALSE, FALSE, FALSE);
      gtk_widget_set_usize (err_dialog, 400, 200);
      gtk_container_border_width (GTK_CONTAINER (err_dialog), 8);

      textbuf = gtk_text_buffer_new(NULL);
      text = gtk_text_view_new_with_buffer (textbuf);

      close = gtk_button_new_with_label ("Close");
      gtk_signal_connect_object (GTK_OBJECT (close), "clicked",
				 GTK_SIGNAL_FUNC (gtk_widget_hide),
				 GTK_OBJECT (err_dialog));
      gtk_signal_connect_object (GTK_OBJECT (err_dialog), "delete-event",
				 GTK_SIGNAL_FUNC (gtk_widget_hide),
				 GTK_OBJECT (err_dialog));

      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (err_dialog)->vbox), text, FALSE,
			  FALSE, 0);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (err_dialog)->action_area), close,
			  FALSE, FALSE, 0);
      gtk_widget_show (text);
      gtk_widget_show (close);
    }

  gtk_text_buffer_set_text(GTK_TEXT_BUFFER(textbuf), errstr, -1);
  g_free (errstr);

  gtk_widget_show (err_dialog);
  gtk_widget_grab_focus (err_dialog);

  if (draw_thread && SDL_ThreadID () == SDL_GetThreadID (draw_thread))
    GDK_THREADS_LEAVE ();
}


/* This is confusing...
   Don't call this from anywhere but the draw thread or
   the initialization xmms thread (ie NOT the xmms sound
   data functions) */
void
pn_quit (void)
{
  if (draw_thread && SDL_ThreadID () == SDL_GetThreadID (draw_thread))
    {
      /* We're in the draw thread so be careful */
      longjmp (quit_jmp, 1);
    }
  else
    {
      /* We're not in the draw thread, so don't sweat it...
	 addendum: looks like we have to bend over backwards (forwards?)
	 for xmms here too */
      aud_plugin_enable (aud_plugin_by_header (& _aud_plugin_self), FALSE);
      while (1)
	gtk_main_iteration ();
    }
}
