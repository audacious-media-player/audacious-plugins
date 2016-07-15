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

#include <QLabel>
#include <QPixmap>

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/hook.h>

#include <libaudqt/libaudqt.h>

class AlbumArtQt : public GeneralPlugin {
public:
    static constexpr PluginInfo info = {
        N_("Album Art"),
        PACKAGE,
        nullptr, // about
        nullptr, // prefs
        PluginQtOnly
    };

    constexpr AlbumArtQt () : GeneralPlugin (info, false) {}
    void * get_qt_widget ();
};

#define MARGIN 4

class ArtLabel : public QLabel {
public:
    ArtLabel (QWidget * parent = 0, Qt::WindowFlags f = 0) : QLabel(parent, f)
    {
        init ();
    }

    ArtLabel (const QString & text, QWidget * parent = 0, Qt::WindowFlags f = 0) : QLabel (text, parent, f)
    {
        init ();
    }

    void update_art ()
    {
        origPixmap = QPixmap (audqt::art_request_current (0, 0));
        origSize = origPixmap.size ();
        drawArt ();
    }

    void clear ()
    {
        QLabel::clear ();
        origPixmap = QPixmap ();
    }

protected:
    virtual void resizeEvent (QResizeEvent * event)
    {
        QLabel::resizeEvent (event);
        const QPixmap * pm = pixmap ();

        if ( ! origPixmap.isNull () && pm && ! pm->isNull () &&
                (size ().width () <= origSize.width () + MARGIN ||
                 size ().height () <= origSize.height () + MARGIN ||
                 pm->size ().width () != origSize.width () ||
                 pm->size ().height () != origSize.height ()))
            drawArt ();
    }

private:
    QPixmap origPixmap;
    QSize origSize;

    void init ()
    {
        clear ();
        setMinimumSize (MARGIN + 1, MARGIN + 1);
        setAlignment (Qt::AlignCenter);
    }

    void drawArt ()
    {
        if (origSize.width () <= size ().width () - MARGIN &&
            origSize.height () <= size ().height() - MARGIN)
            setPixmap (origPixmap);
        else
            setPixmap (origPixmap.scaled (size ().width () - MARGIN, size ().height () - MARGIN,
                        Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
};

#undef MARGIN

static void update (void *, ArtLabel * widget)
{
    widget->update_art ();
}

static void clear (void *, ArtLabel * widget)
{
    widget->clear ();
}

static void widget_cleanup (QObject * widget)
{
    hook_dissociate ("playback ready", (HookFunction) update, widget);
    hook_dissociate ("playback stop", (HookFunction) clear, widget);
}

void * AlbumArtQt::get_qt_widget ()
{
    ArtLabel * widget = new ArtLabel;

    QObject::connect (widget, &QObject::destroyed, widget_cleanup);

    hook_associate ("playback ready", (HookFunction) update, widget);
    hook_associate ("playback stop", (HookFunction) clear, widget);

    if (aud_drct_get_ready ())
        widget->update_art ();

    return widget;
}

EXPORT AlbumArtQt aud_plugin_instance;
