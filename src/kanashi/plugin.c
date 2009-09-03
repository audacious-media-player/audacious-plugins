/*
 * kanashi: iterated javascript-driven visualization framework
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

#include <glib.h>
#include <audacious/i18n.h>

#include <gtk/gtk.h>
#include <audacious/plugin.h>

#include "kanashi.h"

/* Error reporting dlg */
static GtkWidget *err_dialog;
extern GtkWidget *kanashi_win;

/* Draw thread stuff */
/* FIXME: Do I need mutex for kanashi_done? */
static GThread *draw_thread = NULL;
static GMutex *sound_data_mutex;
static GMutex *config_mutex;
GCond *render_cond;
extern GStaticMutex kanashi_mutex;

static gboolean kanashi_done = FALSE;
static jmp_buf quit_jmp;
static gboolean timeout_set = FALSE;
static guint quit_timeout;

/* Sound stuffs */
static gboolean new_pcm_data = FALSE;
static gboolean new_freq_data = FALSE;
static gint16 tmp_pcm_data[2][512];
static gint16 tmp_freq_data[2][256];

/* XMMS interface */
static void kanashi_xmms_init (void);
static void kanashi_xmms_cleanup (void);
static void kanashi_xmms_about (void);
static void kanashi_xmms_configure (void);
static void kanashi_xmms_render_pcm (gint16 data[2][512]);
static void kanashi_xmms_render_freq (gint16 data[2][256]);

static GtkWidget *
kanashi_get_widget(void)
{
  g_print("get widget\n");

  return kanashi_win;
}

static VisPlugin kanashi_vp = 
{
  .description = "Kanashi",
  .num_pcm_chs_wanted = 2,
  .num_freq_chs_wanted = 2,
  .init = kanashi_xmms_init,
  .cleanup = kanashi_xmms_cleanup,
  .about = kanashi_xmms_about,
  .configure = kanashi_xmms_configure,
  .render_pcm = kanashi_xmms_render_pcm,
  .render_freq = kanashi_xmms_render_freq,
  .get_widget = kanashi_get_widget,
};

VisPlugin *kanashi_vplist[] = { &kanashi_vp, NULL };

DECLARE_PLUGIN(kanashi, NULL, NULL, NULL, NULL, NULL, NULL, kanashi_vplist,NULL);

static gpointer
draw_thread_fn (gpointer data)
{
  while (kanashi_done == FALSE)
    {
      g_mutex_lock(sound_data_mutex);

      if (new_freq_data)
	{
	  memcpy (kanashi_sound_data->freq_data, tmp_freq_data,
		  sizeof (gint16) * 2 * 256);
	  new_freq_data = FALSE;
	}
      if (new_pcm_data)
	{
	  memcpy (kanashi_sound_data->pcm_data, tmp_pcm_data,
		  sizeof (gint16) * 2 * 512);
	  new_freq_data = FALSE;
	}

      g_mutex_unlock(sound_data_mutex);

      g_mutex_lock(config_mutex);

      g_static_mutex_lock(&kanashi_mutex);
      g_cond_wait(render_cond, g_static_mutex_get_mutex(&kanashi_mutex));
      g_static_mutex_unlock(&kanashi_mutex);

      kanashi_render ();
      g_mutex_unlock(config_mutex);
    }

  g_print("exit\n");

  kanashi_cleanup ();

  return NULL;
}

/* Is there a better way to do this? this = messy
   It appears that calling disable_plugin () in some
   thread other than the one that called kanashi_xmms_init ()
   causes a seg fault :( */
static int
quit_timeout_fn (gpointer data)
{
  if (kanashi_done)
    {
      kanashi_vp.disable_plugin (&kanashi_vp);
      return FALSE;
    }

  return TRUE;
}

static void
kanashi_xmms_init (void)
{
  kanashi_init ();

  sound_data_mutex = g_mutex_new();
  config_mutex = g_mutex_new();
  render_cond = g_cond_new();

  if (sound_data_mutex == NULL)
    kanashi_fatal_error ("Unable to create a new mutex");

  kanashi_done = FALSE;

  draw_thread = g_thread_create(draw_thread_fn, NULL, TRUE, NULL);
  if (draw_thread == NULL)
    kanashi_fatal_error ("Unable to create a new thread");

  /* Add a gtk timeout to test for quits */
  quit_timeout = gtk_timeout_add (1000, quit_timeout_fn, NULL);
  timeout_set = TRUE;
}

