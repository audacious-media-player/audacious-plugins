/*
 * qt-compat.h
 * Copyright 2023 Thomas Lange
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

#ifndef UI_COMMON_QT_COMPAT_H
#define UI_COMMON_QT_COMPAT_H

#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  #define USE_QT6
#endif

namespace QtCompat
{

static inline int x (QEnterEvent * event)
{
#ifdef USE_QT6
    return event->position ().x ();
#else
    return event->x ();
#endif
}

static inline int y (QEnterEvent * event)
{
#ifdef USE_QT6
    return event->position ().y ();
#else
    return event->y ();
#endif
}

static inline int x (QMouseEvent * event)
{
#ifdef USE_QT6
    return event->position ().x ();
#else
    return event->x ();
#endif
}

static inline int y (QMouseEvent * event)
{
#ifdef USE_QT6
    return event->position ().y ();
#else
    return event->y ();
#endif
}

static inline int globalX (QMouseEvent * event)
{
#ifdef USE_QT6
    return event->globalPosition ().x ();
#else
    return event->globalX ();
#endif
}

static inline int globalY (QMouseEvent * event)
{
#ifdef USE_QT6
    return event->globalPosition ().y ();
#else
    return event->globalY ();
#endif
}

static inline QPoint pos (QDragMoveEvent * event)
{
#ifdef USE_QT6
    return event->position ().toPoint ();
#else
    return event->pos ();
#endif
}

static inline QPoint pos (QDropEvent * event)
{
#ifdef USE_QT6
    return event->position ().toPoint ();
#else
    return event->pos ();
#endif
}

} /* namespace QtCompat */

#endif
