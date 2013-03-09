/*
 * Audacious: Cross platform multimedia player
 * Copyright (c) 2005-2009 Audacious Team
 *
 * Driver for Game_Music_Emu library. See details at:
 * http://www.slack.net/~ant/libs/
 *
 * Preferences GUI by Giacomo Lozito
 */

#include "configure.h"
#include <glib.h>
#include <gtk/gtk.h>

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>

#define CON_CFGID "console"

AudaciousConsoleConfig audcfg;

static const gchar * const console_defaults[] = {
 "loop_length", "180",
 "resample", "FALSE",
 "resample_rate", "32000",
 "treble", "0",
 "bass", "0",
 "ignore_spc_length", "FALSE",
 "echo", "0",
 "inc_spc_reverb", "FALSE",
 NULL};

// TODO: add UI for echo
void console_cfg_load (void)
{
    aud_config_set_defaults (CON_CFGID, console_defaults);

    audcfg.loop_length = aud_get_int (CON_CFGID, "loop_length");
    audcfg.resample = aud_get_bool (CON_CFGID, "resample");
    audcfg.resample_rate = aud_get_int (CON_CFGID, "resample_rate");
    audcfg.treble = aud_get_int (CON_CFGID, "treble");
    audcfg.bass = aud_get_int (CON_CFGID, "bass");
    audcfg.ignore_spc_length = aud_get_bool (CON_CFGID, "ignore_spc_length");
    audcfg.echo = aud_get_int (CON_CFGID, "echo");
    audcfg.inc_spc_reverb = aud_get_bool (CON_CFGID, "inc_spc_reverb");
}

void console_cfg_save (void)
{
    aud_set_int (CON_CFGID, "loop_length", audcfg.loop_length);
    aud_set_bool (CON_CFGID, "resample", audcfg.resample);
    aud_set_int (CON_CFGID, "resample_rate", audcfg.resample_rate);
    aud_set_int (CON_CFGID, "treble", audcfg.treble);
    aud_set_int (CON_CFGID, "bass", audcfg.bass);
    aud_set_bool (CON_CFGID, "ignore_spc_length", audcfg.ignore_spc_length);
    aud_set_int (CON_CFGID, "echo", audcfg.echo);
    aud_set_bool (CON_CFGID, "inc_spc_reverb", audcfg.inc_spc_reverb);
}


// CONFIGURATION PANEL (GTK+2 GUI)

static void i_cfg_ev_resample_enable_commit( gpointer cbt )
{
    audcfg.resample = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(cbt) );
}

static void i_cfg_ev_resample_value_commit( gpointer spbt )
{
    audcfg.resample_rate = (gint)gtk_spin_button_get_value( GTK_SPIN_BUTTON(spbt) );
}

static void i_cfg_ev_bass_value_commit( gpointer spbt )
{
    audcfg.bass = (gint)gtk_spin_button_get_value( GTK_SPIN_BUTTON(spbt) );
}

static void i_cfg_ev_treble_value_commit( gpointer spbt )
{
    audcfg.treble = (gint)gtk_spin_button_get_value( GTK_SPIN_BUTTON(spbt) );
}

static void i_cfg_ev_deflen_value_commit( gpointer spbt )
{
    audcfg.loop_length = (gint)gtk_spin_button_get_value( GTK_SPIN_BUTTON(spbt) );
}

static void i_cfg_ev_ignorespclen_enable_commit( gpointer cbt )
{
    audcfg.ignore_spc_length = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(cbt) );
}

static void i_cfg_ev_incspcreverb_enable_commit( gpointer cbt )
{
    audcfg.inc_spc_reverb = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(cbt) );
}

static void i_cfg_ev_bok( gpointer configwin )
{
    console_cfg_save();
    gtk_widget_destroy( GTK_WIDGET(configwin) );
}

static void i_cfg_ev_toggle_resample( GtkToggleButton *tbt , gpointer val_hbox )
{
    gtk_widget_set_sensitive( GTK_WIDGET(val_hbox) ,
                              gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(tbt) ) );
}


