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
    // clang-format off
    "infoarea_show_vis", "TRUE",
    "infoarea_visible", "TRUE",
    "menu_visible", "TRUE",
    "playlist_tabs_visible", aud::numeric_string<PlaylistTabVisibility::AutoHide>::str,
    "statusbar_visible", "TRUE",
    "entry_count_visible", "FALSE",
    "close_button_visible", "TRUE",

    "autoscroll", "TRUE",
    "playlist_columns", DEFAULT_COLUMNS,
    "playlist_headers", "TRUE",
    "show_remaining_time", "FALSE",
    // clang-format on
    nullptr};

static void qtui_update_playlist_settings()
{
    hook_call("qtui update playlist settings", nullptr);
}

static const ComboItem playlist_tabs_options[] = {
    ComboItem(N_("Always"), PlaylistTabVisibility::Always),
    ComboItem(N_("Auto-hide"), PlaylistTabVisibility::AutoHide),
    ComboItem(N_("Never"), PlaylistTabVisibility::Never)};

static const PreferencesWidget qtui_widgets[] = {
    WidgetLabel(N_("<b>Playlist Tabs</b>")),
    WidgetCombo(N_("Show playlist tabs:"),
                WidgetInt("qtui", "playlist_tabs_visible",
                          qtui_update_playlist_settings),
                {{playlist_tabs_options}}),
    WidgetCheck(N_("Show entry counts"),
                WidgetBool("qtui", "entry_count_visible",
                           qtui_update_playlist_settings)),
    WidgetCheck(N_("Show close buttons"),
                WidgetBool("qtui", "close_button_visible",
                           qtui_update_playlist_settings)),
    WidgetLabel(N_("<b>Playlist Columns</b>")),
    WidgetCheck(
        N_("Show column headers"),
        WidgetBool("qtui", "playlist_headers", qtui_update_playlist_settings)),
    WidgetLabel(N_("<b>Miscellaneous</b>")),
    WidgetCheck(N_("Scroll on song change"), WidgetBool("qtui", "autoscroll"))};

const PluginPreferences qtui_prefs = {{qtui_widgets}};
