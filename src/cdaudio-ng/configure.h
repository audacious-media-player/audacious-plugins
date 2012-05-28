#ifndef _CDNG_CONFIGURE_H
#define _CDNG_CONFIGURE_H

#include <libaudcore/core.h>

typedef struct cdng_cfg_t
{
    bool_t use_cdtext;
    bool_t use_cddb;
    char *device;
    char *cddb_server;
    char *cddb_path;
    int cddb_port;
    bool_t cddb_http;
    int disc_speed;
    bool_t use_proxy;
    char *proxy_host;
    int proxy_port;
    char *proxy_username;
    char *proxy_password;

} cdng_cfg_t;


void configure_create_gui (void);
void configure_show_gui (void);
int pstrcpy (char **, const char *);

#endif // _CDNG_CONFIGURE_H