static void
kanashi_xmms_cleanup (void)
{
  if (timeout_set)
    {
      gtk_timeout_remove (quit_timeout);
      timeout_set = FALSE;
    }

  if (draw_thread)
    {
      kanashi_done = TRUE;
      g_cond_signal(render_cond);
      g_thread_join(draw_thread);
      draw_thread = NULL;
    }

  if (sound_data_mutex)
    {
      g_mutex_free (sound_data_mutex);
      sound_data_mutex = NULL;
    }

  if (config_mutex)
    {
      g_mutex_free (config_mutex);
      config_mutex = NULL;
    }

  if (render_cond)
    {
      g_cond_free (render_cond);
      render_cond = NULL;
    }
}

#define KANASHI_VERSION		"0.1alpha3"
static void
kanashi_xmms_about (void)
{
  audacious_info_dialog("About Kanashi", 

"Kanashi " KANASHI_VERSION "\n\n\
Copyright (C) 2009, William Pitcock <nenolod -at- dereferenced.org>\n\
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
USA", _("Ok"), FALSE, NULL, NULL);
}

static void
kanashi_xmms_configure (void)
{
  /* We should already have a GDK_THREADS_ENTER
     but we need to give it config_mutex */
  if (config_mutex)
    g_mutex_lock (config_mutex);

  kanashi_configure ();

  if (config_mutex)
    g_mutex_unlock (config_mutex);
}

static void
kanashi_xmms_render_pcm (gint16 data[2][512])
{
  g_mutex_lock (sound_data_mutex);
  memcpy (tmp_pcm_data, data, sizeof (gint16) * 2 * 512);
  new_pcm_data = TRUE;
  g_mutex_unlock (sound_data_mutex);
}

static void
kanashi_xmms_render_freq (gint16 data[2][256])
{
  g_mutex_lock (sound_data_mutex);
  memcpy (tmp_freq_data, data, sizeof (gint16) * 2 * 256);
  new_freq_data = TRUE;
  g_mutex_unlock (sound_data_mutex);
}

/* **************** kanashi.h stuff **************** */

void
kanashi_set_rc (struct kanashi_rc *new_rc)
{
}

void
kanashi_fatal_error (const char *fmt, ...)
{
  char *errstr;
  va_list ap;
  GtkWidget *dialog;
  GtkWidget *close, *label;

  /* Don't wanna try to lock GDK if we already have it */
  if (draw_thread && g_thread_self() == draw_thread)
    GDK_THREADS_ENTER ();

  /* now report the error... */
  va_start (ap, fmt);
  errstr = g_strdup_vprintf (fmt, ap);
  va_end (ap);

  dialog=gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(dialog), "Kanashi: Fatal Error");
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

  if (draw_thread && g_thread_self() == draw_thread)
    GDK_THREADS_LEAVE ();

  kanashi_quit ();
}


void
kanashi_error (const char *fmt, ...)
{
  char *errstr;
  va_list ap;
  static GtkWidget *text;
  static GtkTextBuffer *textbuf;

  /* now report the error... */
  va_start (ap, fmt);
  errstr = g_strdup_vprintf (fmt, ap);
  va_end (ap);
  fprintf (stderr, "Kanashi-CRITICAL **: %s\n", errstr);

  /* This is the easiest way of making sure we don't
     get stuck trying to lock a mutex that this thread
     already owns since this fn can be called from either
     thread */
  if (draw_thread && g_thread_self() == draw_thread)
    GDK_THREADS_ENTER ();

  if (! err_dialog)
    {
      GtkWidget *close;

      err_dialog=gtk_dialog_new();
      gtk_window_set_title (GTK_WINDOW (err_dialog), "Kanashi: Error");
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

  if (draw_thread && g_thread_self() == draw_thread)
    GDK_THREADS_LEAVE ();
}


/* This is confusing...
   Don't call this from anywhere but the draw thread or
   the initialization xmms thread (ie NOT the xmms sound
   data functions) */
void
kanashi_quit (void)
{
  if (draw_thread && g_thread_self() == draw_thread)
    {
      /* We're in the draw thread so be careful */
      longjmp (quit_jmp, 1);
    }
  else
    {
      /* We're not in the draw thread, so don't sweat it...
	 addendum: looks like we have to bend over backwards (forwards?)
	 for xmms here too */
      kanashi_vp.disable_plugin (&kanashi_vp);
      while (1)
	gtk_main_iteration ();
    }
}
