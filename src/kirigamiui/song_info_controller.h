/*
 * Copyright 2026 Audacious developers and others
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef AUDACIOUS_KIRIGAMI_SONG_INFO_CONTROLLER_H
#define AUDACIOUS_KIRIGAMI_SONG_INFO_CONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariant>

#include <libaudcore/tuple.h>

class PluginHandle;

class MobileSongInfoController : public QObject
{
public:
    using QObject::QObject;

    QVariantMap open(int entry);
    bool save(int session, const QVariantMap & values);
    void close(int session);

private:
    int m_next_session = 0;
    int m_session = 0;
    QString m_filename;
    PluginHandle * m_decoder = nullptr;
    Tuple m_tuple;
    bool m_can_write = false;
};

#endif
