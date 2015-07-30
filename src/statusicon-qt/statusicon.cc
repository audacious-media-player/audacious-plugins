/*
 * statusicon.cc
 * Copyright 2015 Eugene Paskevich
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

#define AUD_PLUGIN_QT_ONLY
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>
#include <libaudcore/interface.h>

#include <QSystemTrayIcon>

class StatusIcon : public GeneralPlugin {
public:
    static constexpr PluginInfo info = {
        N_("Status Icon"),
        PACKAGE
    };

    constexpr StatusIcon () : GeneralPlugin (info, false) {}

    void cleanup ();

    void * get_qt_widget ();

    static void activate (QSystemTrayIcon::ActivationReason);
};

static QSystemTrayIcon * tray = nullptr;

void * StatusIcon::get_qt_widget()
{
    tray = new QSystemTrayIcon ( QIcon( aud_get_path (AudPath::IconFile)));
    QObject::connect (tray, & QSystemTrayIcon::activated, activate);
    // TODO: find out how to get Playback submenu.
    //tray->setContextMenu (audqt::menu_get_by_id (AudMenuID::count));
    tray->show ();

    // returning nullptr on purpose
    // because tray icon must not be reparented by Audacious
    return nullptr;
}

void StatusIcon::cleanup ()
{
    delete tray;
    tray = nullptr;
}

void StatusIcon::activate(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason)
    {
        case QSystemTrayIcon::Trigger:
            aud_ui_show (! aud_ui_is_shown ());
            break;

        default:
            break;
    }
}

EXPORT StatusIcon aud_plugin_instance;
