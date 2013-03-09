/*
 * notify.c
 *
 * Copyright (C) 2010 Maximilian Bogner <max@mbogner.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <gtk/gtk.h>

#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <audacious/debug.h>

#include "event.h"
#include "osd.h"

gboolean plugin_init (void);
void plugin_cleanup (void);

static const char plugin_about[] =
 N_("Based on libnotify-aosd by Maximilian Bogner:\n"
    "http://www.mbogner.de/projects/libnotify-aosd/\n\n"
    "Copyright (C) 2010 Maximilian Bogner\n"
    "Copyright (C) 2011 John Lindgren\n\n"
    "This plugin is free software: you can redistribute it and/or modify "
    "it under the terms of the GNU General Public License as published by "
    "the Free Software Foundation, either version 3 of the License, or "
    "(at your option) any later version.\n\n"
    "This plugin is distributed in the hope that it will be useful, "
    "but WITHOUT ANY WARRANTY; without even the implied warranty of "
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
    "GNU General Public License for more details.\n\n"
    "You should have received a copy of the GNU General Public License "
    "along with this program.  If not, see <http://www.gnu.org/licenses/>.");

AUD_GENERAL_PLUGIN
(
    .name = N_("Desktop Notifications"),
    .domain = PACKAGE,
    .about_text = plugin_about,
    .init = plugin_init,
    .cleanup = plugin_cleanup
)

short plugin_active = 0;

gboolean plugin_init (void)
{
    AUDDBG("started!\n");
    if(!osd_init()) {
        AUDDBG("osd_init failed!\n");
        return FALSE;
    }
    event_init();

    plugin_active = 1;
    return TRUE;
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
