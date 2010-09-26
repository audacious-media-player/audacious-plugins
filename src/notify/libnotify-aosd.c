/************************************************************************
 * libnotify-aosd.c							*
 *									*
 * Copyright (C) 2010 Maximilian Bogner	<max@mbogner.de>		*
 *									*
 * This program is free software; you can redistribute it and/or modify	*
 * it under the terms of the GNU General Public License as published by	*
 * the Free Software Foundation; either version 3 of the License,	*
 * or (at your option) any later version.				*
 *									*
 * This program is distributed in the hope that it will be useful,	*
 * but WITHOUT ANY WARRANTY; without even the implied warranty of	*
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the		*
 * GNU General Public License for more details.				*
 *									*
 * You should have received a copy of the GNU General Public License	*
 * along with this program; if not, see <http://www.gnu.org/licenses/>.	*
 ************************************************************************/

#include <glib.h>
#include <gtk/gtk.h>
#include <audacious/plugin.h>
#include <audacious/debug.h>
#include "libnotify-aosd_common.h"

// The pointers to the functions of the plugin
GeneralPlugin plugin_gp = {
    .description = PLUGIN_NAME " " PLUGIN_VERSION,
    .init = plugin_init,
    .about = plugin_about,
    .cleanup = plugin_cleanup
};

GeneralPlugin *plugin_gplist[] = {&plugin_gp, NULL};
SIMPLE_GENERAL_PLUGIN(libnotifyaosd, plugin_gplist);

short plugin_active = 0;

void plugin_init() {
	AUDDBG("started!\n");
	if(!osd_init()) {
		AUDDBG("osd_init failed!\n");
  		return;
	}
	event_init();

	plugin_active = 1;
	AUDDBG("done!\n");
}


void plugin_cleanup() {
	if(plugin_active) {
		AUDDBG("started!\n");
		event_uninit();
    		osd_uninit();
		plugin_active = 0;
		AUDDBG("done!\n");
	}
}

void plugin_about() {
	const gchar *authors[] = {PLUGIN_AUTHORS, NULL};

	gtk_show_about_dialog(NULL, "program-name", PLUGIN_NAME,
				"version", PLUGIN_VERSION,
				"license", PLUGIN_LICENSE,
				"wrap-license", TRUE,
				"copyright", PLUGIN_COPYRIGHT,
				"authors", authors,
				"website", PLUGIN_WEBSITE, NULL);
}
