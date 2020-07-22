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

#include <algorithm>
#include <sstream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <adplug/adplug.h>
#include <adplug/emuopl.h>
#include <adplug/silentopl.h>
#ifdef HAVE_ADPLUG_NEMUOPL_H
# include <adplug/nemuopl.h>
#endif
#ifdef HAVE_ADPLUG_WEMUOPL_H
# include <adplug/wemuopl.h>
#endif
#ifdef HAVE_ADPLUG_KEMUOPL_H
# include <adplug/kemuopl.h>
#endif
#include <adplug/players.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>
#include <libaudcore/preferences.h>

#include "adplug-xmms.h"

#define CFG_ID "AdPlug"

#define ADPLUG_MAME  0
#ifdef HAVE_ADPLUG_NEMUOPL_H
# define ADPLUG_NUKED 1
#endif
#ifdef HAVE_ADPLUG_WEMUOPL_H
# define ADPLUG_WOODY 2
#endif
#ifdef HAVE_ADPLUG_KEMUOPL_H
# define ADPLUG_KS    3
#endif

class AdPlugXMMS : public InputPlugin
{
public:
  static const char * const exts[];
  static const char * const defaults[];
  static const PreferencesWidget widgets[];
  static const PluginPreferences prefs;

  static constexpr PluginInfo info = {
    N_("AdPlug (AdLib Player)"),
    PACKAGE,
    nullptr,
    & prefs
  };

  constexpr AdPlugXMMS () : InputPlugin (info, InputInfo ()
    .with_exts (exts)) {}

  bool init ();
  void cleanup ();

  bool is_our_file (const char * filename, VFSFile & file);
  bool read_tag (const char * filename, VFSFile & file, Tuple & tuple, Index<char> * image);
  bool play (const char * filename, VFSFile & file);
};

EXPORT AdPlugXMMS aud_plugin_instance;

const char * const AdPlugXMMS::exts[] = {
  "a2m", "adl", "amd", "bam", "cff", "cmf", "d00", "dfm", "dmo", "dro",
  "dtm", "hsc", "hsp", "ins", "jbm", "ksm", "laa", "lds", "m", "mad",
  "mkj", "msc", "rad", "raw", "rix", "rol", "s3m", "sa2", "sat", "sci",
  "sng", "wlf", "xad", "xsm", nullptr
};

/***** Defines *****/

// Sound buffer size in samples
#define SNDBUFSIZE	512

// 4 byte sample size (16 bit depth & stereo)
#define SAMPLESIZE 4

// Default file name of AdPlug's database file
#define ADPLUGDB_FILE		"adplug.db"

// Default AdPlug user's configuration subdirectory
#define ADPLUG_CONFDIR		".adplug"

/***** Global variables *****/

// Player variables
static struct {
  SmartPtr<CPlayer> p;
  SmartPtr<CAdPlugDatabase> db;
  unsigned int subsong = 0, songlength = 0;
  String filename;
} plr;

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

/***** Main player (!! threaded !!) *****/

bool AdPlugXMMS::read_tag (const char * filename, VFSFile & file, Tuple & tuple,
 Index<char> * image)
{
  CSilentopl tmpopl;

  CFileVFSProvider fp (file);
  CPlayer *p = CAdPlug::factory (filename, &tmpopl, CAdPlug::players, fp);

  if (! p)
    return false;

  if (! p->getauthor().empty())
    tuple.set_str (Tuple::Artist, p->getauthor().c_str());

  if (! p->gettitle().empty())
    tuple.set_str (Tuple::Title, p->gettitle().c_str());
  else if (! p->getdesc().empty())
    tuple.set_str (Tuple::Title, p->getdesc().c_str());

  tuple.set_str (Tuple::Codec, p->gettype().c_str());
  tuple.set_str (Tuple::Quality, _("sequenced"));
  tuple.set_int (Tuple::Length, p->songlength (plr.subsong));
  tuple.set_int (Tuple::Channels, 2);
  delete p;

  return true;
}