void console_cfg_ui(void)
{
    static GtkWidget *configwin = NULL;
    GtkWidget *configwin_vbox;
    GtkWidget *configwin_gen_vbox, *configwin_spc_vbox;
    GtkWidget *configwin_gen_resample_frame, *configwin_gen_resample_vbox;
    GtkWidget *configwin_gen_resample_cbt, *configwin_gen_resample_val_hbox;
    GtkWidget *configwin_gen_resample_val_spbt;
    GtkWidget *configwin_gen_playback_frame, *configwin_gen_playback_vbox;
    GtkWidget *configwin_gen_playback_tb_hbox;
    GtkWidget *configwin_gen_playback_tb_bass_hbox, *configwin_gen_playback_tb_bass_spbt;
    GtkWidget *configwin_gen_playback_tb_treble_hbox, *configwin_gen_playback_tb_treble_spbt;
    GtkWidget *configwin_gen_playback_deflen_hbox, *configwin_gen_playback_deflen_spbt;
    GtkWidget *configwin_spc_ignorespclen_cbt, *configwin_spc_increverb_cbt;
    GtkWidget /* *hseparator, */ *hbuttonbox, *button_ok, *button_cancel;
    GtkWidget *configwin_notebook;

    if (configwin != NULL)
        return;

    configwin = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    gtk_window_set_type_hint( GTK_WINDOW(configwin), GDK_WINDOW_TYPE_HINT_DIALOG );
    gtk_window_set_title( GTK_WINDOW(configwin), _("Game Console Music Decoder") );
    gtk_container_set_border_width( GTK_CONTAINER(configwin), 10 );
    g_signal_connect( G_OBJECT(configwin) , "destroy" ,
                      G_CALLBACK(gtk_widget_destroyed) , &configwin );
    button_ok = gtk_button_new_from_stock( GTK_STOCK_OK );

    configwin_vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL , 6 );
    gtk_container_add( GTK_CONTAINER(configwin) , configwin_vbox );

    configwin_notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos( GTK_NOTEBOOK(configwin_notebook) , GTK_POS_TOP );
    gtk_box_pack_start( GTK_BOX(configwin_vbox) , configwin_notebook , TRUE , TRUE , 2 );

    // GENERAL PAGE
    configwin_gen_vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL , 3 );
    gtk_container_set_border_width( GTK_CONTAINER(configwin_gen_vbox), 5 );
    gtk_notebook_append_page( GTK_NOTEBOOK(configwin_notebook) ,
                              configwin_gen_vbox , gtk_label_new( _("General") ) );
    // GENERAL PAGE - PLAYBACK FRAME
    configwin_gen_playback_frame = gtk_frame_new( _("Playback") );
    gtk_box_pack_start( GTK_BOX(configwin_gen_vbox) ,
                        configwin_gen_playback_frame , TRUE , TRUE , 0 );
    configwin_gen_playback_vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL , 4 );
    gtk_container_set_border_width( GTK_CONTAINER(configwin_gen_playback_vbox), 4 );
    gtk_container_add( GTK_CONTAINER(configwin_gen_playback_frame) , configwin_gen_playback_vbox );
    configwin_gen_playback_tb_hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL , 0 );
    gtk_box_pack_start( GTK_BOX(configwin_gen_playback_vbox) ,
                        configwin_gen_playback_tb_hbox , FALSE , FALSE , 0 );
    configwin_gen_playback_tb_bass_hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL , 4 );
    configwin_gen_playback_tb_bass_spbt = gtk_spin_button_new_with_range( -100 , 100 , 1 );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(configwin_gen_playback_tb_bass_spbt) , audcfg.bass );
    g_signal_connect_swapped( G_OBJECT(button_ok) , "clicked" ,
                              G_CALLBACK(i_cfg_ev_bass_value_commit) , configwin_gen_playback_tb_bass_spbt );
    gtk_box_pack_start( GTK_BOX(configwin_gen_playback_tb_bass_hbox) ,
                        gtk_label_new(_("Bass:")) , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(configwin_gen_playback_tb_bass_hbox) ,
                        configwin_gen_playback_tb_bass_spbt , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(configwin_gen_playback_tb_bass_hbox) ,
                        gtk_label_new(_("secs")) , FALSE , FALSE , 0 );
    configwin_gen_playback_tb_treble_hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL , 4 );
    configwin_gen_playback_tb_treble_spbt = gtk_spin_button_new_with_range( -100 , 100 , 1 );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(configwin_gen_playback_tb_treble_spbt) , audcfg.treble );
    g_signal_connect_swapped( G_OBJECT(button_ok) , "clicked" ,
                              G_CALLBACK(i_cfg_ev_treble_value_commit) , configwin_gen_playback_tb_treble_spbt );
    gtk_box_pack_start( GTK_BOX(configwin_gen_playback_tb_treble_hbox) ,
                        gtk_label_new(_("Treble:")) , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(configwin_gen_playback_tb_treble_hbox) ,
                        configwin_gen_playback_tb_treble_spbt , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(configwin_gen_playback_tb_treble_hbox) ,
                        gtk_label_new(_("secs")) , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(configwin_gen_playback_tb_hbox) ,
                        configwin_gen_playback_tb_bass_hbox , TRUE , TRUE , 0 );
    gtk_box_pack_start( GTK_BOX(configwin_gen_playback_tb_hbox) ,
                        gtk_separator_new( GTK_ORIENTATION_VERTICAL ) , FALSE , FALSE , 4 );
    gtk_box_pack_start( GTK_BOX(configwin_gen_playback_tb_hbox) ,
                        configwin_gen_playback_tb_treble_hbox , TRUE , TRUE , 0 );
    gtk_box_pack_start( GTK_BOX(configwin_gen_playback_vbox) ,
                        gtk_separator_new( GTK_ORIENTATION_HORIZONTAL ) , FALSE , FALSE , 0 );
    configwin_gen_playback_deflen_hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL , 4 );
    gtk_box_pack_start( GTK_BOX(configwin_gen_playback_vbox) ,
                        configwin_gen_playback_deflen_hbox , FALSE , FALSE , 0 );
    configwin_gen_playback_deflen_spbt = gtk_spin_button_new_with_range( 1 , 7200 , 10 );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(configwin_gen_playback_deflen_spbt) , audcfg.loop_length );
    g_signal_connect_swapped( G_OBJECT(button_ok) , "clicked" ,
                              G_CALLBACK(i_cfg_ev_deflen_value_commit) , configwin_gen_playback_deflen_spbt );
    gtk_box_pack_start( GTK_BOX(configwin_gen_playback_deflen_hbox) ,
                        gtk_label_new(_("Default song length:")) , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(configwin_gen_playback_deflen_hbox) ,
                        configwin_gen_playback_deflen_spbt , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(configwin_gen_playback_deflen_hbox) ,
                        gtk_label_new(_("secs")) , FALSE , FALSE , 0 );
    // GENERAL PAGE - RESAMPLING FRAME
    configwin_gen_resample_frame = gtk_frame_new( _("Resampling") );
    gtk_box_pack_start( GTK_BOX(configwin_gen_vbox) ,
                        configwin_gen_resample_frame , TRUE , TRUE , 0 );
    configwin_gen_resample_vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL , 4 );
    gtk_container_set_border_width( GTK_CONTAINER(configwin_gen_resample_vbox), 4 );
    gtk_container_add( GTK_CONTAINER(configwin_gen_resample_frame) , configwin_gen_resample_vbox );
    configwin_gen_resample_cbt = gtk_check_button_new_with_label( _("Enable audio resampling") );
    g_signal_connect_swapped( G_OBJECT(button_ok) , "clicked" ,
                              G_CALLBACK(i_cfg_ev_resample_enable_commit) , configwin_gen_resample_cbt );
    gtk_box_pack_start( GTK_BOX(configwin_gen_resample_vbox) ,
                        configwin_gen_resample_cbt , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(configwin_gen_resample_vbox) ,
                        gtk_separator_new( GTK_ORIENTATION_HORIZONTAL ) , FALSE , FALSE , 0 );
    configwin_gen_resample_val_hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL , 4 );
    configwin_gen_resample_val_spbt = gtk_spin_button_new_with_range( 11025 , 96000 , 100 );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON(configwin_gen_resample_val_spbt) , audcfg.resample_rate );
    g_signal_connect_swapped( G_OBJECT(button_ok) , "clicked" ,
                              G_CALLBACK(i_cfg_ev_resample_value_commit) , configwin_gen_resample_val_spbt );
    gtk_box_pack_start( GTK_BOX(configwin_gen_resample_vbox) ,
                        configwin_gen_resample_val_hbox , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(configwin_gen_resample_val_hbox) ,
                        gtk_label_new(_("Resampling rate:")) , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(configwin_gen_resample_val_hbox) ,
                        configwin_gen_resample_val_spbt , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(configwin_gen_resample_val_hbox) ,
                        gtk_label_new(_("Hz")) , FALSE , FALSE , 0 );
    gtk_widget_set_sensitive( GTK_WIDGET(configwin_gen_resample_val_hbox) , audcfg.resample );
    g_signal_connect( G_OBJECT(configwin_gen_resample_cbt) , "toggled" ,
                      G_CALLBACK(i_cfg_ev_toggle_resample) , configwin_gen_resample_val_hbox );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(configwin_gen_resample_cbt) , audcfg.resample );


    // SPC PAGE
    configwin_spc_vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL , 3 );
    gtk_container_set_border_width( GTK_CONTAINER(configwin_spc_vbox), 5 );
    gtk_notebook_append_page( GTK_NOTEBOOK(configwin_notebook) ,
                              configwin_spc_vbox , gtk_label_new( _("SPC") ) );
    configwin_spc_ignorespclen_cbt = gtk_check_button_new_with_label( _("Ignore length from SPC tags") );
    configwin_spc_increverb_cbt = gtk_check_button_new_with_label( _("Increase reverb") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(configwin_spc_ignorespclen_cbt) , audcfg.ignore_spc_length );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(configwin_spc_increverb_cbt) , audcfg.inc_spc_reverb );
    g_signal_connect_swapped( G_OBJECT(button_ok) , "clicked" ,
                              G_CALLBACK(i_cfg_ev_ignorespclen_enable_commit) , configwin_spc_ignorespclen_cbt );
    g_signal_connect_swapped( G_OBJECT(button_ok) , "clicked" ,
                              G_CALLBACK(i_cfg_ev_incspcreverb_enable_commit) , configwin_spc_increverb_cbt );
    gtk_box_pack_start( GTK_BOX(configwin_spc_vbox) ,
                        configwin_spc_ignorespclen_cbt , FALSE , FALSE , 0 );
    gtk_box_pack_start( GTK_BOX(configwin_spc_vbox) ,
                        configwin_spc_increverb_cbt , FALSE , FALSE , 0 );

    // horizontal separator and buttons
    hbuttonbox = gtk_button_box_new( GTK_ORIENTATION_HORIZONTAL );
    gtk_button_box_set_layout( GTK_BUTTON_BOX(hbuttonbox) , GTK_BUTTONBOX_END );
    button_cancel = gtk_button_new_from_stock( GTK_STOCK_CANCEL );
    g_signal_connect_swapped( G_OBJECT(button_cancel) , "clicked" ,
                              G_CALLBACK(gtk_widget_destroy) , configwin );
    gtk_container_add( GTK_CONTAINER(hbuttonbox) , button_cancel );
    g_signal_connect_swapped( G_OBJECT(button_ok) , "clicked" ,
                              G_CALLBACK(i_cfg_ev_bok) , configwin );
    gtk_container_add( GTK_CONTAINER(hbuttonbox) , button_ok );
    gtk_box_pack_start( GTK_BOX(configwin_vbox) , hbuttonbox , FALSE , FALSE , 0 );

    gtk_widget_set_tooltip_text( configwin_gen_playback_deflen_spbt ,
                                 _("The default song length, expressed in seconds, is used for songs "
                                 "that do not provide length information (i.e. looping tracks)."));

    gtk_widget_show_all( configwin );
}
