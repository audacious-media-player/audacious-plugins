/*
 * Copyright 2026 Audacious developers and others
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "settings_controller.h"

#include <libaudcore/hook.h>
#include <libaudcore/runtime.h>

bool MobileSettingsController::boolValue(const QString & name) const
{
    const QByteArray key = name.toUtf8();
    return aud_get_bool(key.constData());
}

int MobileSettingsController::intValue(const QString & name) const
{
    const QByteArray key = name.toUtf8();
    return aud_get_int(key.constData());
}

void MobileSettingsController::setBoolValue(const QString & name, bool value)
{
    const QByteArray key = name.toUtf8();
    aud_set_bool(key.constData(), value);

    if (name == QStringLiteral("leading_zero") ||
        name == QStringLiteral("show_hours") ||
        name == QStringLiteral("show_numbers_in_pl"))
        hook_call("title change", nullptr);
}

void MobileSettingsController::setIntValue(const QString & name, int value)
{
    const QByteArray key = name.toUtf8();
    aud_set_int(key.constData(), value);
}
