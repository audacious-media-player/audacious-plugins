/*
 * preferences.h
 * Copyright (C) 2023-2024 Igor Kushnir <igorkuo@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef AUDACIOUS_PLAYBACK_HISTORY_PREFERENCES_H
#define AUDACIOUS_PLAYBACK_HISTORY_PREFERENCES_H

#include <libaudcore/i18n.h>
#include <libaudcore/preferences.h>
#include <libaudcore/templates.h>

#include "history-entry.h"

static constexpr const char * configSection = "playback-history";
static constexpr const char * configEntryType = "entry_type";

static const char * const defaults[] = {
    configEntryType,
    aud::numeric_string<static_cast<int>(HistoryEntry::defaultType)>::str,
    nullptr};

static const PreferencesWidget widgets[] = {
    WidgetLabel(N_("<b>History Item Granularity</b>")),
    WidgetRadio(N_("Song"), WidgetInt(configSection, configEntryType),
                {static_cast<int>(HistoryEntry::Type::Song)}),
    WidgetRadio(N_("Album"), WidgetInt(configSection, configEntryType),
                {static_cast<int>(HistoryEntry::Type::Album)})};

static constexpr const char * aboutText =
    N_("Playback History Plugin\n\n"
       "Copyright 2023-2024 Igor Kushnir <igorkuo@gmail.com>\n\n"
       "This plugin tracks and provides access to playback history.\n\n"

       "History entries are stored only in memory and are lost\n"
       "on Audacious exit. When the plugin is disabled,\n"
       "playback history is not tracked at all.\n"
       "History entries are only added, never removed automatically.\n"
       "The user can remove selected entries by pressing the Delete key.\n"
       "Restart Audacious or disable the plugin by closing\n"
       "Playback History view to clear the entries.\n\n"

       "Two history item granularities (modes) are supported.\n"
       "The user can select a mode in the plugin's settings.\n"
       "The Song mode is the default. Each played song is stored\n"
       "in history. Song titles are displayed in the list.\n"
       "When the Album mode is selected and multiple songs from\n"
       "a single album are played in a row, a single album entry\n"
       "is stored in history. Album names are displayed in the list.");

#endif // AUDACIOUS_PLAYBACK_HISTORY_PREFERENCES_H
