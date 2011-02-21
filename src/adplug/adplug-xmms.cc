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
#include <audacious/configdb.h>
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
44100l, true, false, false, CAdPlug::players};

// Player variables
static struct
{
  CPlayer *p;
  CAdPlugDatabase *db;
  unsigned int subsong, songlength;
  int seek;
  char filename[PATH_MAX];
  GtkLabel *infobox;
  GtkDialog *infodlg;
} plr = {0, 0, 0, 0, -1, "", NULL, NULL};

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

/***** [Dialog]: Utility functions *****/

static GtkWidget *
make_framed (GtkWidget * what, const gchar * label)
{
  GtkWidget *framebox = gtk_frame_new (label);

  gtk_container_add (GTK_CONTAINER (framebox), what);
  return framebox;
}

static GtkWidget *
print_left (const gchar * text)
{
  GtkLabel *label = GTK_LABEL (gtk_label_new (text));

  gtk_label_set_justify (label, GTK_JUSTIFY_LEFT);
  gtk_misc_set_padding (GTK_MISC (label), 2, 2);
  return GTK_WIDGET (label);
}

/***** Dialog boxes *****/

extern "C" void
adplug_about (void)
{
  if (!about_win)
  {
    gchar *about_title = g_strjoin ("", _("About "), ADPLUG_NAME, NULL);
    const gchar *version_text = CAdPlug::get_version ().c_str ();
    gchar *about_text = g_strjoin ("", ADPLUG_NAME,
                                   _
                                   ("\nCopyright (C) 2002, 2003 Simon Peter <dn.tlp@gmx.net>\n\n"
                                    "This plugin is released under the terms and conditions of the GNU LGPL.\n"
                                    "See http://www.gnu.org/licenses/lgpl.html for details."
                                    "\n\nThis plugin uses the AdPlug library, which is copyright (C) Simon Peter, et al.\n"
                                    "Linked AdPlug library version: "),
                                   version_text, NULL);
    audgui_simple_message (& about_win, GTK_MESSAGE_INFO, about_title, about_text);
    g_free (about_text);
    g_free (about_title);
  }
}

static void
close_config_box_ok (GtkButton * button, GPtrArray * rblist)
{
  // Apply configuration settings
  conf.bit16 =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
                                  (g_ptr_array_index (rblist, 0)));
  conf.stereo =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
                                  (g_ptr_array_index (rblist, 1)));

  if (gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (g_ptr_array_index (rblist, 2))))
    conf.freq = 11025;
  if (gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (g_ptr_array_index (rblist, 3))))
    conf.freq = 22050;
  if (gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (g_ptr_array_index (rblist, 4))))
    conf.freq = 44100;
  if (gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (g_ptr_array_index (rblist, 5))))
    conf.freq = 48000;

  conf.endless =
    !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
                                   (g_ptr_array_index (rblist, 6)));

  conf.players = *(CPlayers *) g_ptr_array_index (rblist, 7);
  delete (CPlayers *) g_ptr_array_index (rblist, 7);

  g_ptr_array_free (rblist, FALSE);
}

static void
close_config_box_cancel (GtkButton * button, GPtrArray * rblist)
{
  delete (CPlayers *) g_ptr_array_index (rblist, 7);
  g_ptr_array_free (rblist, FALSE);
}

static void
config_fl_row_select (GtkCList * fl, gint row, gint col,
                      GdkEventButton * event, CPlayers * pl)
{
  pl->push_back ((CPlayerDesc *) gtk_clist_get_row_data (fl, row));
  pl->unique ();
}

static void
config_fl_row_unselect (GtkCList * fl, gint row, gint col,
                        GdkEventButton * event, CPlayers * pl)
{
  pl->remove ((CPlayerDesc *) gtk_clist_get_row_data (fl, row));
}

