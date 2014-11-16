/*
 * info_bar.cc
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

#include <cmath>

#include "info_bar.h"

#include <libaudcore/hook.h>
#include <libaudcore/index.h>
#include <libaudcore/objects.h>
#include <libaudcore/runtime.h>
#include <libaudcore/interface.h>
#include <libaudcore/tuple.h>
#include <libaudcore/drct.h>
#include <libaudqt/libaudqt.h>

#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QFont>

static VisItem * s_vis = nullptr;

VisItem::VisItem (QGraphicsItem * parent) : QGraphicsItem (parent)
{
    /* XXX - need something like hookreceiver to get rid of this junk */
    s_vis = this;

    aud_vis_func_add (AUD_VIS_TYPE_CLEAR, (VisFunc) VisItem::vis_clear_cb);
    aud_vis_func_add (AUD_VIS_TYPE_FREQ, (VisFunc) VisItem::vis_render_cb);
}

QRectF VisItem::boundingRect () const
{
    return QRectF (0.0, 0.0, InfoBar::VisWidth, InfoBar::Height);
}

void VisItem::paint (QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget)
{
    QColor c = Qt::blue;

    painter->fillRect (boundingRect (), QColor (0, 0, 0, 0));
    painter->setPen (QPen (c, 1, Qt::SolidLine, Qt::SquareCap, Qt::BevelJoin));

    for (int i = 0; i < InfoBar::VisBands; i++)
    {
        int x = InfoBar::Spacing + 8 * i;
        int q = InfoBar::IconSize - m_bars[i];
        int t = -(InfoBar::IconSize - q);

        painter->fillRect (x, InfoBar::IconSize, 6, t, c);
    }
}

void VisItem::vis_render_cb (const float * freq)
{
    /* xscale[i] = pow (256, i / VIS_BANDS) - 0.5; */
    const float xscale[InfoBar::VisBands + 1] = {0.5, 1.09, 2.02, 3.5, 5.85, 9.58,
     15.5, 24.9, 39.82, 63.5, 101.09, 160.77, 255.5};

    for (int i = 0; i < InfoBar::VisBands; i ++)
    {
        int a = ceilf (xscale[i]);
        int b = floorf (xscale[i + 1]);
        float n = 0;

        if (b < a)
            n += freq[b] * (xscale[i + 1] - xscale[i]);
        else
        {
            if (a > 0)
                n += freq[a - 1] * (a - xscale[i]);
            for (; a < b; a ++)
                n += freq[a];
            if (b < 256)
                n += freq[b] * (xscale[i + 1] - b);
        }

        /* 40 dB range */
        int x = 40 + 20 * log10f (n);
        x = aud::clamp (x, 0, 40);

        s_vis->m_bars[i] -= aud::max (0, InfoBar::VisFalloff - s_vis->m_delay[i]);

        if (s_vis->m_delay[i])
            s_vis->m_delay[i] --;

        if (x > s_vis->m_bars[i])
        {
            s_vis->m_bars[i] = x;
            s_vis->m_delay[i] = InfoBar::VisDelay;
        }
    }

    s_vis->update ();
}

void VisItem::vis_clear_cb ()
{
    memset (s_vis->m_bars, 0, sizeof m_bars);
    memset (s_vis->m_delay, 0, sizeof m_delay);

    s_vis->update ();
}

AlbumArtItem::AlbumArtItem (QGraphicsItem * parent) : QGraphicsPixmapItem (parent),
    hook1 ("playback begin", this, & AlbumArtItem::update_cb),
    hook2 ("current art ready", this, & AlbumArtItem::update_cb)
{
}

void AlbumArtItem::update_cb ()
{
    setPixmap (audqt::art_request_current (InfoBar::IconSize, InfoBar::IconSize));
}

InfoBar::InfoBar (QWidget * parent) : QGraphicsView (parent),
    m_scene (new QGraphicsScene (this)),
    m_art (new AlbumArtItem),
    m_title_text (new QGraphicsTextItem),
    m_album_text (new QGraphicsTextItem),
    m_artist_text (new QGraphicsTextItem),
#ifdef XXX_NOTYET
    m_vis (new VisItem),
#endif
    hook1 ("tuple change", this, & InfoBar::update_metadata_cb),
    hook2 ("playback begin", this, & InfoBar::update_metadata_cb)
{
    QLinearGradient gradient (QPointF (0.0, 0.0), QPointF (0.0, 100.0));

    gradient.setColorAt (0, QColor (64, 64, 64));
    gradient.setColorAt (0.5, QColor (32, 32, 32));
    gradient.setColorAt (0.5, QColor (25, 25, 25));
    gradient.setColorAt (1.0, QColor (0, 0, 0));

    setAlignment (Qt::AlignLeft | Qt::AlignTop);
    setScene (m_scene);
    setFixedHeight (InfoBar::Height);
    setCacheMode (QGraphicsView::CacheBackground);

    m_scene->setBackgroundBrush (gradient);
    m_scene->addItem (m_art);
    m_scene->addItem (m_title_text);
    m_scene->addItem (m_album_text);
    m_scene->addItem (m_artist_text);
#ifdef XXX_NOTYET
    m_scene->addItem (m_vis);
#endif

    m_title_text->setDefaultTextColor (QColor (255, 255, 255));
    m_artist_text->setDefaultTextColor (QColor (255, 255, 255));
    m_album_text->setDefaultTextColor (QColor (128, 128, 128));

    QFont f = m_title_text->font ();
    f.setPointSize (18);
    m_title_text->setFont (f);

    f = m_artist_text->font ();
    f.setPointSize (12);
    m_artist_text->setFont (f);

    f = m_album_text->font ();
    f.setPointSize (12);
    m_album_text->setFont (f);
}

QSize InfoBar::minimumSizeHint () const
{
    return QSize (InfoBar::IconSize + (2 * InfoBar::Spacing), InfoBar::Height);
}

void InfoBar::resizeEvent (QResizeEvent * event)
{
    QGraphicsView::resizeEvent (event);
    setSceneRect (0, 0, width (), height ());

    m_art->setPos (m_art->mapFromScene (InfoBar::Spacing, InfoBar::Spacing));

    qreal x = InfoBar::IconSize + (InfoBar::Spacing * 1.5);
    qreal y = InfoBar::Spacing / 2;
    m_title_text->setPos (m_title_text->mapFromScene (x, y));
    m_artist_text->setPos (m_artist_text->mapFromScene (x, y + (InfoBar::IconSize / 2)));
    m_album_text->setPos (m_album_text->mapFromScene (x, y + ((InfoBar::IconSize * 3) / 4)));

#ifdef XXX_NOTYET
    m_vis->setPos (m_vis->mapFromScene ((width () - InfoBar::VisWidth) - (InfoBar::Spacing * 2), 0));
#endif
}

void InfoBar::update_metadata_cb ()
{
    Tuple tuple = aud_drct_get_tuple ();
    String title = tuple.get_str (Tuple::Title);
    String artist = tuple.get_str (Tuple::Artist);
    String album = tuple.get_str (Tuple::Album);

    m_title_text->setPlainText (QString ((const char *) title));
    m_artist_text->setPlainText (QString ((const char *) artist));
    m_album_text->setPlainText (QString ((const char *) album));
}
