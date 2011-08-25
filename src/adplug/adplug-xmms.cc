/*
   AdPlug/XMMS - AdPlug XMMS Plugin
   Copyright (C) 2002, 2003 Simon Peter <dn.tlp@gmx.net>

   AdPlug/XMMS is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This plugin is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this plugin; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "config.h"

#include <algorithm>
#include <sstream>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <gtk/gtk.h>
#include "adplug.h"
#include "emuopl.h"
#include "silentopl.h"
#include "players.h"

extern "C" {
#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <libaudcore/tuple_formatter.h>
#include <libaudcore/vfs_buffered_file.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "adplug-xmms.h"
}


/***** Defines *****/

// Version string
#define ADPLUG_NAME	"AdPlug (AdLib Sound Player)"

// Sound buffer size in samples
#define SNDBUFSIZE	512

// AdPlug's 8 and 16 bit audio formats
#define FORMAT_8	FMT_U8
#define FORMAT_16	FMT_S16_NE

// Default file name of AdPlug's database file
#define ADPLUGDB_FILE		"adplug.db"

// Default AdPlug user's configuration subdirectory
#define ADPLUG_CONFDIR		".adplug"

/***** Global variables *****/

extern "C" InputPlugin adplug_ip;
static gboolean audio_error = FALSE;
GtkWidget *about_win = NULL;
static GMutex * control_mutex;
static GCond * control_cond;
static gboolean stop_flag;

// Configuration (and defaults)
static struct
{
  gint freq;
  gboolean bit16, stereo, endless;
  CPlayers players;
} conf =
{
44100l, true, false, false, CAdPlug::getPlayers()};

// Player variables
static struct
{
  CPlayer *p;
  CAdPlugDatabase *db;
  unsigned int subsong, songlength;
  int seek;
  char * filename;
  GtkLabel *infobox;
  GtkDialog *infodlg;
} plr = {0, 0, 0, 0, -1, NULL, NULL, NULL};

static InputPlayback *playback;

/***** Debugging *****/

#ifdef DEBUG

#include <stdarg.h>

static void
dbg_printf (const char *fmt, ...)
{
  va_list argptr;

  va_start (argptr, fmt);
  vfprintf (stderr, fmt, argptr);
  va_end (argptr);
}

#else

static void
dbg_printf (const char *fmt, ...)
{
}

#endif

static CPlayer *
factory (VFSFile * fd, Copl * newopl)
{
  CPlayers::const_iterator i;

  dbg_printf ("factory(%p<%s>,opl): ", fd,
              fd->uri != NULL ? fd->uri : "unknown");
  return CAdPlug::factory (fd, newopl, conf.players);
}

extern "C" {
void adplug_stop(InputPlayback * data);
gboolean adplug_play(InputPlayback * data, const gchar * filename, VFSFile * file, gint start_time, gint stop_time, gboolean pause);
}

/***** Main player (!! threaded !!) *****/

extern "C" Tuple * adplug_get_tuple (const gchar * filename, VFSFile * fd)
{
  Tuple * ti = NULL;
  CSilentopl tmpopl;

  if (!fd)
    return NULL;

  CPlayer *p = factory (fd, &tmpopl);

  if (p)
  {
    ti = tuple_new_from_filename (filename);

    if (! p->getauthor().empty())
      tuple_associate_string(ti, FIELD_ARTIST, NULL, p->getauthor().c_str());
    if (! p->gettitle().empty())
      tuple_associate_string(ti, FIELD_TITLE, NULL, p->gettitle().c_str());
    else if (! p->getdesc().empty())
      tuple_associate_string(ti, FIELD_TITLE, NULL, p->getdesc().c_str());
    else
      tuple_associate_string(ti, FIELD_TITLE, NULL, g_path_get_basename(filename));
    tuple_associate_string(ti, FIELD_CODEC, NULL, p->gettype().c_str());
    tuple_associate_string(ti, FIELD_QUALITY, NULL, "sequenced");
    tuple_associate_int(ti, FIELD_LENGTH, NULL, p->songlength (plr.subsong));
    delete p;
  }

  return ti;
}

// Define sampsize macro (only usable inside play_loop()!)
#define sampsize ((bit16 ? 2 : 1) * (stereo ? 2 : 1))

static gboolean play_loop (InputPlayback * playback, const gchar * filename,
 VFSFile * fd)
