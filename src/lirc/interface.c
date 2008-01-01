#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <audacious/i18n.h>

#include "common.h"

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
  GtkObject *reconnectspin_adj;
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

  dialog_vbox1 = GTK_DIALOG (lirc_cfg)->vbox;

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

  dialog_action_area1 = GTK_DIALOG (lirc_cfg)->action_area;
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area1), GTK_BUTTONBOX_END);

  cancelbutton1 = gtk_button_new_from_stock ("gtk-cancel");
  gtk_dialog_add_action_widget (GTK_DIALOG (lirc_cfg), cancelbutton1, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton1, GTK_CAN_DEFAULT);

  okbutton1 = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (lirc_cfg), okbutton1, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton1, GTK_CAN_DEFAULT);

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

