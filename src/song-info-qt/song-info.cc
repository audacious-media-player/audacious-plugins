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
#include <libaudcore/probe.h>

#include <libaudqt/libaudqt.h>
#include <libaudqt/info-widget.h>

class SongInfo : public GeneralPlugin {
public:
    static constexpr PluginInfo info = {
        N_("Song Info"),
        PACKAGE,
        nullptr, // about
        nullptr, // prefs
        PluginQtOnly
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

    auto playlist = Playlist::playing_playlist ();

    if (playlist == Playlist ())
        playlist = Playlist::active_playlist ();

    int position = playlist.get_position ();
    if (position == -1)
        return;

    String filename = playlist.entry_filename (position);
    if (! filename)
        return;

    PluginHandle * decoder = playlist.entry_decoder (position);
    if (! decoder)
        return;

    Tuple tuple = playlist.entry_tuple (position);
    if (tuple.valid ())
        widget->fillInfo (filename, tuple, decoder,
                aud_file_can_write_tuple (filename, decoder));
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

    update(nullptr, widget);

    return widget;
}

EXPORT SongInfo aud_plugin_instance;