extern "C" void
adplug_config (void)
{
  GtkDialog *config_dlg = GTK_DIALOG (gtk_dialog_new ());
  GtkNotebook *notebook = GTK_NOTEBOOK (gtk_notebook_new ());
  GtkTable *table;
  GtkTooltips *tooltips = gtk_tooltips_new ();
  GPtrArray *rblist = g_ptr_array_new ();

  gtk_window_set_title (GTK_WINDOW (config_dlg), _("AdPlug :: Configuration"));
  gtk_window_set_policy (GTK_WINDOW (config_dlg), FALSE, FALSE, TRUE);  // Window is auto sized
  gtk_window_set_modal (GTK_WINDOW (config_dlg), TRUE);
  gtk_container_add (GTK_CONTAINER (config_dlg->vbox), GTK_WIDGET (notebook));

  // Add Ok & Cancel buttons
  {
    GtkWidget *button;

    button = gtk_button_new_with_label (_("Ok"));
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (close_config_box_ok), (gpointer) rblist);
    g_signal_connect_data (G_OBJECT (button), "clicked",
                           G_CALLBACK (gtk_widget_destroy),
                           GTK_OBJECT (config_dlg), NULL,
                           (GConnectFlags) (G_CONNECT_AFTER |
                                            G_CONNECT_SWAPPED));
    gtk_container_add (GTK_CONTAINER (config_dlg->action_area), button);

    button = gtk_button_new_with_label (_("Cancel"));
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (close_config_box_cancel),
                      (gpointer) rblist);
    g_signal_connect_swapped (G_OBJECT (button), "clicked",
                              G_CALLBACK (gtk_widget_destroy),
                              GTK_OBJECT (config_dlg));
    gtk_container_add (GTK_CONTAINER (config_dlg->action_area), button);
  }

  /***** Page 1: General *****/

  table = GTK_TABLE (gtk_table_new (1, 2, TRUE));
  gtk_table_set_row_spacings (table, 5);
  gtk_table_set_col_spacings (table, 5);
  gtk_notebook_append_page (notebook, GTK_WIDGET (table),
                            print_left (_("General")));

  // Add "Sound quality" section
  {
    GtkTable *sqt = GTK_TABLE (gtk_table_new (2, 2, FALSE));
    GtkVBox *fvb;
    GtkRadioButton *rb;

    gtk_table_set_row_spacings (sqt, 5);
    gtk_table_set_col_spacings (sqt, 5);
    gtk_table_attach_defaults (table,
                               make_framed (GTK_WIDGET (sqt),
                                            _("Sound quality")), 0, 1, 0, 1);

    // Add "Resolution" section
    fvb = GTK_VBOX (gtk_vbox_new (TRUE, 0));
    gtk_table_attach_defaults (sqt,
                               make_framed (GTK_WIDGET (fvb), _("Resolution")),
                               0, 1, 0, 1);
    rb = GTK_RADIO_BUTTON (gtk_radio_button_new_with_label (NULL, _("8bit")));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb), !conf.bit16);
    gtk_container_add (GTK_CONTAINER (fvb), GTK_WIDGET (rb));
    rb =
      GTK_RADIO_BUTTON (gtk_radio_button_new_with_label_from_widget
                        (rb, _("16bit")));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb), conf.bit16);
    gtk_container_add (GTK_CONTAINER (fvb), GTK_WIDGET (rb));
    g_ptr_array_add (rblist, (gpointer) rb);

    // Add "Channels" section
    fvb = GTK_VBOX (gtk_vbox_new (TRUE, 0));
    gtk_table_attach_defaults (sqt,
                               make_framed (GTK_WIDGET (fvb), _("Channels")), 0,
                               1, 1, 2);
    rb = GTK_RADIO_BUTTON (gtk_radio_button_new_with_label (NULL, _("Mono")));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb), !conf.stereo);
    gtk_container_add (GTK_CONTAINER (fvb), GTK_WIDGET (rb));
    rb =
      GTK_RADIO_BUTTON (gtk_radio_button_new_with_label_from_widget
                        (rb, _("Stereo")));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb), conf.stereo);
    gtk_container_add (GTK_CONTAINER (fvb), GTK_WIDGET (rb));
    gtk_tooltips_set_tip (tooltips, GTK_WIDGET (rb),
                          _("Setting stereo is not recommended, unless you need to. "
                          "This won't add any stereo effects to the sound - OPL2 "
                          "is just mono - but eats up more CPU power!"), NULL);
    g_ptr_array_add (rblist, (gpointer) rb);

    // Add "Frequency" section
    fvb = GTK_VBOX (gtk_vbox_new (TRUE, 0));
    gtk_table_attach_defaults (sqt,
                               make_framed (GTK_WIDGET (fvb), _("Frequency")), 1,
                               2, 0, 2);
    rb = GTK_RADIO_BUTTON (gtk_radio_button_new_with_label (NULL, "11025"));
    if (conf.freq == 11025)
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb), TRUE);
    gtk_container_add (GTK_CONTAINER (fvb), GTK_WIDGET (rb));
    g_ptr_array_add (rblist, (gpointer) rb);
    rb =
      GTK_RADIO_BUTTON (gtk_radio_button_new_with_label_from_widget
                        (rb, "22050"));
    if (conf.freq == 22050)
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb), TRUE);
    gtk_container_add (GTK_CONTAINER (fvb), GTK_WIDGET (rb));
    g_ptr_array_add (rblist, (gpointer) rb);
    rb =
      GTK_RADIO_BUTTON (gtk_radio_button_new_with_label_from_widget
                        (rb, "44100"));
    if (conf.freq == 44100)
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb), TRUE);
    gtk_container_add (GTK_CONTAINER (fvb), GTK_WIDGET (rb));
    g_ptr_array_add (rblist, (gpointer) rb);
    rb =
      GTK_RADIO_BUTTON (gtk_radio_button_new_with_label_from_widget
                        (rb, "48000"));
    if (conf.freq == 48000)
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb), TRUE);
    gtk_container_add (GTK_CONTAINER (fvb), GTK_WIDGET (rb));
    g_ptr_array_add (rblist, (gpointer) rb);
  }

  // Add "Playback" section
  {
    GtkVBox *vb = GTK_VBOX (gtk_vbox_new (FALSE, 0));
    GtkCheckButton *cb;

    gtk_table_attach_defaults (table,
                               make_framed (GTK_WIDGET (vb), _("Playback")), 1,
                               2, 0, 1);

    cb =
      GTK_CHECK_BUTTON (gtk_check_button_new_with_label (_("Detect songend")));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb), !conf.endless);
    gtk_container_add (GTK_CONTAINER (vb), GTK_WIDGET (cb));
    gtk_tooltips_set_tip (tooltips, GTK_WIDGET (cb),
                          _("If enabled, XMMS will detect a song's ending, stop "
                          "it and advance in the playlist. If disabled, XMMS "
                          "won't take notice of a song's ending and loop it all "
                          "over again and again."), NULL);
    g_ptr_array_add (rblist, (gpointer) cb);
  }

  /***** Page 2: Formats *****/

  table = GTK_TABLE (gtk_table_new (1, 1, TRUE));
  gtk_notebook_append_page (notebook, GTK_WIDGET (table),
                            print_left (_("Formats")));

  // Add "Format selection" section
  {
    GtkHBox *vb = GTK_HBOX (gtk_hbox_new (FALSE, 0));
    gtk_table_attach_defaults (table,
                               make_framed (GTK_WIDGET (vb),
                                            _("Format selection")), 0, 1, 0, 1);
    // Add scrollable list
    {
      const gchar *rowstr[] = { _("Format"), _("Extension") };
      GtkEventBox *eventbox = GTK_EVENT_BOX (gtk_event_box_new ());
      GtkScrolledWindow *formatswnd =
        GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
      GtkCList *fl = GTK_CLIST (gtk_clist_new_with_titles (2, (gchar **)rowstr));
      CPlayers::const_iterator i;
      unsigned int j;
      gtk_clist_set_selection_mode (fl, GTK_SELECTION_MULTIPLE);

      // Build list
      for (i = CAdPlug::players.begin (); i != CAdPlug::players.end (); i++)
      {
        gint rownum;

        gchar *rws[2];
        rws[0] = g_strdup ((*i)->filetype.c_str ());
        rws[1] = g_strdup ((*i)->get_extension (0));
        for (j = 1; (*i)->get_extension (j); j++)
          rws[1] = g_strjoin (", ", rws[1], (*i)->get_extension (j), NULL);
        rownum = gtk_clist_append (fl, rws);
        g_free (rws[0]);
        g_free (rws[1]);
        gtk_clist_set_row_data (fl, rownum, (gpointer) (*i));
        if (find (conf.players.begin (), conf.players.end (), *i) !=
            conf.players.end ())
          gtk_clist_select_row (fl, rownum, 0);
      }

      gtk_clist_columns_autosize (fl);
      gtk_scrolled_window_set_policy (formatswnd, GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      gpointer pl = (gpointer) new CPlayers (conf.players);
      g_signal_connect (G_OBJECT (fl), "select-row",
                        G_CALLBACK (config_fl_row_select), pl);
      g_signal_connect (G_OBJECT (fl), "unselect-row",
                        G_CALLBACK (config_fl_row_unselect), pl);
      gtk_container_add (GTK_CONTAINER (formatswnd), GTK_WIDGET (fl));
      gtk_container_add (GTK_CONTAINER (eventbox), GTK_WIDGET (formatswnd));
      gtk_container_add (GTK_CONTAINER (vb), GTK_WIDGET (eventbox));
      gtk_tooltips_set_tip (tooltips, GTK_WIDGET (eventbox),
                            _("Selected file types will be recognized and played "
                            "back by this plugin. Deselected types will be "
                            "ignored to make room for other plugins to play "
                            "these files."), NULL);
      g_ptr_array_add (rblist, pl);
    }
  }

  // Show window
  gtk_widget_show_all (GTK_WIDGET (config_dlg));
}

