//gui.c:

/*
 *      Copyright (C) Philipp 'ph3-der-loewe' Schafft    - 2008,
 *                    Daniel Duntemann <dauxx@dauxx.org> - 2009
 *
 *  This file is part of the XMMS RoarAudio output plugin a part of RoarAudio,
 *  a cross-platform sound system for both, home and professional use.
 *  See README for details.
 *
 *  This file is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3
 *  as published by the Free Software Foundation.
 *
 *  RoarAudio is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "all.h"

// ABOUT:

void roar_about(void) {
 static GtkWidget *window = NULL;

 audgui_simple_message(&window, GTK_MESSAGE_INFO, 
                _("About RoarAudio Plugin"),
                _("RoarAudio Audacious Plugin..."
                 ));
}

// CONFIG:

void on_button_ok_enter(GtkButton * button, gpointer user_data) {
}

void roar_configure(void) {
  GtkWidget *config_win;
  GtkWidget *vbox_main;
  GtkWidget *tabs_main;
  GtkWidget *frame_server;
  GtkWidget *alignment_server;
  GtkWidget *table1;
  GtkWidget *label_server_type;
  GtkWidget *label_server_host;
  GtkWidget *label_server_port;
  GtkObject *input_server_port_adj;
  GtkWidget *input_server_port;
  GtkWidget *input_server_host;
/*
  GtkWidget *input_server_type;
*/
  GtkWidget *label_host;
  GtkWidget *label_server;
  GtkWidget *frame_proxy;
  GtkWidget *alignment_proxy;
  GtkWidget *table2;
  GtkWidget *label_proxy_type;
  GtkWidget *label_proxy_addr;
  GtkWidget *input_proxy_host;
/*
  GtkWidget *input_proxy_type;
*/
  GtkWidget *label_frame_proxy;
  GtkWidget *label_proxy;
  GtkWidget *vbox_meta;
  GtkWidget *frame_meta_player;
  GtkWidget *alignment1;
  GtkWidget *hbox_meta_player;
  GtkWidget *label_player_name;
  GtkWidget *input_meta_player_name;
  GtkWidget *label_player;
  GtkWidget *frame_meta_title;
  GtkWidget *alignment2;
  GtkWidget *hbox_meta_title;
/*
  GtkWidget *input_meta_send_title;
  GtkWidget *input_meta_send_filename;
*/
  GtkWidget *label_meta_title;
  GtkWidget *label_meta;
  GtkWidget *hbox_buttons;
  GtkWidget *button_ok;
  GtkWidget *button_cancel;

  config_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(config_win), _("RoarAudio Plugin - Configuration"));
//  gtk_window_set_destroy_with_parent(GTK_WINDOW(config_win), TRUE);
//  gtk_window_set_icon_name(GTK_WINDOW(config_win), "gtk-preferences");

  gtk_widget_show (config_win);

  vbox_main = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox_main);
  gtk_container_add (GTK_CONTAINER (config_win), vbox_main);

  tabs_main = gtk_notebook_new ();
  gtk_widget_show (tabs_main);
  gtk_box_pack_start (GTK_BOX (vbox_main), tabs_main, TRUE, TRUE, 0);

  frame_server = gtk_frame_new (NULL);
  gtk_widget_show (frame_server);
  gtk_container_add (GTK_CONTAINER (tabs_main), frame_server);
  gtk_frame_set_shadow_type (GTK_FRAME (frame_server), GTK_SHADOW_NONE);

  alignment_server = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_widget_show (alignment_server);
  gtk_container_add (GTK_CONTAINER (frame_server), alignment_server);
