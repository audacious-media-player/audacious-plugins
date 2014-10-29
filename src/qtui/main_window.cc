/*
 * main_window.cc
 * Copyright 2014 Michał Lipski
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

#include "main_window.h"

#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>

#include <libaudqt/libaudqt.h>
#include <libaudqt/volumebutton.h>

#include "filter_input.h"
#include "playlist.h"
#include "time_slider.h"
#include "status_bar.h"
#include "playlist_tabs.h"
#include "tool_bar.h"

#include <QApplication>
#include <QDockWidget>
#include <QAction>

MainWindow::MainWindow () :
    m_dialogs (this),
    filterInput (new FilterInput (this)),
    playlistTabs (new PlaylistTabs (this))
{
#if defined(Q_OS_WIN32) || defined(Q_OS_MAC)
    QIcon::setThemeName ("QtUi");

    QStringList paths = QIcon::themeSearchPaths ();
    paths.prepend (aud_get_path (AudPath::DataDir));
    QIcon::setThemeSearchPaths (paths);
#else
    QApplication::setWindowIcon (QIcon::fromTheme ("audacious"));
#endif

    auto slider = new TimeSlider (this);

    const ToolBarItem items[] = {
        ToolBarAction ("document-open", N_("Open Files"), N_("Open Files"), [] () { audqt::fileopener_show(false); }),
        ToolBarAction ("list-add", N_("Add Files"), N_("Add Files"), [] () { audqt::fileopener_show(true); }),
        ToolBarSeparator (),
        ToolBarAction ("media-playback-play", N_("Play"), N_("Play"), aud_drct_play_pause, & toolButtonPlayPause),
        ToolBarAction ("media-playback-stop", N_("Stop"), N_("Stop"), aud_drct_stop),
        ToolBarAction ("media-skip-backward", N_("Previous"), N_("Previous"), aud_drct_pl_prev),
        ToolBarAction ("media-skip-forward", N_("Next"), N_("Next"), aud_drct_pl_next),
        ToolBarSeparator (),
        ToolBarCustom (slider),
        ToolBarCustom (slider->label ()),
        ToolBarSeparator (),
        ToolBarAction ("media-playlist-repeat", N_("Repeat"), N_("Repeat"),
            [] (bool on) { aud_set_bool (nullptr, "repeat", on); }, & toolButtonRepeat),
        ToolBarAction ("media-playlist-shuffle", N_("Shuffle"), N_("Shuffle"),
            [] (bool on) { aud_set_bool (nullptr, "shuffle", on); }, & toolButtonShuffle),
        ToolBarCustom (new audqt::VolumeButton (this)),
        ToolBarCustom (filterInput),
    };

    addToolBar (Qt::TopToolBarArea, new ToolBar (this, items));

    setUnifiedTitleAndToolBarOnMac (true);

    updateToggles ();

    setStatusBar (new StatusBar (this));
    setCentralWidget (playlistTabs);

    connect (filterInput, &QLineEdit::textChanged, playlistTabs, &PlaylistTabs::filterTrigger);

    setupActions ();

    hook_associate ("title change",     (HookFunction) title_change_cb, this);
    hook_associate ("playback begin",   (HookFunction) playback_begin_cb, this);
    hook_associate ("playback ready",   (HookFunction) playback_ready_cb, this);
    hook_associate ("playback pause",   (HookFunction) pause_cb, this);
    hook_associate ("playback unpause", (HookFunction) pause_cb, this);
    hook_associate ("playback stop",    (HookFunction) playback_stop_cb, this);

    hook_associate ("set repeat",                  (HookFunction) update_toggles_cb, this);
    hook_associate ("set shuffle",                 (HookFunction) update_toggles_cb, this);
    hook_associate ("set no_playlist_advance",     (HookFunction) update_toggles_cb, this);
    hook_associate ("set stop_after_current_song", (HookFunction) update_toggles_cb, this);

    add_dock_plugins ();

    buffering_timer.setSingleShot (true);
    connect (& buffering_timer, & QTimer::timeout, this, & MainWindow::show_buffering);

    if (aud_drct_get_playing ())
    {
        playback_begin_cb (nullptr, this);
        if (aud_drct_get_ready ())
            playback_ready_cb (nullptr, this);
    }
    else
        playback_stop_cb (nullptr, this);

    title_change_cb (nullptr, this);
}

MainWindow::~MainWindow ()
{
    hook_dissociate ("title change",     (HookFunction) title_change_cb);
    hook_dissociate ("playback begin",   (HookFunction) playback_begin_cb);
    hook_dissociate ("playback ready",   (HookFunction) playback_ready_cb);
    hook_dissociate ("playback pause",   (HookFunction) pause_cb);
    hook_dissociate ("playback unpause", (HookFunction) pause_cb);
    hook_dissociate ("playback stop",    (HookFunction) playback_stop_cb);

    hook_dissociate ("set repeat",                  (HookFunction) update_toggles_cb);
    hook_dissociate ("set shuffle",                 (HookFunction) update_toggles_cb);
    hook_dissociate ("set no_playlist_advance",     (HookFunction) update_toggles_cb);
    hook_dissociate ("set stop_after_current_song", (HookFunction) update_toggles_cb);

    remove_dock_plugins ();
}

void MainWindow::closeEvent (QCloseEvent * e)
{
    aud_quit ();
    e->ignore ();
}

void MainWindow::keyPressEvent (QKeyEvent * e)
{
    switch (e->modifiers ())
    {
    case Qt::ControlModifier:
        switch (e->key ())
        {
        case Qt::Key_F:
            filterInput->setFocus ();
            break;
        }
        break;
    }

    QMainWindow::keyPressEvent (e);
}

void MainWindow::updateToggles ()
{
    toolButtonRepeat->setChecked (aud_get_bool (nullptr, "repeat"));
    toolButtonShuffle->setChecked (aud_get_bool (nullptr, "shuffle"));
}

void MainWindow::action_play_pause_set_play ()
{
    toolButtonPlayPause->setIcon (QIcon::fromTheme ("media-playback-start"));
    toolButtonPlayPause->setText ("Play");
}

void MainWindow::action_play_pause_set_pause ()
{
    toolButtonPlayPause->setIcon (QIcon::fromTheme ("media-playback-pause"));
    toolButtonPlayPause->setText ("Pause");
}

void MainWindow::show_buffering ()
{
    if (aud_drct_get_playing () && ! aud_drct_get_ready ())
        setWindowTitle (_("Buffering ..."));
}

void MainWindow::title_change_cb (void *, MainWindow * window)
{
    auto title = aud_drct_get_title ();
    if (title)
        window->setWindowTitle (QString (title) + QString (" - Audacious"));
}

void MainWindow::playback_begin_cb (void *, MainWindow * window)
{
    pause_cb (nullptr, window);
    window->buffering_timer.start (250);
}

void MainWindow::playback_ready_cb (void *, MainWindow * window)
{
    title_change_cb (nullptr, window);
    pause_cb (nullptr, window);
}

void MainWindow::pause_cb (void *, MainWindow * window)
{
    if (aud_drct_get_paused ())
        window->action_play_pause_set_play ();
    else
        window->action_play_pause_set_pause ();

    window->playlistTabs->activePlaylistWidget ()->positionUpdate (); /* updates indicator icon */
}