/* Main playback thread. Takes the filename to play as argument. */
{
  dbg_printf ("play_loop(\"%s\"): ", filename);
  CEmuopl opl (conf.freq, conf.bit16, conf.stereo);
  long toadd = 0, i, towrite;
  char *sndbuf, *sndbufpos;
  bool playing = true,          // Song self-end indicator.
    bit16 = conf.bit16,          // Duplicate config, so it doesn't affect us if
    stereo = conf.stereo;        // the user changes it while we're playing.
  unsigned long freq = conf.freq;

  if (!fd)
    return FALSE;

  // Try to load module
  dbg_printf ("factory, ");
  if (!(plr.p = factory (fd, &opl)))
  {
    dbg_printf ("error!\n");
    // MessageBox("AdPlug :: Error", "File could not be opened!", "Ok");
    return FALSE;
  }

  // reset to first subsong on new file
  dbg_printf ("subsong, ");
  if (! plr.filename || strcmp (filename, plr.filename))
  {
    g_free (plr.filename);
    plr.filename = g_strdup (filename);
    plr.subsong = 0;
  }

  // Allocate audio buffer
  dbg_printf ("buffer, ");
  sndbuf = (char *) malloc (SNDBUFSIZE * sampsize);

  // Set XMMS main window information
  dbg_printf ("xmms, ");
  playback->set_params (playback, freq * sampsize * 8, freq, stereo ? 2 : 1);

  // Rewind player to right subsong
  dbg_printf ("rewind, ");
  plr.p->rewind (plr.subsong);

  g_mutex_lock (control_mutex);
  plr.seek = -1;
  stop_flag = FALSE;
  playback->set_pb_ready (playback);
  g_mutex_unlock (control_mutex);

  // main playback loop
  dbg_printf ("loop.\n");
  while ((playing || conf.endless))
  {
    g_mutex_lock (control_mutex);

    if (stop_flag)
    {
        g_mutex_unlock (control_mutex);
        break;
    }

    // seek requested ?
    if (plr.seek != -1)
    {
      gint time = playback->output->written_time ();

      // backward seek ?
      if (plr.seek < time)
      {
        plr.p->rewind (plr.subsong);
        time = 0;
      }

      // seek to requested position
      while (time < plr.seek && plr.p->update ())
        time += (gint) (1000 / plr.p->getrefresh ());

      // Reset output plugin and some values
      playback->output->flush (time);
      plr.seek = -1;
      g_cond_signal (control_cond);
    }

    g_mutex_unlock (control_mutex);

    // fill sound buffer
    towrite = SNDBUFSIZE;
    sndbufpos = sndbuf;
    while (towrite > 0)
    {
      while (toadd < 0)
      {
        toadd += freq;
        playing = plr.p->update ();
      }
      i = MIN (towrite, (long) (toadd / plr.p->getrefresh () + 4) & ~3);
      opl.update ((short *) sndbufpos, i);
      sndbufpos += i * sampsize;
      towrite -= i;
      toadd -= (long) (plr.p->getrefresh () * i);
    }

    playback->output->write_audio (sndbuf, SNDBUFSIZE * sampsize);
  }

  while (playback->output->buffer_playing ())
      g_usleep (10000);

  g_mutex_lock (control_mutex);
  stop_flag = FALSE;
  g_cond_signal (control_cond); /* wake up any waiting request */
  g_mutex_unlock (control_mutex);

  // free everything and exit
  dbg_printf ("free");
  delete plr.p;
  plr.p = 0;
  free (sndbuf);
  dbg_printf (".\n");
  return TRUE;
}

// sampsize macro not useful anymore.
#undef sampsize

/***** Informational *****/

extern "C" int
adplug_is_our_fd (const gchar * filename, VFSFile * fd)
{
  CSilentopl tmpopl;

  CPlayer *p = factory (fd, &tmpopl);

  dbg_printf ("adplug_is_our_file(\"%s\"): returned ", filename);

  if (p)
  {
    delete p;
    dbg_printf ("TRUE\n");
    return TRUE;
  }

  dbg_printf ("FALSE\n");
  return FALSE;
}

/***** Player control *****/

