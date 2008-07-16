#include <gtk/gtk.h>
#include <glib.h>
#include <dbus/dbus-glib.h>
void run_agent(void);
int setup_agents(DBusGConnection *conn);
void cleanup_agents(void);

int register_agents(void);
void unregister_agents(void);

void show_agents(void);
void set_auto_authorize(gboolean value);
