/*
 * Copyright 2026 Audacious developers and others
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef AUDACIOUS_KIRIGAMI_SETTINGS_CONTROLLER_H
#define AUDACIOUS_KIRIGAMI_SETTINGS_CONTROLLER_H

#include <QObject>
#include <QString>

class MobileSettingsController : public QObject
{
public:
    using QObject::QObject;

    bool boolValue(const QString & name) const;
    int intValue(const QString & name) const;
    void setBoolValue(const QString & name, bool value);
    void setIntValue(const QString & name, int value);
};

#endif
