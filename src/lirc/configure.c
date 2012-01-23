/* Audacious Lirc plugin - configure.c

   Copyright (C) 2012 Joonas Harjumäki (jharjuma@gmail.com)

   Copyright (C) 2005 Audacious development team

   Copyright (c) 1998-1999 Carl van Schaik (carl@leg.uct.ac.za)

   Copyright (C) 2000 Christoph Bartelmus (xmms@bartelmus.de)

   some code was stolen from:
   IRman plugin for xmms by Charles Sielski (stray@teklabs.net)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "config.h"

#include <gtk/gtk.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <audacious/misc.h>
#include <gdk/gdkkeysyms.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "configure.h"

static const gchar * const lirc_defaults[] = {
 "enable_reconnect", "1",
 "reconnect_timeout", "5",
 "text_fonts_name_0", "Sans 26",
 NULL};

gint b_enable_reconnect;
gint reconnect_timeout;
gchar *aosd_font = NULL;

GtkWidget *lirc_cfg;

void load_cfg(void)
{
  aud_config_set_defaults ("lirc", lirc_defaults);
  b_enable_reconnect = aud_get_int("lirc", "enable_reconnect");
  reconnect_timeout = aud_get_int("lirc", "reconnect_timeout");
  aosd_font = aud_get_string("lirc", "text_fonts_name_0");

}

void save_cfg(void)
{
  aud_set_int("lirc", "enable_reconnect", b_enable_reconnect);
  aud_set_int("lirc", "reconnect_timeout", reconnect_timeout);
  aud_set_string("lirc", "text_fonts_name_0", aosd_font);
}

void configure(void) {
	if (lirc_cfg) {
		gtk_window_present(GTK_WINDOW(lirc_cfg));
		return;
	}
	load_cfg();
	lirc_cfg = create_lirc_cfg();
	gtk_widget_show_all(lirc_cfg);

}

void
on_reconnectcheck_toggled              (GtkToggleButton *togglebutton,
                                        GtkWidget       *reconnectspin)
{
  gtk_widget_set_sensitive(reconnectspin, gtk_toggle_button_get_active(togglebutton));
}


void
on_cancelbutton1_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
  gtk_widget_destroy(lirc_cfg);
  lirc_cfg=NULL;
}


void
on_okbutton1_clicked                   (GtkButton       *button,
                                        gpointer         user_data)
{
  b_enable_reconnect=(gint)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(lirc_cfg), "reconnectcheck")));
  reconnect_timeout=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(lirc_cfg), "reconnectspin")));
  save_cfg();
  gtk_widget_destroy(lirc_cfg);
  lirc_cfg=NULL;
}

GtkWidget*
create_lirc_cfg (void)
{
  GtkWidget *dialog_vbox1;
  GtkWidget *notebook1;
  GtkWidget *vbox2;
  GtkWidget *frame1;
  GtkWidget *alignment1;
  GtkWidget *vbox3;
  GtkWidget *hbox1;
  GtkWidget *label3;
  GtkWidget *reconnectcheck;
  GtkWidget *reconnectspin;
  GtkAdjustment *reconnectspin_adj;
  GtkWidget *label2;
  GtkWidget *label1;
  GtkWidget *dialog_action_area1;
  GtkWidget *cancelbutton1;
  GtkWidget *okbutton1;

  lirc_cfg = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (lirc_cfg), _("LIRC plugin settings"));
  gtk_window_set_position (GTK_WINDOW (lirc_cfg), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_resizable (GTK_WINDOW (lirc_cfg), FALSE);
  gtk_window_set_type_hint (GTK_WINDOW (lirc_cfg), GDK_WINDOW_TYPE_HINT_DIALOG);
  g_signal_connect(G_OBJECT(lirc_cfg),"destroy", G_CALLBACK(gtk_widget_destroyed), &lirc_cfg);

  dialog_vbox1 = gtk_dialog_get_content_area(GTK_DIALOG (lirc_cfg));

  notebook1 = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), notebook1, TRUE, TRUE, 0);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (notebook1), vbox2);

  frame1 = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (vbox2), frame1, TRUE, TRUE, 0);

  alignment1 = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_container_add (GTK_CONTAINER (frame1), alignment1);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment1), 0, 0, 12, 0);

  vbox3 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (alignment1), vbox3);

  reconnectcheck = gtk_check_button_new_with_mnemonic (_("Reconnect to LIRC server"));
  gtk_box_pack_start (GTK_BOX (vbox3), reconnectcheck, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (reconnectcheck), (gboolean)b_enable_reconnect);

  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox3), hbox1, TRUE, TRUE, 0);

  label3 = gtk_label_new (_("Timeout before reconnecting (seconds): "));
  gtk_box_pack_start (GTK_BOX (hbox1), label3, FALSE, FALSE, 17);

  reconnectspin_adj = gtk_adjustment_new (reconnect_timeout, 1, 100, 1, 10, 10);
  reconnectspin = gtk_spin_button_new (GTK_ADJUSTMENT (reconnectspin_adj), 1, 0);
  gtk_box_pack_start (GTK_BOX (hbox1), reconnectspin, TRUE, TRUE, 15);

  label2 = gtk_label_new (_("Reconnect"));
  gtk_frame_set_label_widget (GTK_FRAME (frame1), label2);
  gtk_label_set_use_markup (GTK_LABEL (label2), TRUE);

  label1 = gtk_label_new (_("Connection"));
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook1), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook1), 0), label1);

  dialog_action_area1 = gtk_dialog_get_action_area(GTK_DIALOG (lirc_cfg));
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area1), GTK_BUTTONBOX_END);

  cancelbutton1 = gtk_button_new_from_stock ("gtk-cancel");
  gtk_dialog_add_action_widget (GTK_DIALOG (lirc_cfg), cancelbutton1, GTK_RESPONSE_CANCEL);
  gtk_widget_set_can_default(cancelbutton1, TRUE);

  okbutton1 = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (lirc_cfg), okbutton1, GTK_RESPONSE_OK);
  gtk_widget_set_can_default(okbutton1, TRUE);

  g_signal_connect (G_OBJECT (reconnectcheck), "toggled",
                    G_CALLBACK (on_reconnectcheck_toggled),
                    G_OBJECT (reconnectspin));
  g_signal_connect (G_OBJECT (cancelbutton1), "clicked",
                    G_CALLBACK (on_cancelbutton1_clicked),
                    NULL);
  g_signal_connect (G_OBJECT (okbutton1), "clicked",
                    G_CALLBACK (on_okbutton1_clicked),
                    NULL);
  gtk_widget_set_sensitive(reconnectspin, (gboolean)b_enable_reconnect);

  g_object_set_data(G_OBJECT(lirc_cfg), "reconnectcheck", G_OBJECT(reconnectcheck));
  g_object_set_data(G_OBJECT(lirc_cfg), "reconnectspin", G_OBJECT(reconnectspin));

  return lirc_cfg;
}
