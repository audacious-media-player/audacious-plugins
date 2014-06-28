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

#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudgui/libaudgui.h>

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
 "resident", "FALSE",
 nullptr
};

static bool plugin_init (void)
{
    aud_config_set_defaults ("notify", notify_defaults);

    if (! notify_init ("Audacious"))
        return FALSE;

    audgui_init ();
    event_init ();
    return TRUE;
}

static void plugin_cleanup (void)
{
    event_uninit ();
    audgui_cleanup ();
    notify_uninit ();
}

static void plugin_reinit (void)
{
    event_uninit ();
    event_init ();
}

static const PreferencesWidget prefs_widgets[] = {
    WidgetCheck (N_("Show playback controls"),
        WidgetBool ("notify", "actions", plugin_reinit)),
    WidgetCheck (N_("Always show notification"),
        WidgetBool ("notify", "resident", plugin_reinit))
};

static const PluginPreferences plugin_prefs = {
    prefs_widgets,
    ARRAY_LEN (prefs_widgets)
};

#define AUD_PLUGIN_NAME        N_("Desktop Notifications")
#define AUD_PLUGIN_ABOUT       plugin_about
#define AUD_PLUGIN_PREFS       & plugin_prefs
#define AUD_PLUGIN_INIT        plugin_init
#define AUD_PLUGIN_CLEANUP     plugin_cleanup

#define AUD_DECLARE_GENERAL
#include <libaudcore/plugin-declare.h>
