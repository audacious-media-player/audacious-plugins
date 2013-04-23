/*
 * notify.c
 *
 * Copyright (C) 2010 Maximilian Bogner <max@mbogner.de>
 * Copyright (C) 2013 John Lindgren and Jean-Alexandre Anglès d'Auriac
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

#include <gtk/gtk.h>

#include <libnotify/notify.h>

#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <audacious/preferences.h>
#include <audacious/misc.h>

#include "event.h"

static const char plugin_about[] =
 N_("Desktop Notifications Plugin for Audacious\n"
    "Copyright (C) 2010 Maximilian Bogner\n"
    "Copyright (C) 2011-2013 John Lindgren and Jean-Alexandre Anglès d'Auriac\n\n"
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

static const char * const notify_defaults[] = {
 "actions", "TRUE",
 "resident", "TRUE",
 NULL
};

static bool_t plugin_init (void)
{
    aud_config_set_defaults ("notify", notify_defaults);

    if (! notify_init ("Audacious"))
        return FALSE;

    event_init();
    return TRUE;
}

static void plugin_cleanup (void)
{
    event_uninit ();
    notify_uninit ();
}

static void plugin_reinit (void)
{
    event_uninit ();
    event_init ();
}

static const PreferencesWidget prefs_widgets[] = {
 {WIDGET_CHK_BTN, N_("Show playback controls"),
  .cfg_type = VALUE_BOOLEAN, .csect = "notify", .cname = "actions",
  .callback = plugin_reinit},
 {WIDGET_CHK_BTN, N_("Always show notification"),
  .cfg_type = VALUE_BOOLEAN, .csect = "notify", .cname = "resident",
  .callback = plugin_reinit}
};

static const PluginPreferences plugin_prefs = {
 .widgets = prefs_widgets,
 .n_widgets = G_N_ELEMENTS (prefs_widgets)
};

AUD_GENERAL_PLUGIN
(
    .name = N_("Desktop Notifications"),
    .domain = PACKAGE,
    .about_text = plugin_about,
    .prefs = & plugin_prefs,
    .init = plugin_init,
    .cleanup = plugin_cleanup
)
