/*
 * song-info.cc
 * Copyright 2014 William Pitcock
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

#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>

#include <libaudqt/libaudqt.h>
#include <libaudqt/info-widget.h>

class SongInfo : public GeneralPlugin {
public:
    static constexpr PluginInfo info = {
        N_("Song Info (Qt)"),
        PACKAGE
    };

    constexpr SongInfo () : GeneralPlugin (info, false) {}
    void * get_qt_widget ();

private:
    static void update (void * unused, audqt::InfoWidget * widget);
    static void clear (void * unused, audqt::InfoWidget * widget);
    static void widget_cleanup (QObject * widget);
};

void SongInfo::update (void * unused, audqt::InfoWidget * widget)
{
    if (! aud_drct_get_playing ())
        return;

    if (! widget)
        return;

    int playlist = aud_playlist_get_playing ();

    if (playlist == -1)
        playlist = aud_playlist_get_active ();

    int position = aud_playlist_get_position (playlist);
    if (position == -1)
        return;

    String filename = aud_playlist_entry_get_filename (playlist, position);
    if (! filename)
        return;

    PluginHandle * decoder = aud_playlist_entry_get_decoder (playlist, position);
    if (! decoder)
        return;

    Tuple tuple = aud_playlist_entry_get_tuple (playlist, position);
    if (tuple)
        widget->fillInfo (playlist, position, filename, tuple, decoder, false);
}

void SongInfo::clear (void * unused, audqt::InfoWidget * widget)
{
    if (! widget)
        return;
}

void SongInfo::widget_cleanup (QObject * widget)
{
    hook_dissociate ("playback begin", (HookFunction) update, widget);
    hook_dissociate ("playback stop", (HookFunction) clear, widget);
}

void * SongInfo::get_qt_widget ()
{
    audqt::InfoWidget * widget = new audqt::InfoWidget;

    QObject::connect (widget, &QObject::destroyed, widget_cleanup);

    hook_associate ("playback begin", (HookFunction) update, widget);
    hook_associate ("playback stop", (HookFunction) clear, widget);

    return widget;
}

EXPORT SongInfo aud_plugin_instance;
