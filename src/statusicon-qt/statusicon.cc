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

#include <libaudcore/i18n.h>
#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/mainloop.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>
#include <libaudcore/interface.h>
#include <libaudcore/preferences.h>

#include <libaudqt/menu.h>

#include <QApplication>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QWheelEvent>

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
        & prefs,
        PluginQtOnly
    };

    constexpr StatusIcon () : GeneralPlugin (info, false) {}

    bool init ();
    void cleanup ();

    static void window_closed (void * data, void * user_data);
    static void activate (QSystemTrayIcon::ActivationReason);
    static void open_files ();
    static void toggle_aud_ui ();
};

EXPORT StatusIcon aud_plugin_instance;

const char StatusIcon::about[] =
 N_("Status Icon Plugin (partial port for Qt interface)\n\n"
    "Copyright 2005-2007 Giacomo Lozito <james@develia.org>\n"
    "Copyright 2010 Michał Lipski <tallica@o2.pl>\n"
    "Copyright 2015 Eugene Paskevich <eugene@raptor.kiev.ua>\n\n"
    "This plugin provides a status icon, placed in\n"
    "the system tray area of the window manager.");

enum {
    SI_CFG_SCROLL_ACTION_VOLUME,
    SI_CFG_SCROLL_ACTION_SKIP
};

const char * const StatusIcon::defaults[] = {
    "scroll_action", aud::numeric_string<SI_CFG_SCROLL_ACTION_VOLUME>::str,
    "volume_delta", "5",
    "disable_popup", "FALSE",
    "close_to_tray", "FALSE",
    "reverse_scroll", "FALSE",
    nullptr
};

const PreferencesWidget StatusIcon::widgets[] = {
    WidgetLabel (N_("<b>Mouse Scroll Action</b>")),
    WidgetRadio (N_("Change volume"),
        WidgetInt ("statusicon", "scroll_action"),
        {SI_CFG_SCROLL_ACTION_VOLUME}),
    WidgetRadio (N_("Change playing song"),
        WidgetInt ("statusicon", "scroll_action"),
        {SI_CFG_SCROLL_ACTION_SKIP}),
    WidgetLabel (N_("<b>Other Settings</b>")),
    WidgetCheck (N_("Disable the popup window"),
        WidgetBool ("statusicon", "disable_popup")),
    WidgetCheck (N_("Close to the system tray"),
        WidgetBool ("statusicon", "close_to_tray")),
    WidgetCheck (N_("Advance in playlist when scrolling upward"),
        WidgetBool ("statusicon", "reverse_scroll"))
};

const PluginPreferences StatusIcon::prefs = {{widgets}};

const audqt::MenuItem StatusIcon::items[] =
{
    audqt::MenuCommand ({N_("_Play"), "media-playback-start"}, aud_drct_play),
    audqt::MenuCommand ({N_("Paus_e"), "media-playback-pause"}, aud_drct_pause),
    audqt::MenuCommand ({N_("_Stop"), "media-playback-stop"}, aud_drct_stop),
    audqt::MenuCommand ({N_("Pre_vious"), "media-skip-backward"}, aud_drct_pl_prev),
    audqt::MenuCommand ({N_("_Next"), "media-skip-forward"}, aud_drct_pl_next),
    audqt::MenuSep (),
    audqt::MenuCommand ({N_("_Open Files ..."), "document-open"}, StatusIcon::open_files),
    audqt::MenuCommand ({N_("Se_ttings ..."), "preferences-system"}, audqt::prefswin_show),
    audqt::MenuCommand ({N_("_Quit"), "application-exit"}, aud_quit),
};

class SystemTrayIcon : public QSystemTrayIcon
{
public:
    SystemTrayIcon (const QIcon & icon, QObject * parent = nullptr) :
        QSystemTrayIcon (icon, parent) {}
    ~SystemTrayIcon ()
        { hide_popup (); }

    void show_popup ();
    void hide_popup ();

protected:
    void scroll (int steps);
    bool event (QEvent * e) override;

private:
    int scroll_delta = 0;
    bool popup_shown = false;
    QueuedFunc popup_timer;
};

void SystemTrayIcon::scroll (int delta)
{
    scroll_delta += delta;

    /* we want discrete steps here */
    int steps = scroll_delta / 120;
    if (steps == 0)
        return;

    scroll_delta -= 120 * steps;

    switch (aud_get_int ("statusicon", "scroll_action"))
    {
    case SI_CFG_SCROLL_ACTION_VOLUME:
        aud_drct_set_volume_main (aud_drct_get_volume_main () +
         aud_get_int ("statusicon", "volume_delta") * steps);
        break;

    case SI_CFG_SCROLL_ACTION_SKIP:
        if ((steps > 0) ^ aud_get_bool ("statusicon", "reverse_scroll"))
            aud_drct_pl_prev ();
        else
            aud_drct_pl_next ();
        break;
    }
}

bool SystemTrayIcon::event (QEvent * e)
{
    switch (e->type ())
    {
    case QEvent::ToolTip:
        if (! aud_get_bool ("statusicon", "disable_popup"))
            show_popup ();
        return true;

    case QEvent::Wheel:
        scroll (((QWheelEvent *) e)->angleDelta ().y ());
        return true;

    default:
        return QSystemTrayIcon::event (e);
    }
}

void SystemTrayIcon::show_popup ()
{
    audqt::infopopup_show_current ();
    popup_timer.queue (5000, aud::obj_member<SystemTrayIcon, & SystemTrayIcon::hide_popup>, this);
    popup_shown = true;
}

void SystemTrayIcon::hide_popup ()
{
    if (popup_shown)
    {
        audqt::infopopup_hide ();
        popup_shown = false;
    }
}

static SystemTrayIcon * tray = nullptr;
static QMenu * menu = nullptr;

bool StatusIcon::init ()
{
    aud_config_set_defaults ("statusicon", defaults);

    audqt::init ();

    tray = new SystemTrayIcon (qApp->windowIcon ());
    QObject::connect (tray, & QSystemTrayIcon::activated, activate);
    menu = audqt::menu_build (items);
    QObject::connect (menu, & QMenu::aboutToShow, tray, & SystemTrayIcon::hide_popup);
    tray->setContextMenu (menu);
    tray->show ();

    hook_associate ("window close", window_closed, nullptr);

    return true;
}

void StatusIcon::cleanup ()
{
    hook_dissociate ("window close", window_closed);

    /* Prevent accidentally hiding the interface by disabling
     * the plugin while Audacious is closed to the tray. */
    PluginHandle * p = aud_plugin_by_header (& aud_plugin_instance);
    if (! aud_plugin_get_enabled (p) && ! aud_get_headless_mode () && ! aud_ui_is_shown ())
        aud_ui_show (true);

    delete tray;
    tray = nullptr;
    delete menu;
    menu = nullptr;

    audqt::cleanup ();
}

void StatusIcon::window_closed (void * data, void * user_data)
{
    bool * handled = (bool *) data;

    if (aud_get_bool ("statusicon", "close_to_tray") && tray->isVisible ())
    {
        * handled = true;
        aud_ui_show (false);
    }
}

void StatusIcon::activate(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason)
    {
#ifndef Q_OS_MAC
        case QSystemTrayIcon::Trigger:
            toggle_aud_ui ();
            break;
#endif

        case QSystemTrayIcon::MiddleClick:
            aud_drct_pause ();
            break;

        default:
            break;
    }
}

void StatusIcon::open_files ()
{
    audqt::fileopener_show (audqt::FileMode::Open);
}

void StatusIcon::toggle_aud_ui ()
{
    aud_ui_show (! aud_ui_is_shown ());
}
