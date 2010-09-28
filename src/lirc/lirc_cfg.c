#include <config.h>

#include <gtk/gtk.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <audacious/configdb.h>

#include "common.h"

GtkWidget *lirc_cfg = NULL;

gint b_enable_reconnect;
gint reconnect_timeout;
gchar *aosd_font = NULL;

void load_cfg(void)
{
  mcs_handle_t *db;
  db = aud_cfg_db_open();
  aud_cfg_db_get_int(db, "lirc", "enable_reconnect", &b_enable_reconnect);
  aud_cfg_db_get_int(db, "lirc", "reconnect_timeout", &reconnect_timeout);
  if (!aud_cfg_db_get_string(db, "aosd", "text_fonts_name_0", &aosd_font))
    aosd_font = g_strdup("Sans 26");
  if (!reconnect_timeout)
  {
    reconnect_timeout = 5;
    b_enable_reconnect = 1;
  }
  aud_cfg_db_close(db);
}

void save_cfg(void)
{
  mcs_handle_t *db;
  db = aud_cfg_db_open();
  aud_cfg_db_set_int(db, "lirc", "enable_reconnect", b_enable_reconnect);
  aud_cfg_db_set_int(db, "lirc", "reconnect_timeout", reconnect_timeout);
  aud_cfg_db_close(db);
}

void configure(void)
{
  if (lirc_cfg)
  {
    gtk_window_present(GTK_WINDOW(lirc_cfg));
    return;
  }
  load_cfg();
  lirc_cfg = create_lirc_cfg();
  gtk_widget_show_all(lirc_cfg);
}
