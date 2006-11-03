

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <audacious/plugin.h>
#include <audacious/beepctrl.h>
#include <audacious/configdb.h>
#include <audacious/util.h>

#include "xmmsmplayer.h"

static GtkWidget *vo_none;
static GtkWidget *vo_xv;
static GtkWidget *vo_x11;
static GtkWidget *vo_gl;
static GtkWidget *vo_sdl;
static GtkWidget *opt_zoom;
static GtkWidget *opt_framedrop;
static GtkWidget *opt_idx;
static GtkWidget *opt_onewin;
static GtkWidget *opt_xmmsaudio;
static GtkWidget *ao_none;
static GtkWidget *ao_oss;
static GtkWidget *ao_arts;
static GtkWidget *ao_esd;
static GtkWidget *ao_alsa;
static GtkWidget *ao_sdl;
static GtkWidget *entry_extra_opts;

static GtkWidget *mplayer_configure_win = NULL;

void mplayer_destroyed_conf_win(GtkWidget *widget, gpointer data) {
  mplayer_configure_win = NULL;
}


void on_btn_cancel_clicked(GtkButton *button, gpointer user_data){
  gtk_widget_destroy(mplayer_configure_win);
  mplayer_configure_win=NULL;
}


void on_btn_ok_clicked (GtkButton *button, gpointer user_data){
  ConfigDb *cfg;
  struct mplayer_cfg new_cfg;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vo_none)))
    new_cfg.vo=MPLAYER_VO_NONE;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vo_xv)))
    new_cfg.vo=MPLAYER_VO_XV;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vo_x11)))
    new_cfg.vo=MPLAYER_VO_X11;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vo_gl)))
    new_cfg.vo=MPLAYER_VO_GL;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vo_sdl)))
    new_cfg.vo=MPLAYER_VO_SDL;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ao_none)))
    new_cfg.ao=MPLAYER_AO_NONE;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ao_oss)))
    new_cfg.ao=MPLAYER_AO_OSS;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ao_arts)))
    new_cfg.ao=MPLAYER_AO_ARTS;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ao_esd)))
    new_cfg.ao=MPLAYER_AO_ESD;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ao_alsa)))
    new_cfg.ao=MPLAYER_AO_ALSA;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ao_sdl)))
    new_cfg.ao=MPLAYER_AO_SDL;
  new_cfg.zoom=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(opt_zoom));
  new_cfg.framedrop=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(opt_framedrop));
  new_cfg.idx=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(opt_idx));
  new_cfg.onewin=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(opt_onewin));
  new_cfg.xmmsaudio=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(opt_xmmsaudio));
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(opt_xmmsaudio)))
    new_cfg.ao=MPLAYER_AO_XMMS;
  new_cfg.extra=gtk_entry_get_text(GTK_ENTRY(entry_extra_opts));

  cfg = bmp_cfg_db_open();
  bmp_cfg_db_set_int(cfg,"xmms-mplayer","vo",new_cfg.vo);
  bmp_cfg_db_set_int(cfg,"xmms-mplayer","ao",new_cfg.ao);
  bmp_cfg_db_set_bool(cfg,"xmms-mplayer","zoom",new_cfg.zoom);
  bmp_cfg_db_set_bool(cfg,"xmms-mplayer","framedrop",new_cfg.framedrop);
  bmp_cfg_db_set_bool(cfg,"xmms-mplayer","idx",new_cfg.idx);
  bmp_cfg_db_set_bool(cfg,"xmms-mplayer","onewin",new_cfg.onewin);
  bmp_cfg_db_set_bool(cfg,"xmms-mplayer","xmmsaudio",new_cfg.xmmsaudio);
  bmp_cfg_db_set_string(cfg,"xmms-mplayer","extra",new_cfg.extra);
  bmp_cfg_db_close(cfg);

  gtk_widget_destroy(mplayer_configure_win);
  mplayer_configure_win=NULL;
}


