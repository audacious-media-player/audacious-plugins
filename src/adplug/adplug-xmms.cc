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

#ifdef USE_GTK
#include <gtk/gtk.h>
#endif

#ifdef USE_QT
#include <QVBoxLayout>
#include <QCheckBox>
#include <libaudqt/libaudqt.h>
#endif

#include <adplug/adplug.h>
#include <adplug/emuopl.h>
#include <adplug/silentopl.h>
#include <adplug/nemuopl.h>
#include <adplug/wemuopl.h>
#include <adplug/kemuopl.h>
#include <adplug/players.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>
#include <libaudcore/preferences.h>

#include "adplug-xmms.h"

#define CFG_ID "AdPlug"

#define ADPLUG_NUKED 0
#define ADPLUG_WOODY 1
#define ADPLUG_MAME  2
#define ADPLUG_KS    3

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

// AdPlug's 8 and 16 bit audio formats
#define FORMAT_8	FMT_U8
#define FORMAT_16	FMT_S16_NE

// Default file name of AdPlug's database file
#define ADPLUGDB_FILE		"adplug.db"

// Default AdPlug user's configuration subdirectory
#define ADPLUG_CONFDIR		".adplug"

/***** Global variables *****/

// Player variables
static struct {
  CPlayer *p = nullptr;
  CAdPlugDatabase *db = nullptr;
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
  delete p;

  return true;
}

/* Main playback thread. Takes the filename to play as argument. */
bool AdPlugXMMS::play (const char * filename, VFSFile & fd)
{
  dbg_printf ("adplug_play(\"%s\"): ", filename);

  int emulator = aud_get_int (CFG_ID, "Emulator");
  // ADPLUG_NUKED only emits 16 bit
  bool bit16 = emulator == ADPLUG_NUKED ? true : aud_get_bool (CFG_ID, "16bit");
  // ADPLUG_NUKED only emits stereo
  bool stereo = emulator == ADPLUG_NUKED ? true : aud_get_bool (CFG_ID, "Stereo");
  int freq = aud_get_int (CFG_ID, "Frequency");
  bool endless = aud_get_bool (CFG_ID, "Endless");

  // Set XMMS main window information
  dbg_printf ("xmms, ");
  int sampsize = (bit16 ? 2 : 1) * (stereo ? 2 : 1);
  set_stream_bitrate (freq * sampsize * 8);

  // open output plugin
  dbg_printf ("open, ");
  open_audio (bit16 ? FORMAT_16 : FORMAT_8, freq, stereo ? 2 : 1);

  Copl *opl = nullptr;
  switch (emulator) {
    case ADPLUG_NUKED:
      opl = new CNemuopl (freq);
      break;
    case ADPLUG_WOODY:
      opl = new CWemuopl (freq, bit16, stereo);
      break;
    case ADPLUG_MAME:
      opl = new CEmuopl (freq, bit16, stereo);
      break;
    case ADPLUG_KS:
      opl = new CKemuopl (freq, bit16, stereo);
      break;
  }

  long toadd = 0, i, towrite;
  char *sndbuf, *sndbufpos;
  bool playing = true;  // Song self-end indicator.

  // Try to load module
  dbg_printf ("factory, ");
  CFileVFSProvider fp (fd);
  if (!(plr.p = CAdPlug::factory (filename, opl, CAdPlug::players, fp)))
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
  sndbuf = (char *) malloc (SNDBUFSIZE * sampsize);

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
      sndbufpos += i * sampsize;
      towrite -= i;
      toadd -= (long) (plr.p->getrefresh () * i);
    }

    write_audio (sndbuf, SNDBUFSIZE * sampsize);
  }

  // free everything and exit
  dbg_printf ("free");
  delete plr.p;
  delete opl;
  plr.p = 0;
  free (sndbuf);
  dbg_printf (".\n");
  return true;
}

// sampsize macro not useful anymore.
#undef sampsize

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
 "16bit", "TRUE",
 "Stereo", "FALSE",
 "Frequency", "44100",
 "Endless", "FALSE",
 "Emulator", "0",
 nullptr};

/***** Configuration UI *****/

#ifdef USE_GTK
static GtkWidget * output_16bit_cbtn, * output_stereo_cbtn;

void emulator_changed ()
{
    if (aud_get_int (CFG_ID, "Emulator") == ADPLUG_NUKED) {
        aud_set_bool (CFG_ID, "16bit", true);
        aud_set_bool (CFG_ID, "Stereo", true);
        gtk_toggle_button_set_active ((GtkToggleButton *) output_16bit_cbtn,
            true);
        gtk_widget_set_sensitive (output_16bit_cbtn, false);
        gtk_toggle_button_set_active ((GtkToggleButton *) output_stereo_cbtn,
            true);
        gtk_widget_set_sensitive (output_stereo_cbtn, false);
    }
    else {
        gtk_widget_set_sensitive (output_16bit_cbtn, true);
        gtk_widget_set_sensitive (output_stereo_cbtn, true);
    }
}