void MainWindow::playback_stop_cb (void *, MainWindow * window)
{
    window->setWindowTitle ("Audacious");
    window->action_play_pause_set_play ();
    window->playlistTabs->activePlaylistWidget ()->positionUpdate (); /* updates indicator icon */
}

void MainWindow::update_toggles_cb (void *, MainWindow * window)
{
    window->updateToggles ();
}

struct DockWidget {
    QDockWidget * w;
    PluginHandle * pl;
};

void MainWindow::add_dock_plugin_cb (PluginHandle * plugin, MainWindow * window)
{
    QWidget * widget = (QWidget *) aud_plugin_get_qt_widget (plugin);

    if (widget)
    {
        widget->resize (320, 240);

        DockWidget * dw = new DockWidget;

        dw->w = new QDockWidget;
        dw->w->setWindowTitle (aud_plugin_get_name (plugin));
        dw->w->setWidget (widget);
        dw->pl = plugin;

        window->addDockWidget (Qt::LeftDockWidgetArea, dw->w);

        window->dock_widgets.append (dw);
    }
}

void MainWindow::remove_dock_plugin_cb (PluginHandle * plugin, MainWindow * window)
{
    for (auto dw : window->dock_widgets)
    {
        if (dw->pl == plugin)
        {
            int pos = window->dock_widgets.find (dw);

            window->dock_widgets.remove (pos, 1);

            window->removeDockWidget (dw->w);
            delete dw->w;

            delete dw;
        }
    }
}

void MainWindow::add_dock_plugins ()
{
    for (PluginHandle * plugin : aud_plugin_list (PLUGIN_TYPE_GENERAL))
    {
        if (aud_plugin_get_enabled (plugin))
            add_dock_plugin_cb (plugin, this);
    }

    for (PluginHandle * plugin : aud_plugin_list (PLUGIN_TYPE_VIS))
    {
        if (aud_plugin_get_enabled (plugin))
            add_dock_plugin_cb (plugin, this);
    }

    hook_associate ("dock plugin enabled",  (HookFunction) add_dock_plugin_cb, this);
    hook_associate ("dock plugin disabled", (HookFunction) remove_dock_plugin_cb, this);
}

void MainWindow::remove_dock_plugins ()
{
    for (PluginHandle * plugin : aud_plugin_list (PLUGIN_TYPE_GENERAL))
    {
        if (aud_plugin_get_enabled (plugin))
            remove_dock_plugin_cb (plugin, this);
    }

    for (PluginHandle * plugin : aud_plugin_list (PLUGIN_TYPE_VIS))
    {
        if (aud_plugin_get_enabled (plugin))
            remove_dock_plugin_cb (plugin, this);
    }

    hook_dissociate ("dock plugin enabled",  (HookFunction) add_dock_plugin_cb);
    hook_dissociate ("dock plugin disabled", (HookFunction) remove_dock_plugin_cb);
}
