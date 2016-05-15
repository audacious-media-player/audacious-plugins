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

#include <libnotify/notify.h>

#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>

#include "event.h"

class NotifyPlugin : public GeneralPlugin
{
public:
    static const char about[];
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("Desktop Notifications"),
        PACKAGE,
        about,
        & prefs
    };

    constexpr NotifyPlugin () : GeneralPlugin (info, false) {}

    bool init ();
    void cleanup ();

private:
    static void reinit ();
};

EXPORT NotifyPlugin aud_plugin_instance;

const char NotifyPlugin::about[] =
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

const char * const NotifyPlugin::defaults[] = {
 "actions", "TRUE",
 "resident", "FALSE",
 "album", "TRUE",
 nullptr
};

bool NotifyPlugin::init ()
{
    aud_config_set_defaults ("notify", defaults);

    if (! notify_init ("Audacious"))
        return false;

    event_init ();
    return true;
}

void NotifyPlugin::cleanup ()
{
    event_uninit ();
    notify_uninit ();
}

void NotifyPlugin::reinit ()
{
    event_uninit ();
    event_init ();
}

const PreferencesWidget NotifyPlugin::widgets[] = {
    WidgetCheck (N_("Show playback controls"),
        WidgetBool ("notify", "actions", reinit)),
    WidgetCheck (N_("Always show notification"),
        WidgetBool ("notify", "resident", reinit)),
    WidgetCheck (N_("Include album name in notification"),
        WidgetBool ("notify", "album", reinit))
};

const PluginPreferences NotifyPlugin::prefs = {{widgets}};