GtkWidget*
mplayer_create_configure_win (void)
{
  GtkWidget *window1;
  GtkWidget *scrolledwindow1;
  GtkWidget *layout1;
  GtkWidget *notebook1;
  GtkWidget *scrolledwindow2;
  GtkWidget *layout2;
  GSList *layout2_group = NULL;
  GtkWidget *label_vo;
  GtkWidget *scrolledwindow3;
  GtkWidget *layout3;
  GSList *layout3_group = NULL;
  GtkWidget *label_ao;
  GtkWidget *scrolledwindow4;
  GtkWidget *layout4;
  GtkWidget *label_other;
  GtkWidget *scrolledwindow5;
  GtkWidget *layout5;
  GtkWidget *label_extra_des;
  GtkWidget *label_extra;
  GtkWidget *btn_ok;
  GtkWidget *btn_cancel;

  window1 = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_object_set_data (GTK_OBJECT (window1), "window1", window1);
  gtk_window_set_title (GTK_WINDOW (window1), "MPlayer");
  gtk_widget_set_usize (window1, 550, 430);


  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow1);
  gtk_object_set_data_full (GTK_OBJECT (window1), "scrolledwindow1", scrolledwindow1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow1);
  gtk_container_add (GTK_CONTAINER (window1), scrolledwindow1);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  layout1 = gtk_layout_new (NULL, NULL);
  gtk_widget_ref (layout1);
  gtk_object_set_data_full (GTK_OBJECT (window1), "layout1", layout1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (layout1);
  gtk_container_add (GTK_CONTAINER (scrolledwindow1), layout1);
  gtk_layout_set_size (GTK_LAYOUT (layout1), 336, 235);
  GTK_ADJUSTMENT (GTK_LAYOUT (layout1)->hadjustment)->step_increment = 10;
  GTK_ADJUSTMENT (GTK_LAYOUT (layout1)->vadjustment)->step_increment = 10;

  notebook1 = gtk_notebook_new ();
  gtk_widget_ref (notebook1);
  gtk_object_set_data_full (GTK_OBJECT (window1), "notebook1", notebook1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (notebook1);
  gtk_layout_put (GTK_LAYOUT (layout1), notebook1, 24, 24);
  gtk_widget_set_usize (notebook1, 496, 336);

  scrolledwindow2 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow2);
  gtk_object_set_data_full (GTK_OBJECT (window1), "scrolledwindow2", scrolledwindow2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow2);
  gtk_container_add (GTK_CONTAINER (notebook1), scrolledwindow2);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow2), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  layout2 = gtk_layout_new (NULL, NULL);
  gtk_widget_ref (layout2);
  gtk_object_set_data_full (GTK_OBJECT (window1), "layout2", layout2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (layout2);
  gtk_container_add (GTK_CONTAINER (scrolledwindow2), layout2);
  gtk_layout_set_size (GTK_LAYOUT (layout2), 384, 296);
  GTK_ADJUSTMENT (GTK_LAYOUT (layout2)->hadjustment)->step_increment = 10;
  GTK_ADJUSTMENT (GTK_LAYOUT (layout2)->vadjustment)->step_increment = 10;

  vo_none = gtk_radio_button_new_with_label (layout2_group, "Automatic (MPlayer Chooses)");
  layout2_group = gtk_radio_button_group (GTK_RADIO_BUTTON (vo_none));
  gtk_widget_ref (vo_none);
  gtk_object_set_data_full (GTK_OBJECT (window1), "vo_none", vo_none,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vo_none);
  gtk_layout_put (GTK_LAYOUT (layout2), vo_none, 24, 32);
  gtk_widget_set_usize (vo_none, 336, 24);

  vo_xv = gtk_radio_button_new_with_label (layout2_group, "Xvideo");
  layout2_group = gtk_radio_button_group (GTK_RADIO_BUTTON (vo_xv));
  gtk_widget_ref (vo_xv);
  gtk_object_set_data_full (GTK_OBJECT (window1), "vo_xv", vo_xv,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vo_xv);
  gtk_layout_put (GTK_LAYOUT (layout2), vo_xv, 24, 64);
  gtk_widget_set_usize (vo_xv, 96, 24);

  vo_x11 = gtk_radio_button_new_with_label (layout2_group, "X11");
  layout2_group = gtk_radio_button_group (GTK_RADIO_BUTTON (vo_x11));
  gtk_widget_ref (vo_x11);
  gtk_object_set_data_full (GTK_OBJECT (window1), "vo_x11", vo_x11,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vo_x11);
  gtk_layout_put (GTK_LAYOUT (layout2), vo_x11, 24, 104);
  gtk_widget_set_usize (vo_x11, 96, 24);

  vo_gl = gtk_radio_button_new_with_label (layout2_group, "GL");
  layout2_group = gtk_radio_button_group (GTK_RADIO_BUTTON (vo_gl));
  gtk_widget_ref (vo_gl);
  gtk_object_set_data_full (GTK_OBJECT (window1), "vo_gl", vo_gl,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vo_gl);
  gtk_layout_put (GTK_LAYOUT (layout2), vo_gl, 24, 144);
  gtk_widget_set_usize (vo_gl, 96, 24);

  vo_sdl = gtk_radio_button_new_with_label (layout2_group, "SDL");
  layout2_group = gtk_radio_button_group (GTK_RADIO_BUTTON (vo_sdl));
  gtk_widget_ref (vo_sdl);
  gtk_object_set_data_full (GTK_OBJECT (window1), "vo_sdl", vo_sdl,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vo_sdl);
  gtk_layout_put (GTK_LAYOUT (layout2), vo_sdl, 24, 184);
  gtk_widget_set_usize (vo_sdl, 96, 24);

  label_vo = gtk_label_new ("Video Out");
  gtk_widget_ref (label_vo);
  gtk_object_set_data_full (GTK_OBJECT (window1), "label_vo", label_vo,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label_vo);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 0), label_vo);

  scrolledwindow3 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow3);
  gtk_object_set_data_full (GTK_OBJECT (window1), "scrolledwindow3", scrolledwindow3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow3);
  gtk_container_add (GTK_CONTAINER (notebook1), scrolledwindow3);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow3), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  layout3 = gtk_layout_new (NULL, NULL);
  gtk_widget_ref (layout3);
  gtk_object_set_data_full (GTK_OBJECT (window1), "layout3", layout3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (layout3);
  gtk_container_add (GTK_CONTAINER (scrolledwindow3), layout3);
  gtk_layout_set_size (GTK_LAYOUT (layout3), 382, 295);
  GTK_ADJUSTMENT (GTK_LAYOUT (layout3)->hadjustment)->step_increment = 10;
  GTK_ADJUSTMENT (GTK_LAYOUT (layout3)->vadjustment)->step_increment = 10;

  ao_none = gtk_radio_button_new_with_label (layout3_group, "Automatic (MPlayer Chooses)");
  layout3_group = gtk_radio_button_group (GTK_RADIO_BUTTON (ao_none));
  gtk_widget_ref (ao_none);
  gtk_object_set_data_full (GTK_OBJECT (window1), "ao_none", ao_none,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (ao_none);
  gtk_layout_put (GTK_LAYOUT (layout3), ao_none, 24, 32);
  gtk_widget_set_usize (ao_none, 272, 24);

  ao_oss = gtk_radio_button_new_with_label (layout3_group, "OSS");
  layout3_group = gtk_radio_button_group (GTK_RADIO_BUTTON (ao_oss));
  gtk_widget_ref (ao_oss);
  gtk_object_set_data_full (GTK_OBJECT (window1), "ao_oss", ao_oss,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (ao_oss);
  gtk_layout_put (GTK_LAYOUT (layout3), ao_oss, 24, 64);
  gtk_widget_set_usize (ao_oss, 96, 24);

  ao_arts = gtk_radio_button_new_with_label (layout3_group, "ARTS");
  layout3_group = gtk_radio_button_group (GTK_RADIO_BUTTON (ao_arts));
  gtk_widget_ref (ao_arts);
  gtk_object_set_data_full (GTK_OBJECT (window1), "ao_arts", ao_arts,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (ao_arts);
  gtk_layout_put (GTK_LAYOUT (layout3), ao_arts, 24, 96);
  gtk_widget_set_usize (ao_arts, 96, 24);

  ao_esd = gtk_radio_button_new_with_label (layout3_group, "ESD");
  layout3_group = gtk_radio_button_group (GTK_RADIO_BUTTON (ao_esd));
  gtk_widget_ref (ao_esd);
  gtk_object_set_data_full (GTK_OBJECT (window1), "ao_esd", ao_esd,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (ao_esd);
  gtk_layout_put (GTK_LAYOUT (layout3), ao_esd, 24, 128);
  gtk_widget_set_usize (ao_esd, 96, 24);

  ao_alsa = gtk_radio_button_new_with_label (layout3_group, "ALSA");
  layout3_group = gtk_radio_button_group (GTK_RADIO_BUTTON (ao_alsa));
  gtk_widget_ref (ao_alsa);
  gtk_object_set_data_full (GTK_OBJECT (window1), "ao_alsa", ao_alsa,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (ao_alsa);
  gtk_layout_put (GTK_LAYOUT (layout3), ao_alsa, 24, 160);
  gtk_widget_set_usize (ao_alsa, 103, 24);

  ao_sdl = gtk_radio_button_new_with_label (layout3_group, "SDL");
  layout3_group = gtk_radio_button_group (GTK_RADIO_BUTTON (ao_sdl));
  gtk_widget_ref (ao_sdl);
  gtk_object_set_data_full (GTK_OBJECT (window1), "ao_sdl", ao_sdl,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (ao_sdl);
  gtk_layout_put (GTK_LAYOUT (layout3), ao_sdl, 24, 192);
  gtk_widget_set_usize (ao_sdl, 103, 24);

  opt_xmmsaudio = gtk_radio_button_new_with_label (layout3_group, "XMMS Output Plugin (Experimental)");
  layout3_group = gtk_radio_button_group (GTK_RADIO_BUTTON (opt_xmmsaudio));
  gtk_widget_ref (opt_xmmsaudio);
  gtk_object_set_data_full (GTK_OBJECT (window1), "opt_xmmsaudio", opt_xmmsaudio,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (opt_xmmsaudio);
  gtk_layout_put (GTK_LAYOUT (layout3), opt_xmmsaudio, 24, 224);
  gtk_widget_set_usize (opt_xmmsaudio, 272, 24);

  label_ao = gtk_label_new ("Audio Out");
  gtk_widget_ref (label_ao);
  gtk_object_set_data_full (GTK_OBJECT (window1), "label_ao", label_ao,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label_ao);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 1), label_ao);

  scrolledwindow4 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow4);
  gtk_object_set_data_full (GTK_OBJECT (window1), "scrolledwindow4", scrolledwindow4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow4);
  gtk_container_add (GTK_CONTAINER (notebook1), scrolledwindow4);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow4), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  layout4 = gtk_layout_new (NULL, NULL);
  gtk_widget_ref (layout4);
  gtk_object_set_data_full (GTK_OBJECT (window1), "layout4", layout4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (layout4);
  gtk_container_add (GTK_CONTAINER (scrolledwindow4), layout4);
  gtk_layout_set_size (GTK_LAYOUT (layout4), 377, 272);
  GTK_ADJUSTMENT (GTK_LAYOUT (layout4)->hadjustment)->step_increment = 10;
  GTK_ADJUSTMENT (GTK_LAYOUT (layout4)->vadjustment)->step_increment = 10;

  opt_zoom = gtk_check_button_new_with_label ("Software Zoom");
  gtk_widget_ref (opt_zoom);
  gtk_object_set_data_full (GTK_OBJECT (window1), "opt_zoom", opt_zoom,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (opt_zoom);
  gtk_layout_put (GTK_LAYOUT (layout4), opt_zoom, 24, 32);
  gtk_widget_set_usize (opt_zoom, 168, 24);

  opt_framedrop = gtk_check_button_new_with_label ("Frame Dropping");
  gtk_widget_ref (opt_framedrop);
  gtk_object_set_data_full (GTK_OBJECT (window1), "opt_framedrop", opt_framedrop,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (opt_framedrop);
  gtk_layout_put (GTK_LAYOUT (layout4), opt_framedrop, 24, 72);
  gtk_widget_set_usize (opt_framedrop, 136, 24);

  opt_idx = gtk_check_button_new_with_label ("Build Index");
  gtk_widget_ref (opt_idx);
  gtk_object_set_data_full (GTK_OBJECT (window1), "opt_idx", opt_idx,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (opt_idx);
  gtk_layout_put (GTK_LAYOUT (layout4), opt_idx, 24, 112);
  gtk_widget_set_usize (opt_idx, 102, 24);

  opt_onewin = gtk_check_button_new_with_label ("One Window (Experimental)");
  gtk_widget_ref (opt_onewin);
  gtk_object_set_data_full (GTK_OBJECT (window1), "opt_onewin", opt_onewin,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (opt_onewin);
  gtk_layout_put (GTK_LAYOUT (layout4), opt_onewin, 24, 152);
  gtk_widget_set_usize (opt_onewin, 240, 24);

  label_other = gtk_label_new ("Other");
  gtk_widget_ref (label_other);
  gtk_object_set_data_full (GTK_OBJECT (window1), "label_other", label_other,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label_other);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 2), label_other);

  scrolledwindow5 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow5);
  gtk_object_set_data_full (GTK_OBJECT (window1), "scrolledwindow5", scrolledwindow5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow5);
  gtk_container_add (GTK_CONTAINER (notebook1), scrolledwindow5);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow5), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  layout5 = gtk_layout_new (NULL, NULL);
  gtk_widget_ref (layout5);
  gtk_object_set_data_full (GTK_OBJECT (window1), "layout5", layout5,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (layout5);
  gtk_container_add (GTK_CONTAINER (scrolledwindow5), layout5);
  gtk_layout_set_size (GTK_LAYOUT (layout5), 346, 267);
  GTK_ADJUSTMENT (GTK_LAYOUT (layout5)->hadjustment)->step_increment = 10;
  GTK_ADJUSTMENT (GTK_LAYOUT (layout5)->vadjustment)->step_increment = 10;

  entry_extra_opts = gtk_entry_new ();
  gtk_widget_ref (entry_extra_opts);
  gtk_object_set_data_full (GTK_OBJECT (window1), "entry_extra_opts", entry_extra_opts,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (entry_extra_opts);
  gtk_layout_put (GTK_LAYOUT (layout5), entry_extra_opts, 40, 192);
  gtk_widget_set_usize (entry_extra_opts, 392, 24);

  label_extra_des = gtk_label_new ("Extra options for mplayer can be added here.\nParsing is done based on spaces.\nQuotes and escapes are not recognised yet.");
  gtk_widget_ref (label_extra_des);
  gtk_object_set_data_full (GTK_OBJECT (window1), "label_extra_des", label_extra_des,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label_extra_des);
  gtk_layout_put (GTK_LAYOUT (layout5), label_extra_des, 40, 48);
  gtk_widget_set_usize (label_extra_des, 392, 128);
  gtk_label_set_justify (GTK_LABEL (label_extra_des), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label_extra_des), 0.18, 1);

  label_extra = gtk_label_new ("Extra");
  gtk_widget_ref (label_extra);
  gtk_object_set_data_full (GTK_OBJECT (window1), "label_extra", label_extra,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label_extra);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 3), label_extra);

  btn_ok = gtk_button_new_with_label ("OK");
  gtk_widget_ref (btn_ok);
  gtk_object_set_data_full (GTK_OBJECT (window1), "btn_ok", btn_ok,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (btn_ok);
  gtk_layout_put (GTK_LAYOUT (layout1), btn_ok, 432, 376);
  gtk_widget_set_usize (btn_ok, 88, 32);

  btn_cancel = gtk_button_new_with_label ("Cancel");
  gtk_widget_ref (btn_cancel);
  gtk_object_set_data_full (GTK_OBJECT (window1), "btn_cancel", btn_cancel,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (btn_cancel);
  gtk_layout_put (GTK_LAYOUT (layout1), btn_cancel, 24, 376);
  gtk_widget_set_usize (btn_cancel, 88, 30);

  gtk_signal_connect(GTK_OBJECT(window1),
		     "destroy",
		     GTK_SIGNAL_FUNC(mplayer_destroyed_conf_win),
		      NULL);

  gtk_signal_connect (GTK_OBJECT (btn_ok), "clicked",
                      GTK_SIGNAL_FUNC (on_btn_ok_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (btn_cancel), "clicked",
                      GTK_SIGNAL_FUNC (on_btn_cancel_clicked),
                      NULL);
  return window1;
}

void mplayer_configure(){
  struct mplayer_cfg *cfg;
  if (mplayer_configure_win)
    return;
  mplayer_configure_win=mplayer_create_configure_win();
  gtk_widget_show(mplayer_configure_win);
  cfg=mplayer_read_cfg();
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (opt_zoom),cfg->zoom);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (opt_framedrop),cfg->framedrop);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (opt_idx),cfg->idx);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (opt_onewin),cfg->onewin);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (opt_xmmsaudio),cfg->xmmsaudio);

  switch(cfg->vo){
  case MPLAYER_VO_NONE:
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vo_none), TRUE);
    break;
  case MPLAYER_VO_XV:
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vo_xv), TRUE);
    break;
  case MPLAYER_VO_X11:
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vo_x11), TRUE);
    break;
  case MPLAYER_VO_GL:
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vo_gl), TRUE);
    break;
  case MPLAYER_VO_SDL:
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vo_sdl), TRUE);
    break;
  }
  switch(cfg->ao){
  case MPLAYER_AO_NONE:
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ao_none), TRUE);
    break;
  case MPLAYER_AO_OSS:
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ao_oss), TRUE);
    break;
  case MPLAYER_AO_ARTS:
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ao_arts), TRUE);
    break;
  case MPLAYER_AO_ESD:
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ao_esd), TRUE);
    break;
  case MPLAYER_AO_SDL:
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ao_sdl), TRUE);
    break;
  case MPLAYER_AO_ALSA:
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ao_alsa), TRUE);
    break;
  }
  gtk_entry_set_text(GTK_ENTRY(entry_extra_opts),cfg->extra);
}
