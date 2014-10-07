/*
 * albumart.cc
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
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/hook.h>

#include <libaudqt/libaudqt.h>

class AlbumArt : public GeneralPlugin {
public:
    static constexpr PluginInfo info = {
        N_("Album Art"),
        PACKAGE
    };

    constexpr AlbumArt () : GeneralPlugin (info, false) {}
    void * get_qt_widget ();

private:
    static void update (void * unused, QLabel * widget);
    static void clear (void * unused, QLabel * widget);
    static void widget_cleanup (QObject * widget);
};

void AlbumArt::update (void * unused, QLabel * widget)
{
    if (! aud_drct_get_playing ())
        return;

    if (! widget)
        return;

    QImage img = audqt::art_request_current ();
    QSize size = widget->size ();

    widget->setPixmap (QPixmap::fromImage (img.scaled (size.width (), size.height (), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
}

void AlbumArt::clear (void * unused, QLabel * widget)
{
    if (! widget)
        return;

    widget->setPixmap (QPixmap ());
}

void AlbumArt::widget_cleanup (QObject * widget)
{
    hook_dissociate_full ("playback begin", (HookFunction) update, widget);
    hook_dissociate_full ("current art ready", (HookFunction) update, widget);
    hook_dissociate_full ("playback stop", (HookFunction) clear, widget);
}

void * AlbumArt::get_qt_widget ()
{
    QLabel * widget = new QLabel;

    QObject::connect (widget, &QObject::destroyed, widget_cleanup);

    hook_associate ("playback begin", (HookFunction) update, widget);
    hook_associate ("current art ready", (HookFunction) update, widget);
    hook_associate ("playback stop", (HookFunction) clear, widget);

    widget->resize (96, 96);
    update (nullptr, widget);

    return widget;
}

EXPORT AlbumArt aud_plugin_instance;
