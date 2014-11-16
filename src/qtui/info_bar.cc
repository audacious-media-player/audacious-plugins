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

InfoBar::InfoBar (QWidget * parent) : QGraphicsView (parent),
    m_scene (new QGraphicsScene (this))
{
    QLinearGradient gradient (QPointF (0.0, 0.0), QPointF (0.0, 100.0));

    gradient.setColorAt (0, QColor (64, 64, 64));
    gradient.setColorAt (0.5, QColor (32, 32, 32));
    gradient.setColorAt (0.5, QColor (25, 25, 25));
    gradient.setColorAt (1.0, QColor (0, 0, 0));

    m_scene->setBackgroundBrush (gradient);

    setScene (m_scene);
    setFixedHeight (InfoBar::Height);
}
