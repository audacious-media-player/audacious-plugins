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
#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>
#include <libaudcore/interface.h>
#include <libaudcore/preferences.h>

#include <libaudqt/menu.h>

#include <QMenu>
#include <QSystemTrayIcon>

class StatusIcon : public GeneralPlugin {
public:
    static const char about[];
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;
    static const audqt::MenuItem items[];

    static constexpr PluginInfo info = {
        N_("Status Icon"),
        PACKAGE,
        about,
        & prefs
    };

    constexpr StatusIcon () : GeneralPlugin (info, false) {}

    bool init ();
    void * get_qt_widget ();
    void cleanup ();

    static void window_closed (void * data, void * user_data);
    static void activate (QSystemTrayIcon::ActivationReason);
    static void open_files ();
};

const char StatusIcon::about[] =
 N_("Status Icon Plugin (partial port for Qt interface)\n\n"
    "Copyright 2005-2007 Giacomo Lozito <james@develia.org>\n"
    "Copyright 2010 Michał Lipski <tallica@o2.pl>\n"
    "Copyright 2015 Eugene Paskevich <eugene@raptor.kiev.ua>\n\n"
    "This plugin provides a status icon, placed in\n"
    "the system tray area of the window manager.");

const char * const StatusIcon::defaults[] = {
    "close_to_tray", "FALSE",
    nullptr
};

const PreferencesWidget StatusIcon::widgets[] = {
    WidgetCheck (N_("Close to the system tray"), WidgetBool ("statusicon-qt", "close_to_tray"))
};

const PluginPreferences StatusIcon::prefs = {{widgets}};

const audqt::MenuItem StatusIcon::items[] =
{
    audqt::MenuCommand ({N_("_Open Files ..."), "document-open"}, StatusIcon::open_files),
    audqt::MenuCommand ({N_("Pre_vious"), "media-skip-backward"}, aud_drct_pl_prev),
    audqt::MenuCommand ({N_("_Play"), "media-playback-start"}, aud_drct_play),
    audqt::MenuCommand ({N_("Paus_e"), "media-playback-pause"}, aud_drct_pause),
    audqt::MenuCommand ({N_("_Stop"), "media-playback-stop"}, aud_drct_stop),
    audqt::MenuCommand ({N_("_Next"), "media-skip-forward"}, aud_drct_pl_next),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("Se_ttings ..."), "preferences-system"}, audqt::prefswin_show)
    // Quit command is added automatically
};

bool StatusIcon::init ()
{
    aud_config_set_defaults ("statusicon-qt", defaults);
    return true;
}

static QSystemTrayIcon * tray = nullptr;
static QMenu * menu = nullptr;

void * StatusIcon::get_qt_widget ()
{
    tray = new QSystemTrayIcon ( QIcon( aud_get_path (AudPath::IconFile)));
    QObject::connect (tray, & QSystemTrayIcon::activated, activate);
    menu = audqt::menu_build (items);
    tray->setContextMenu (menu);
    tray->show ();

    hook_associate ("window close", window_closed, nullptr);

    // returning nullptr on purpose
    // because tray icon must not be reparented by Audacious
    return nullptr;
}

void StatusIcon::cleanup ()
{
    hook_dissociate ("window close", window_closed);

    if (! aud_ui_is_shown ())
        aud_ui_show (true);

    delete tray;
    tray = nullptr;
    delete menu;
    menu = nullptr;
}

void StatusIcon::window_closed (void * data, void * user_data)
{
    bool * handled = (bool *) data;

    if (aud_get_bool ("statusicon-qt", "close_to_tray") && tray->isVisible ())
    {
        * handled = true;
        aud_ui_show (false);
    }
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

void StatusIcon::open_files ()
{
    audqt::fileopener_show (audqt::FileMode::Open);
}

EXPORT StatusIcon aud_plugin_instance;
