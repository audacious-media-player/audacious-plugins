/*
 * settings.cc
 * Copyright 2015 Eugene Paskevich
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include "settings.h"

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/preferences.h>

#include <QApplication>
#include <QDesktopWidget>

const char * const qtui_defaults[] = {
    "infoarea_show_vis", "TRUE",
    "infoarea_visible", "TRUE",
    "menu_visible", "TRUE",
    "playlist_tabs_visible", "TRUE",
    "statusbar_visible", "TRUE",
    "entry_count_visible", "FALSE",
    "close_button_visible", "TRUE",

    "autoscroll", "TRUE",
    "playlist_columns", DEFAULT_COLUMNS,
    "playlist_headers", "TRUE",
//    "record", "FALSE",
    "show_remaining_time", "FALSE",
    "step_size", "5",

    nullptr
};

static void qtui_update_playlist_settings ()
{
    hook_call ("qtui update playlist settings", nullptr);
}

static const PreferencesWidget qtui_widgets[] = {
    WidgetLabel (N_("<b>Playlist Tabs</b>")),
#if QT_VERSION >= 0x050400
    WidgetCheck (N_("Always show tabs"),
        WidgetBool ("qtui", "playlist_tabs_visible", qtui_update_playlist_settings)),
#endif
    WidgetCheck (N_("Show entry counts"),
        WidgetBool ("qtui", "entry_count_visible", qtui_update_playlist_settings)),
    WidgetCheck (N_("Show close buttons"),
        WidgetBool ("qtui", "close_button_visible", qtui_update_playlist_settings)),
    WidgetLabel (N_("<b>Playlist Columns</b>")),
    WidgetCheck (N_("Show column headers"),
        WidgetBool ("qtui", "playlist_headers", qtui_update_playlist_settings)),
    WidgetLabel (N_("<b>Miscellaneous</b>")),
    WidgetSpin (N_("Arrow keys seek by:"),
        WidgetFloat ("qtui", "step_size"),
        {0.1, 60, 0.1, N_("seconds")}),
    WidgetCheck (N_("Scroll on song change"),
        WidgetBool ("qtui", "autoscroll"))
};

const PluginPreferences qtui_prefs = {{qtui_widgets}};

int getDPI ()
{
    static int dpi = 0;

    if (! dpi)
    {
        auto desktop = qApp->desktop ();
        dpi = aud::max (96, (desktop->logicalDpiX () + desktop->logicalDpiY ()) / 2);
    }

    return dpi;
}
