#ifndef _CDNG_CONFIGURE_H
#define _CDNG_CONFIGURE_H
#include <glib.h>

extern struct cdng_cfg_t {
	gboolean	use_dae,
			use_cdtext,
			use_cddb,
			debug;
	gchar		*device,
			*cddb_server;
	gint		cddb_port,
			limitspeed;
} cdng_cfg;


void	configure_create_gui(void);
void	configure_show_gui(void);
gint	pstrcpy(gchar **, const gchar *);

#endif	// _CDNG_CONFIGURE_H
