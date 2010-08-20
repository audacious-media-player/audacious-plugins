#ifndef _CDNG_CONFIGURE_H
#define _CDNG_CONFIGURE_H
#include <glib.h>

typedef struct cdng_cfg_t
{
    gboolean use_cdtext;
    gboolean use_cddb;
    gchar *device;
    gchar *cddb_server;
    gchar *cddb_path;
    gint cddb_port;
    gboolean cddb_http;
    gint disc_speed;
    gboolean use_proxy;
    gchar *proxy_host;
    gint proxy_port;
    gchar *proxy_username;
    gchar *proxy_password;

} cdng_cfg_t;


void configure_create_gui (void);
void configure_show_gui (void);
gint pstrcpy (gchar **, const gchar *);

#endif // _CDNG_CONFIGURE_H