/* Main playback thread. Takes the filename to play as argument. */
bool AdPlugXMMS::play (const char * filename, VFSFile & fd)
{
  dbg_printf ("adplug_play(\"%s\"): ", filename);

  int emulator = aud_get_int (CFG_ID, "Emulator");
  int freq = aud_get_int (CFG_ID, "Frequency");
  bool endless = aud_get_bool (CFG_ID, "Endless");

  // Set XMMS main window information
  dbg_printf ("xmms, ");
  set_stream_bitrate (freq * SAMPLESIZE * 8);

  // open output plugin
  dbg_printf ("open, ");
  open_audio (FMT_S16_NE, freq, 2);

  SmartPtr<Copl> opl;
  switch (emulator) {
#ifdef HAVE_ADPLUG_NEMUOPL_H
    case ADPLUG_NUKED:
      opl.capture(new CNemuopl (freq));
      break;
#endif
#ifdef HAVE_ADPLUG_WEMUOPL_H
    case ADPLUG_WOODY:
      opl.capture(new CWemuopl (freq, true, true));
      break;
#endif
#ifdef HAVE_ADPLUG_KEMUOPL_H
    case ADPLUG_KS:
      opl.capture(new CKemuopl (freq, true, true));
      break;
#endif
    case ADPLUG_MAME:
    default:
      opl.capture(new CEmuopl (freq, true, true));
      // otherwise sound only comes out of left
      static_cast<CEmuopl *>(opl.get())->settype(Copl::TYPE_OPL2);
  }

  long toadd = 0, i, towrite;
  char *sndbuf, *sndbufpos;
  bool playing = true;  // Song self-end indicator.

  // Try to load module
  dbg_printf ("factory, ");
  CFileVFSProvider fp (fd);
  if (!(plr.p.capture(CAdPlug::factory (filename, opl.get (), CAdPlug::players, fp))))
  {
    dbg_printf ("error!\n");
    // MessageBox("AdPlug :: Error", "File could not be opened!", "Ok");
    return false;
  }

  // reset to first subsong on new file
  dbg_printf ("subsong, ");
  if (! plr.filename || strcmp (filename, plr.filename))
  {
    plr.filename = String (filename);
    plr.subsong = 0;
  }

  // Allocate audio buffer
  dbg_printf ("buffer, ");
  // 4 byte sample size
  sndbuf = (char *) malloc (SNDBUFSIZE * 4);

  // Rewind player to right subsong
  dbg_printf ("rewind, ");
  plr.p->rewind (plr.subsong);

  int time = 0;

  // main playback loop
  dbg_printf ("loop.\n");
  while ((playing || endless))
  {
    if (check_stop ())
      break;

    int seek = check_seek ();

    // seek requested ?
    if (seek != -1)
    {
      // backward seek ?
      if (seek < time)
      {
        plr.p->rewind (plr.subsong);
        time = 0;
      }

      // seek to requested position
      while (time < seek && plr.p->update ())
        time += (int) (1000 / plr.p->getrefresh ());
    }

    // fill sound buffer
    towrite = SNDBUFSIZE;
    sndbufpos = sndbuf;
    while (towrite > 0)
    {
      while (toadd < 0)
      {
        toadd += freq;
        playing = plr.p->update ();
        if (playing)
          time += (int) (1000 / plr.p->getrefresh ());
      }
      i = std::min (towrite, (long) (toadd / plr.p->getrefresh () + 4) & ~3);
      opl->update ((short *) sndbufpos, i);
      sndbufpos += i * SAMPLESIZE;
      towrite -= i;
      toadd -= (long) (plr.p->getrefresh () * i);
    }

    write_audio (sndbuf, SNDBUFSIZE * SAMPLESIZE);
  }

  // free everything and exit
  dbg_printf ("free");
  plr.p.clear ();
  free (sndbuf);
  dbg_printf (".\n");
  return true;
}

/***** Informational *****/

bool AdPlugXMMS::is_our_file (const char * filename, VFSFile & fd)
{
  CSilentopl tmpopl;

  CFileVFSProvider fp (fd);
  CPlayer *p = CAdPlug::factory (filename, &tmpopl, CAdPlug::players, fp);

  dbg_printf ("adplug_is_our_file(\"%s\"): returned ", filename);

  if (p)
  {
    delete p;
    dbg_printf ("true\n");
    return true;
  }

  dbg_printf ("false\n");
  return false;
}

/***** Configuration file handling *****/

const char * const AdPlugXMMS::defaults[] = {
 "Frequency", "44100",
 "Endless", "FALSE",
 "Emulator", "0",
 nullptr};

/***** Configuration UI *****/

static const ComboItem plugin_combo[] = {
  ComboItem ("Tatsuyuki Satoh 0.72 (MAME, 2003)", ADPLUG_MAME),
#ifdef HAVE_ADPLUG_NEMUOPL_H
  ComboItem ("Nuked OPL3 (Nuke.YKT, 2018)", ADPLUG_NUKED),
#endif
#ifdef HAVE_ADPLUG_WEMUOPL_H
  ComboItem ("WoodyOPL (DOSBox, 2016)", ADPLUG_WOODY),
#endif
#ifdef HAVE_ADPLUG_KEMUOPL_H
  ComboItem ("Ken Silverman (2001)", ADPLUG_KS),
#endif
};

const PreferencesWidget AdPlugXMMS::widgets[] = {
  WidgetLabel (N_("<b>Output</b>")),
  WidgetCombo (N_("OPL Emulator:"),
    WidgetInt (CFG_ID, "Emulator"),
    {{plugin_combo}}),
  WidgetSpin (N_("Sample rate"),
    WidgetInt (CFG_ID, "Frequency"), {8000, 192000, 50, N_("Hz")}),
  WidgetLabel (N_("<b>Miscellaneous</b>")),
  WidgetCheck (N_("Repeat song in endless loop"),
    WidgetBool (CFG_ID, "Endless"))
};

const PluginPreferences AdPlugXMMS::prefs = {{widgets}};

bool AdPlugXMMS::init ()
{
  aud_config_set_defaults (CFG_ID, defaults);

  // Load database from disk and hand it to AdPlug
  dbg_printf ("database");
  {
    const char *homedir = getenv ("HOME");

    if (homedir)
    {
      std::string userdb;
      userdb = std::string ("file://") + homedir + "/" ADPLUG_CONFDIR "/" + ADPLUGDB_FILE;

      if (VFSFile::test_file (userdb.c_str (), VFS_EXISTS))
      {
        plr.db.capture(new CAdPlugDatabase);
        plr.db->load (userdb);    // load user's database
        dbg_printf (" (userdb=\"%s\")", userdb.c_str());
        CAdPlug::set_database (plr.db.get ());
      }
    }
  }
  dbg_printf (".\n");

  return true;
}

void AdPlugXMMS::cleanup ()
{
  // Close database
  dbg_printf ("db, ");
  plr.db.clear ();
  plr.filename = String ();
}
