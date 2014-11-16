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

#include "info_bar.h"

#include <libaudcore/hook.h>
#include <libaudcore/index.h>
#include <libaudcore/objects.h>
#include <libaudcore/runtime.h>
#include <libaudqt/libaudqt.h>

#include <QGraphicsItem>
#include <QGraphicsPixmapItem>

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
    m_art (new AlbumArtItem)
{
    QLinearGradient gradient (QPointF (0.0, 0.0), QPointF (0.0, 100.0));

    gradient.setColorAt (0, QColor (64, 64, 64));
    gradient.setColorAt (0.5, QColor (32, 32, 32));
    gradient.setColorAt (0.5, QColor (25, 25, 25));
    gradient.setColorAt (1.0, QColor (0, 0, 0));

    setAlignment (Qt::AlignLeft | Qt::AlignTop);
    setScene (m_scene);
    setFixedHeight (InfoBar::Height);

    m_scene->setBackgroundBrush (gradient);
    m_scene->addItem (m_art);
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
}