extern "C" gboolean
adplug_play (InputPlayback * data, const gchar * filename, VFSFile * file, gint start_time, gint stop_time, gboolean pause)
{
  playback = data;
  dbg_printf ("adplug_play(\"%s\"): ", filename);
  audio_error = FALSE;

  // On new song, re-open "Song info" dialog, if open
  dbg_printf ("dialog, ");
  if (plr.infobox && (! plr.filename || strcmp (filename, plr.filename)))
    gtk_widget_destroy (GTK_WIDGET (plr.infodlg));

  // open output plugin
  dbg_printf ("open, ");
  if (!playback->output->
      open_audio (conf.bit16 ? FORMAT_16 : FORMAT_8, conf.freq,
                  conf.stereo ? 2 : 1))
  {
    audio_error = TRUE;
    return TRUE;
  }

  play_loop (playback, filename, file);
  playback->output->close_audio ();
  return FALSE;
}

extern "C" void adplug_stop (InputPlayback * p)
{
    g_mutex_lock (control_mutex);

    if (! stop_flag)
    {
        stop_flag = TRUE;
        p->output->abort_write ();
        g_cond_signal (control_cond);
        g_cond_wait (control_cond, control_mutex);
    }

    g_mutex_unlock (control_mutex);
}

extern "C" void adplug_pause (InputPlayback * p, gboolean pause)
{
    g_mutex_lock (control_mutex);

    if (! stop_flag)
        p->output->pause (pause);

    g_mutex_unlock (control_mutex);
}

extern "C" void adplug_mseek (InputPlayback * p, gint time)
{
    g_mutex_lock (control_mutex);

    if (! stop_flag)
    {
        plr.seek = time;
        p->output->abort_write();
        g_cond_signal (control_cond);
        g_cond_wait (control_cond, control_mutex);
    }

    g_mutex_unlock (control_mutex);
}

/***** Configuration file handling *****/

#define CFG_VERSION "AdPlug"

extern "C" gboolean adplug_init (void)
{
  conf.bit16 = aud_get_bool (CFG_VERSION, "16bit");
  conf.stereo = aud_get_bool (CFG_VERSION, "Stereo");
  conf.freq = aud_get_int (CFG_VERSION, "Frequency");
  conf.endless = aud_get_bool (CFG_VERSION, "Endless");

  // Read file type exclusion list
  dbg_printf ("exclusion, ");
  {
    gchar * cfgstr = aud_get_string (CFG_VERSION, "Exclude");

    if (cfgstr[0])
    {
        gchar * exclude = (gchar *) malloc (strlen (cfgstr) + 2);
        strcpy (exclude, cfgstr);
        exclude[strlen (exclude) + 1] = '\0';
        g_strdelimit (exclude, ":", '\0');
        for (gchar * p = exclude; *p; p += strlen (p) + 1)
            conf.players.remove (conf.players.lookup_filetype (p));
        free (exclude);
    }

    g_free (cfgstr);
  }

  // Load database from disk and hand it to AdPlug
  dbg_printf ("database");
  plr.db = new CAdPlugDatabase;

  {
    const char *homedir = getenv ("HOME");

    if (homedir)
    {
      std::string userdb;
      userdb = "file://" + std::string(g_get_home_dir()) + "/" ADPLUG_CONFDIR "/" + ADPLUGDB_FILE;
      if (vfs_file_test(userdb.c_str(),G_FILE_TEST_EXISTS)) {
      plr.db->load (userdb);    // load user's database
      dbg_printf (" (userdb=\"%s\")", userdb.c_str());
      }
    }
  }
  CAdPlug::set_database (plr.db);
  dbg_printf (".\n");

  control_mutex = g_mutex_new ();
  control_cond = g_cond_new ();
  return TRUE;
}

extern "C" void
adplug_quit (void)
{
  // Close database
  dbg_printf ("db, ");
  if (plr.db)
    delete plr.db;

  g_free (plr.filename);
  plr.filename = NULL;

  aud_set_bool (CFG_VERSION, "16bit", conf.bit16);
  aud_set_bool (CFG_VERSION, "Stereo", conf.stereo);
  aud_set_int (CFG_VERSION, "Frequency", conf.freq);
  aud_set_bool (CFG_VERSION, "Endless", conf.endless);

  dbg_printf ("exclude, ");
  std::string exclude;
  for (CPlayers::const_iterator i = CAdPlug::getPlayers().begin ();
       i != CAdPlug::getPlayers().end (); i++)
    if (find (conf.players.begin (), conf.players.end (), *i) ==
        conf.players.end ())
    {
      if (!exclude.empty ())
        exclude += ":";
      exclude += (*i)->filetype;
    }

  aud_set_string (CFG_VERSION, "Exclude", exclude.c_str ());

  g_mutex_free (control_mutex);
  g_cond_free (control_cond);
}