static void
add_instlist (GtkCList * instlist, const char *t1, const char *t2)
{
  gchar *rowstr[2];

  rowstr[0] = g_strdup (t1);
  rowstr[1] = g_strdup (t2);
  gtk_clist_append (instlist, rowstr);
  g_free (rowstr[0]);
  g_free (rowstr[1]);
}

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

#if 0
static void
subsong_slider (GtkAdjustment * adj)
{
  adplug_stop (NULL);
  plr.subsong = (unsigned int) adj->value - 1;
  adplug_play (playback, playback->filename, NULL, 0, 0, FALSE);
}
#endif

static void
close_infobox (GtkDialog * infodlg)
{
  // Forget our references to the instance of the "currently playing song" info
  // box. But only if we're really destroying that one... ;)
  if (infodlg == plr.infodlg)
  {
    plr.infobox = NULL;
    plr.infodlg = NULL;
  }
}

extern "C" void
adplug_info_box (const gchar *filename)
{
  CSilentopl tmpopl;
  VFSFile *fd = vfs_buffered_file_new_from_uri (filename);

  if (!fd)
    return;

  CPlayer *p = (strcmp (filename, plr.filename) || !plr.p) ?
    factory (fd, &tmpopl) : plr.p;

  if (!p)
    return;                     // bail out if no player could be created
  if (p == plr.p && plr.infodlg)
    return;                     // only one info box for active song

  std::ostringstream infotext;
  unsigned int i;
  GtkDialog *infobox = GTK_DIALOG (gtk_dialog_new ());
  GtkButton *okay_button = GTK_BUTTON (gtk_button_new_with_label (_("Ok")));

  GtkVBox *box = GTK_VBOX (gtk_vbox_new (TRUE, 2));
  GtkHBox *hbox = GTK_HBOX (gtk_hbox_new (TRUE, 2));
  GtkHBox *hbox2 = GTK_HBOX (gtk_hbox_new (TRUE, 2));

  // Build file info box
  gtk_window_set_title (GTK_WINDOW (infobox), _("AdPlug :: File Info"));
  gtk_window_set_policy (GTK_WINDOW (infobox), FALSE, FALSE, TRUE); // Window is auto sized

  gtk_container_add (GTK_CONTAINER (infobox->vbox), GTK_WIDGET (box));
// Former packer layout, for future reproduction
//  gtk_packer_set_default_border_width(packer, 2);

  gtk_box_set_homogeneous (GTK_BOX (hbox), FALSE);
  g_signal_connect_swapped (G_OBJECT (okay_button), "clicked",
                            G_CALLBACK (gtk_widget_destroy),
                            GTK_OBJECT (infobox));
  g_signal_connect (G_OBJECT (infobox), "destroy",
                    G_CALLBACK (close_infobox), 0);
  gtk_container_add (GTK_CONTAINER (infobox->action_area),
                     GTK_WIDGET (okay_button));

  // Add filename section
// Former packer layout, for future reproduction
//  gtk_packer_add_defaults(packer, make_framed(print_left(filename), "Filename"),
//            GTK_SIDE_TOP, GTK_ANCHOR_CENTER, GTK_FILL_X);
  gtk_box_pack_end (GTK_BOX (box), GTK_WIDGET (hbox2), TRUE, TRUE, 2);
  gtk_box_pack_end (GTK_BOX (box),
                    make_framed (print_left (filename), _("Filename")), TRUE,
                    TRUE, 2);

  // Add "Song info" section
  infotext << _("Title: ") << p->gettitle () << std::endl <<
    _("Author: ") << p->getauthor () << std::endl <<
    _("File Type: ") << p->gettype () << std::endl <<
    _("Subsongs: ") << p->getsubsongs () << std::endl <<
    _("Instruments: ") << p->getinstruments ();
  if (plr.p == p)
    infotext << std::ends;
  else
  {
    infotext << std::endl << _("Orders: ") << p->getorders () << std::endl <<
      _("Patterns: ") << p->getpatterns () << std::ends;
  }
  gtk_container_add (GTK_CONTAINER (hbox),
                     make_framed (print_left (infotext.str ().c_str ()),
                                  _("Song")));

  // Add "Playback info" section if currently playing
  if (plr.p == p)
  {
    plr.infobox = GTK_LABEL (gtk_label_new (""));
    gtk_label_set_justify (plr.infobox, GTK_JUSTIFY_LEFT);
    gtk_misc_set_padding (GTK_MISC (plr.infobox), 2, 2);
    gtk_container_add (GTK_CONTAINER (hbox),
                       make_framed (GTK_WIDGET (plr.infobox), _("Playback")));
  }

// Former packer layout, for future reproduction
//  gtk_packer_add_defaults(packer, GTK_WIDGET(hbox), GTK_SIDE_TOP,
//            GTK_ANCHOR_CENTER, GTK_FILL_X);
  gtk_box_pack_end (GTK_BOX (hbox2), GTK_WIDGET (hbox), TRUE, TRUE, 2);

  // Add instrument names section
  if (p->getinstruments ())
  {
    GtkScrolledWindow *instwnd =
      GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
    GtkCList *instnames;
    gchar tmpstr[10];

    {
      const gchar *rowstr[] = { "#", _("Instrument name") };
      instnames = GTK_CLIST (gtk_clist_new_with_titles (2, (gchar **)rowstr));
    }
    gtk_clist_set_column_justification (instnames, 0, GTK_JUSTIFY_RIGHT);

    for (i = 0; i < p->getinstruments (); i++)
    {
      sprintf (tmpstr, "%d", i + 1);
      add_instlist (instnames, tmpstr, p->getinstrument (i).c_str ());
    }

    gtk_clist_columns_autosize (instnames);
    gtk_scrolled_window_set_policy (instwnd, GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (instwnd), GTK_WIDGET (instnames));
// Former packer layout, for future reproduction
//    gtk_packer_add(packer, GTK_WIDGET(instwnd), GTK_SIDE_TOP,
//         GTK_ANCHOR_CENTER, GTK_FILL_X, 0, 0, 0, 0, 50);
    gtk_box_pack_end (GTK_BOX (hbox2), GTK_WIDGET (instwnd), TRUE, TRUE, 2);
  }

  // Add "Song message" section
  if (!p->getdesc ().empty ())
  {
    GtkScrolledWindow *msgwnd =
      GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
    GtkTextView *msg = GTK_TEXT_VIEW (gtk_text_view_new ());

    gtk_scrolled_window_set_policy (msgwnd, GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_text_view_set_editable (msg, FALSE);
    gtk_text_view_set_wrap_mode (msg, GTK_WRAP_WORD_CHAR);

    gtk_text_buffer_set_text (gtk_text_view_get_buffer (msg),
                              p->getdesc ().c_str (),
                              p->getdesc ().length ());
    gtk_container_add (GTK_CONTAINER (msgwnd), GTK_WIDGET (msg));

// Former packer layout, for future reproduction
//    gtk_packer_add(packer, make_framed(GTK_WIDGET(msgwnd), "Song message"),
//         GTK_SIDE_TOP, GTK_ANCHOR_CENTER, GTK_FILL_X, 2, 0, 0, 200, 50);
    gtk_box_pack_end (GTK_BOX (hbox2),
                      make_framed (GTK_WIDGET (msgwnd), _("Song message")), TRUE,
                      TRUE, 2);
  }

#if 0
  // Add subsong slider section
  if (p == plr.p && p->getsubsongs () > 1)
  {
    GtkAdjustment *adj =
      GTK_ADJUSTMENT (gtk_adjustment_new (plr.subsong + 1, 1,
                                          p->getsubsongs () + 1,
                                          1, 5, 1));
    GtkHScale *slider = GTK_HSCALE (gtk_hscale_new (adj));

    g_signal_connect (G_OBJECT (adj), "value_changed",
                      G_CALLBACK (subsong_slider), NULL);
    gtk_range_set_update_policy (GTK_RANGE (slider),
                                 GTK_UPDATE_DISCONTINUOUS);
    gtk_scale_set_digits (GTK_SCALE (slider), 0);
// Former packer layout, for future reproduction
//   gtk_packer_add_defaults(packer, make_framed(GTK_WIDGET(slider), "Subsong selection"),
//              GTK_SIDE_TOP, GTK_ANCHOR_CENTER, GTK_FILL_X);
    gtk_box_pack_end (GTK_BOX (hbox2),
                      make_framed (GTK_WIDGET (slider), _("Subsong selection")),
                      TRUE, TRUE, 2);
  }
#endif

  // Show dialog box
  gtk_widget_show_all (GTK_WIDGET (infobox));
  if (p == plr.p)
  {                             // Remember widget, so we could destroy it later
    plr.infodlg = infobox;
  }
  else                          // Delete temporary player
    delete p;
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

static void
update_infobox (void)
{
  std::ostringstream infotext;

  // Recreate info string
  infotext << _("Order: ") << plr.p->getorder () << " / " << plr.p->
    getorders () << std::endl << _("Pattern: ") << plr.p->
    getpattern () << " / " << plr.p->
    getpatterns () << std::endl << _("Row: ") << plr.p->
    getrow () << std::endl << _("Speed: ") << plr.p->
    getspeed () << std::endl << _("Timer: ") << plr.p->
    getrefresh () << _("Hz") << std::ends;

  GDK_THREADS_ENTER ();
  gtk_label_set_text (plr.infobox, infotext.str ().c_str ());
  GDK_THREADS_LEAVE ();
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
  if (strcmp (filename, plr.filename))
  {
    strcpy (plr.filename, filename);
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

    // update infobox, if necessary
    if (plr.infobox)
      update_infobox ();
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
  if (plr.infobox && strcmp (filename, plr.filename))
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
  dbg_printf ("adplug_init(): open, ");
  mcs_handle_t *db = aud_cfg_db_open ();

  // Read configuration
  dbg_printf ("read, ");
  aud_cfg_db_get_bool (db, CFG_VERSION, "16bit", (gboolean *) & conf.bit16);
  aud_cfg_db_get_bool (db, CFG_VERSION, "Stereo", (gboolean *) & conf.stereo);
  aud_cfg_db_get_int (db, CFG_VERSION, "Frequency", (gint *) & conf.freq);
  aud_cfg_db_get_bool (db, CFG_VERSION, "Endless",
                       (gboolean *) & conf.endless);

  // Read file type exclusion list
  dbg_printf ("exclusion, ");
  {
    gchar *cfgstr = NULL, *exclude = NULL;
    gboolean cfgread;

    cfgread = aud_cfg_db_get_string (db, CFG_VERSION, "Exclude", &cfgstr);
    if (cfgread) {
        exclude = (char *) malloc (strlen (cfgstr) + 2);
        strcpy (exclude, cfgstr);
        exclude[strlen (exclude) + 1] = '\0';
        g_strdelimit (exclude, ":", '\0');
        for (gchar * p = exclude; *p; p += strlen (p) + 1)
            conf.players.remove (conf.players.lookup_filetype (p));
        free (exclude); free (cfgstr);
    }
  }
  aud_cfg_db_close (db);

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
  dbg_printf ("adplug_quit(): open, ");
  mcs_handle_t *db = aud_cfg_db_open ();

  // Close database
  dbg_printf ("db, ");
  if (plr.db)
    delete plr.db;

  // Write configuration
  dbg_printf ("write, ");
  aud_cfg_db_set_bool (db, CFG_VERSION, "16bit", conf.bit16);
  aud_cfg_db_set_bool (db, CFG_VERSION, "Stereo", conf.stereo);
  aud_cfg_db_set_int (db, CFG_VERSION, "Frequency", conf.freq);
  aud_cfg_db_set_bool (db, CFG_VERSION, "Endless", conf.endless);

  dbg_printf ("exclude, ");
  std::string exclude;
  for (CPlayers::const_iterator i = CAdPlug::players.begin ();
       i != CAdPlug::players.end (); i++)
    if (find (conf.players.begin (), conf.players.end (), *i) ==
        conf.players.end ())
    {
      if (!exclude.empty ())
        exclude += ":";
      exclude += (*i)->filetype;
    }
  gchar *cfgval = g_strdup (exclude.c_str ());
  aud_cfg_db_set_string (db, CFG_VERSION, "Exclude", cfgval);
  free (cfgval);

  dbg_printf ("close");
  aud_cfg_db_close (db);
  dbg_printf (".\n");

  g_mutex_free (control_mutex);
  g_cond_free (control_cond);
}
