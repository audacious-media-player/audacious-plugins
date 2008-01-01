#include <config.h>

#include <gtk/gtk.h>

#include "common.h"

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
