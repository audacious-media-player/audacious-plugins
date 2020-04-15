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

#include <QApplication>
#include <QLabel>
#include <QPixmap>

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/hook.h>
#include <libaudcore/templates.h>

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
        qreal r = qApp->devicePixelRatio ();
        origPixmap.setDevicePixelRatio (r);
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
    static constexpr int MARGIN = 4;

    const HookReceiver<ArtLabel>
        update_hook{"playback ready", this, &ArtLabel::update_art},
        clear_hook{"playback stop", this, &ArtLabel::clear};

    QPixmap origPixmap;
    QSize origSize;

    void init ()
    {
        clear ();
        setMinimumSize (MARGIN + 1, MARGIN + 1);
        setAlignment (Qt::AlignCenter);

        if (aud_drct_get_ready())
            update_art();
    }

    void drawArt ()
    {
        qreal r = qApp->devicePixelRatio();
        if (aud::abs(r - 1.0) <= 0.01 &&
            origSize.width () <= size ().width () - MARGIN &&
            origSize.height () <= size ().height() - MARGIN) {
            // If device pixel ratio is close to 1:1 (within 1%) and art is
            // smaller than widget, set pixels directly w/o scaling
            setPixmap (origPixmap);
        } else {
            // Otherwise scale image with device pixel ratio, but limit size to
            // widget dimensions.
            auto width = std::min(r * (size().width() - MARGIN),
                                  r * origSize.width());
            auto height = std::min(r * (size().height() - MARGIN),
                                   r * origSize.height());

            setPixmap (origPixmap.scaled (width, height, Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));
        }

#ifdef Q_OS_MAC
	repaint();
#endif
    }
};

void * AlbumArtQt::get_qt_widget ()
{
    return new ArtLabel;
}

EXPORT AlbumArtQt aud_plugin_instance;
