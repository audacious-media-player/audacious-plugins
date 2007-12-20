#ifndef _CDNG_CONFIGURE_H
#define _CDNG_CONFIGURE_H
#include <glib.h>

extern struct cdng_cfg_t {

	gboolean	use_dae;
	gboolean	use_cdtext;
	gboolean	use_cddb;
	gboolean	debug;
	gchar		*device;
	gchar		*cddb_server;
	gint		cddb_port;
	gboolean	cddb_http;
	gint		limitspeed;
	gboolean	use_proxy;
	gchar		*proxy_host;
	gint		proxy_port;
	gchar		*proxy_username;
	gchar		*proxy_password;

} cdng_cfg;


void	configure_create_gui(void);
void	configure_show_gui(void);
gint	pstrcpy(gchar **, const gchar *);

#endif	// _CDNG_CONFIGURE_H

