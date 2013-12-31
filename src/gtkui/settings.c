/*
 * settings.c
 * Copyright 2013 John Lindgren
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

#include "gtkui.h"

#include <audacious/i18n.h>
#include <audacious/preferences.h>

#include "ui_playlist_notebook.h"
#include "ui_playlist_widget.h"

static void redisplay_playlists (void)
{
    ui_playlist_notebook_empty ();
    ui_playlist_notebook_populate ();
}

static PreferencesWidget gtkui_widgets[] = {
    {WIDGET_LABEL, N_("<b>Playlist Tabs</b>")},
    {WIDGET_CHK_BTN, N_("Always show tabs"),
     .cfg_type = VALUE_BOOLEAN, .csect = "gtkui", .cname = "playlist_tabs_visible",
     .callback = show_playlist_tabs},
    {WIDGET_CHK_BTN, N_("Show entry counts"),
     .cfg_type = VALUE_BOOLEAN, .csect = "gtkui", .cname = "entry_count_visible",
     .callback = redisplay_playlists},
    {WIDGET_CHK_BTN, N_("Show close buttons"),
     .cfg_type = VALUE_BOOLEAN, .csect = "gtkui", .cname = "close_button_visible",
     .callback = redisplay_playlists},
    {WIDGET_LABEL, N_("<b>Playlist Columns</b>")},
    {WIDGET_CUSTOM, .data.populate = pw_col_create_chooser},
    {WIDGET_CHK_BTN, N_("Show column headers"),
     .cfg_type = VALUE_BOOLEAN, .csect = "gtkui", .cname = "playlist_headers",
     .callback = redisplay_playlists},
    {WIDGET_LABEL, N_("<b>Miscellaneous</b>")},
    {WIDGET_CHK_BTN, N_("Scroll on song change"),
     .cfg_type = VALUE_BOOLEAN, .csect = "gtkui", .cname = "autoscroll"}
};

const PluginPreferences gtkui_prefs = {
    .widgets = gtkui_widgets,
    .n_widgets = ARRAY_LEN (gtkui_widgets)
};