//  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment_server), 0, 0, 12, 0);

  table1 = gtk_table_new (3, 2, FALSE);
  gtk_widget_show (table1);
  gtk_container_add (GTK_CONTAINER (alignment_server), table1);

  label_server_type = gtk_label_new (_("Server Type:"));
  gtk_widget_show (label_server_type);
  gtk_table_attach (GTK_TABLE (table1), label_server_type, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label_server_type), 0, 0.5);

  label_server_host = gtk_label_new (_("Host or Path:"));
  gtk_widget_show (label_server_host);
  gtk_table_attach (GTK_TABLE (table1), label_server_host, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label_server_host), 0, 0.5);

  label_server_port = gtk_label_new (_("Port:"));
  gtk_widget_show (label_server_port);
  gtk_table_attach (GTK_TABLE (table1), label_server_port, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label_server_port), 0, 0.5);

  input_server_port_adj = gtk_adjustment_new (16002, 0, 65535, 1, 10, 10);
  input_server_port = gtk_spin_button_new (GTK_ADJUSTMENT (input_server_port_adj), 1, 0);
  gtk_widget_show (input_server_port);
  gtk_table_attach (GTK_TABLE (table1), input_server_port, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  input_server_host = gtk_entry_new ();
  gtk_widget_show (input_server_host);
  gtk_table_attach (GTK_TABLE (table1), input_server_host, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

/*
  input_server_type = gtk_combo_box_entry_new_text ();
  gtk_widget_show (input_server_type);
  gtk_table_attach (GTK_TABLE (table1), input_server_type, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_combo_box_append_text (GTK_COMBO_BOX (input_server_type), _("Local"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (input_server_type), _("TCP/IP"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (input_server_type), _("DECnet"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (input_server_type), _("Fork"));
*/
  label_host = gtk_label_new (_("<b>Host:</b>"));
  gtk_widget_show (label_host);
//  gtk_frame_set_label_widget (GTK_FRAME (frame_server), label_host);
//  gtk_label_set_use_markup (GTK_LABEL (label_host), TRUE);

  label_server = gtk_label_new (_("Server & Network"));
  gtk_widget_show (label_server);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (tabs_main), gtk_notebook_get_nth_page (GTK_NOTEBOOK (tabs_main), 0), label_server);

  frame_proxy = gtk_frame_new (NULL);
  gtk_widget_show (frame_proxy);
  gtk_container_add (GTK_CONTAINER (tabs_main), frame_proxy);
  gtk_frame_set_shadow_type (GTK_FRAME (frame_proxy), GTK_SHADOW_NONE);

  alignment_proxy = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_widget_show (alignment_proxy);
  gtk_container_add (GTK_CONTAINER (frame_proxy), alignment_proxy);
//  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment_proxy), 0, 0, 12, 0);

  table2 = gtk_table_new (2, 2, FALSE);
  gtk_widget_show (table2);
  gtk_container_add (GTK_CONTAINER (alignment_proxy), table2);

  label_proxy_type = gtk_label_new (_("Type:"));
  gtk_widget_show (label_proxy_type);
  gtk_table_attach (GTK_TABLE (table2), label_proxy_type, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label_proxy_type), 0, 0.5);

  label_proxy_addr = gtk_label_new (_("Proxy Address"));
  gtk_widget_show (label_proxy_addr);
  gtk_table_attach (GTK_TABLE (table2), label_proxy_addr, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label_proxy_addr), 0, 0.5);

  input_proxy_host = gtk_entry_new ();
  gtk_widget_show (input_proxy_host);
  gtk_table_attach (GTK_TABLE (table2), input_proxy_host, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

/*
  input_proxy_type = gtk_combo_box_entry_new_text ();
  gtk_widget_show (input_proxy_type);
  gtk_table_attach (GTK_TABLE (table2), input_proxy_type, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);
  gtk_combo_box_append_text (GTK_COMBO_BOX (input_proxy_type), _("None"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (input_proxy_type), _("SOCKS4"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (input_proxy_type), _("SOCKS4a"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (input_proxy_type), _("SOCKS4d"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (input_proxy_type), _("HTTP Connect"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (input_proxy_type), _("SSH"));
*/

  label_frame_proxy = gtk_label_new (_("<b>Proxy</b>"));
  gtk_widget_show (label_frame_proxy);
/*
  gtk_frame_set_label_widget (GTK_FRAME (frame_proxy), label_frame_proxy);
  gtk_label_set_use_markup (GTK_LABEL (label_frame_proxy), TRUE);
*/

  label_proxy = gtk_label_new (_("Proxy"));
  gtk_widget_show (label_proxy);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (tabs_main), gtk_notebook_get_nth_page (GTK_NOTEBOOK (tabs_main), 1), label_proxy);

  vbox_meta = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox_meta);
  gtk_container_add (GTK_CONTAINER (tabs_main), vbox_meta);

  frame_meta_player = gtk_frame_new (NULL);
  gtk_widget_show (frame_meta_player);
  gtk_box_pack_start (GTK_BOX (vbox_meta), frame_meta_player, TRUE, TRUE, 0);
  gtk_frame_set_shadow_type (GTK_FRAME (frame_meta_player), GTK_SHADOW_NONE);

  alignment1 = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_widget_show (alignment1);
  gtk_container_add (GTK_CONTAINER (frame_meta_player), alignment1);