void optional_check_changed (GtkWidget * widget, const void * ignored)
{
    if (widget == output_16bit_cbtn)
        aud_set_bool (CFG_ID, "16bit", 
            gtk_toggle_button_get_active ((GtkToggleButton *) widget));
    else if (widget == output_stereo_cbtn)
        aud_set_bool (CFG_ID, "Stereo", 
            gtk_toggle_button_get_active ((GtkToggleButton *) widget));
}

void * create_optional_checks ()
{
    GtkWidget * output_checks_box;

    output_checks_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    output_16bit_cbtn = gtk_check_button_new_with_label (
        N_("16-bit output (if unchecked, output is 8-bit)"));
    gtk_toggle_button_set_active ((GtkToggleButton *) output_16bit_cbtn,
        aud_get_bool (CFG_ID, "16bit"));
    g_signal_connect (output_16bit_cbtn, "toggled", 
        (GCallback) optional_check_changed, 0);
    output_stereo_cbtn = gtk_check_button_new_with_label (
        N_("Duplicate mono output to two channels"));
    gtk_toggle_button_set_active ((GtkToggleButton *) output_stereo_cbtn,
        aud_get_bool (CFG_ID, "Stereo"));
    g_signal_connect (output_stereo_cbtn, "toggled", 
        (GCallback) optional_check_changed, 0);
    gtk_box_pack_start ((GtkBox *) output_checks_box, output_16bit_cbtn, false,
        false, 0);
    gtk_box_pack_start ((GtkBox *) output_checks_box, output_stereo_cbtn, false,
        false, 0);

    emulator_changed ();

    return output_checks_box;
}
#endif

#ifdef USE_QT
static QCheckBox * output_16bit_cbtn, * output_stereo_cbtn;

void emulator_changed ()
{
    if (aud_get_int (CFG_ID, "Emulator") == ADPLUG_NUKED) {
        aud_set_bool (CFG_ID, "16bit", true);
        aud_set_bool (CFG_ID, "Stereo", true);
        output_16bit_cbtn->setCheckState(Qt::Checked);
        output_16bit_cbtn->setEnabled (false);
        output_stereo_cbtn->setCheckState(Qt::Checked);
        output_stereo_cbtn->setEnabled (false);
    }
    else {
        output_16bit_cbtn->setEnabled (true);
        output_stereo_cbtn->setEnabled (true);
    }
}

void * create_optional_checks ()
{
    QWidget * output_checks_widget;
    QVBoxLayout * output_checks_box;

    output_checks_widget = new QWidget ();
    output_checks_box = audqt::make_vbox (output_checks_widget, audqt::sizes.TwoPt);
    output_16bit_cbtn = new QCheckBox (
        N_("16-bit output (if unchecked, output is 8-bit)"));
    output_16bit_cbtn->setCheckState(
        aud_get_bool (CFG_ID, "16bit") ? Qt::Checked : Qt::Unchecked);
    QObject::connect (output_16bit_cbtn, & QCheckBox::stateChanged, 
        [] (int state) {
            aud_set_bool (CFG_ID, "16bit", state != Qt::Unchecked);
        });
    output_stereo_cbtn = new QCheckBox (
        N_("Duplicate mono output to two channels"));
    output_stereo_cbtn->setCheckState(
        aud_get_bool (CFG_ID, "Stereo") ? Qt::Checked : Qt::Unchecked);
    QObject::connect (output_stereo_cbtn, & QCheckBox::stateChanged, 
        [] (int state) {
            aud_set_bool (CFG_ID, "Stereo", state != Qt::Unchecked);
        });
    output_checks_box->addWidget(output_16bit_cbtn);
    output_checks_box->addWidget(output_stereo_cbtn);

    emulator_changed ();

    return output_checks_widget;
}
#endif

static const ComboItem plugin_combo[] = {
    ComboItem ("Nuked OPL3 (Nuke.YKT, 2018)", ADPLUG_NUKED),
    ComboItem ("WoodyOPL (DOSBox, 2016)", ADPLUG_WOODY),
    ComboItem ("Tatsuyuki Satoh 0.72 (MAME, 2003)", ADPLUG_MAME),
    ComboItem ("Ken Silverman (2001)", ADPLUG_KS),
};

const PreferencesWidget AdPlugXMMS::widgets[] = {
    WidgetLabel (N_("<b>Output</b>")),
    WidgetCombo (N_("OPL Emulator:"),
        WidgetInt (CFG_ID, "Emulator", emulator_changed),
        {{plugin_combo}}),
#ifdef USE_GTK
    WidgetCustomGTK (create_optional_checks),
#endif
#ifdef USE_QT
    WidgetCustomQt (create_optional_checks),
#endif
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
        plr.db = new CAdPlugDatabase;
        plr.db->load (userdb);    // load user's database
        dbg_printf (" (userdb=\"%s\")", userdb.c_str());
        CAdPlug::set_database (plr.db);
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
  if (plr.db)
    delete plr.db;

  plr.filename = String ();
}