//  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment1), 0, 0, 12, 0);

  hbox_meta_player = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox_meta_player);
  gtk_container_add (GTK_CONTAINER (alignment1), hbox_meta_player);

  label_player_name = gtk_label_new (_("Player Name:"));
  gtk_widget_show (label_player_name);
  gtk_box_pack_start (GTK_BOX (hbox_meta_player), label_player_name, FALSE, FALSE, 0);

  input_meta_player_name = gtk_entry_new ();
  gtk_widget_show (input_meta_player_name);
  gtk_box_pack_start (GTK_BOX (hbox_meta_player), input_meta_player_name, TRUE, TRUE, 0);
  gtk_entry_set_text (GTK_ENTRY (input_meta_player_name), _("Audacious"));

  label_player = gtk_label_new (_("<b>Player</b>"));
  gtk_widget_show (label_player);
/*
  gtk_frame_set_label_widget (GTK_FRAME (frame_meta_player), label_player);
  gtk_label_set_use_markup (GTK_LABEL (label_player), TRUE);
*/

  frame_meta_title = gtk_frame_new (NULL);
  gtk_widget_show (frame_meta_title);
  gtk_box_pack_start (GTK_BOX (vbox_meta), frame_meta_title, TRUE, TRUE, 0);
  gtk_frame_set_shadow_type (GTK_FRAME (frame_meta_title), GTK_SHADOW_NONE);

  alignment2 = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_widget_show (alignment2);
  gtk_container_add (GTK_CONTAINER (frame_meta_title), alignment2);
//  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment2), 0, 0, 12, 0);

  hbox_meta_title = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox_meta_title);
  gtk_container_add (GTK_CONTAINER (alignment2), hbox_meta_title);

/*
  input_meta_send_title = gtk_check_button_new_with_mnemonic (_("Send song title to server"));
  gtk_widget_show (input_meta_send_title);
  gtk_box_pack_start (GTK_BOX (hbox_meta_title), input_meta_send_title, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (input_meta_send_title), TRUE);

  input_meta_send_filename = gtk_check_button_new_with_mnemonic (_("Send song filename to server"));
  gtk_widget_show (input_meta_send_filename);
  gtk_box_pack_start (GTK_BOX (hbox_meta_title), input_meta_send_filename, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (input_meta_send_filename), TRUE);
*/

  label_meta_title = gtk_label_new (_("<b>Current Song</b>"));
  gtk_widget_show (label_meta_title);
/*
  gtk_frame_set_label_widget (GTK_FRAME (frame_meta_title), label_meta_title);
  gtk_label_set_use_markup (GTK_LABEL (label_meta_title), TRUE);
*/

  label_meta = gtk_label_new (_("Meta Data"));
  gtk_widget_show (label_meta);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (tabs_main), gtk_notebook_get_nth_page (GTK_NOTEBOOK (tabs_main), 2), label_meta);

  hbox_buttons = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox_buttons);
  gtk_box_pack_start (GTK_BOX (vbox_main), hbox_buttons, TRUE, TRUE, 0);


  button_ok = gtk_button_new_with_label(_("OK"));
  gtk_box_pack_start (GTK_BOX (hbox_buttons), button_ok, FALSE, FALSE, 0);

  button_cancel = gtk_button_new_with_label(_("Cancel"));
  gtk_signal_connect_object(GTK_OBJECT(button_cancel), "clicked",
                            GTK_SIGNAL_FUNC(gtk_widget_destroy),
                            GTK_OBJECT(config_win));
  gtk_box_pack_start (GTK_BOX (hbox_buttons), button_cancel, FALSE, FALSE, 0);

/*
  button_ok = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (button_ok);
  gtk_box_pack_start (GTK_BOX (hbox_buttons), button_ok, FALSE, FALSE, 0);

  button_cancel = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (button_cancel);
  gtk_box_pack_start (GTK_BOX (hbox_buttons), button_cancel, FALSE, FALSE, 0);

  g_signal_connect_swapped ((gpointer) button_ok, "clicked",
                            G_CALLBACK (on_button_ok_enter),
                            GTK_OBJECT (config_win));
  g_signal_connect_swapped ((gpointer) button_cancel, "clicked",
                            G_CALLBACK (gtk_widget_destroy),
                            GTK_OBJECT (config_win));
*/
}

//ll
