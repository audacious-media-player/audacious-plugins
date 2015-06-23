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
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
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
#include <QSettings>

class PluginWidget : public QDockWidget
{
public:
    PluginWidget (PluginHandle * plugin) :
        m_plugin (plugin)
    {
        setObjectName (aud_plugin_get_basename (plugin));
        setWindowTitle (aud_plugin_get_name (plugin));
    }

    PluginHandle * plugin () const { return m_plugin; }

protected:
    void closeEvent (QCloseEvent * event)
    {
        aud_plugin_enable (m_plugin, false);
        event->ignore ();
    }

private:
    PluginHandle * m_plugin;
};

MainWindow::MainWindow () :
    m_dialogs (this),
    filterInput (new FilterInput (this)),
    playlistTabs (new PlaylistTabs (this)),
    infoBar (new InfoBar (this)),
    centralWidget (new QWidget (this)),
    centralLayout (new QVBoxLayout (centralWidget))
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
        ToolBarAction ("document-open", N_("Open Files"), N_("Open Files"),
            [] () { audqt::fileopener_show (audqt::FileMode::Open); }),
        ToolBarAction ("list-add", N_("Add Files"), N_("Add Files"),
            [] () { audqt::fileopener_show (audqt::FileMode::Add); }),
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
    setCentralWidget (centralWidget);

    centralLayout->addWidget (playlistTabs);
    centralLayout->addWidget (infoBar);

    centralLayout->setContentsMargins (0, 0, 0, 0);
    centralLayout->setSpacing (4);

    connect (filterInput, &QLineEdit::textChanged, playlistTabs, &PlaylistTabs::filterTrigger);

    setupActions ();
    add_dock_plugins ();

    buffering_timer.setSingleShot (true);
    connect (& buffering_timer, & QTimer::timeout, this, & MainWindow::show_buffering);

    if (aud_drct_get_playing ())
    {
        playback_begin_cb ();
        if (aud_drct_get_ready ())
            playback_ready_cb ();
    }
    else
        playback_stop_cb ();

    title_change_cb ();

    readSettings ();
}

MainWindow::~MainWindow ()
{
    QSettings settings ("audacious", "QtUi");
    settings.setValue ("geometry", saveGeometry());
    settings.setValue ("windowState", saveState());

    remove_dock_plugins ();
}

void MainWindow::closeEvent (QCloseEvent * e)
{
    aud_quit ();
    e->ignore ();
}

void MainWindow::readSettings ()
{
    QSettings settings ("audacious", "QtUi");
    restoreGeometry (settings.value ("geometry").toByteArray());
    restoreState (settings.value ("windowState").toByteArray());
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

void MainWindow::update_play_pause ()
{
    if (! aud_drct_get_playing () || aud_drct_get_paused ())
    {
        toolButtonPlayPause->setIcon (QIcon::fromTheme ("media-playback-start"));
        toolButtonPlayPause->setText (_("Play"));
        toolButtonPlayPause->setToolTip (_("Play"));
    }
    else
    {
        toolButtonPlayPause->setIcon (QIcon::fromTheme ("media-playback-pause"));
        toolButtonPlayPause->setText (_("Pause"));
        toolButtonPlayPause->setToolTip (_("Pause"));
    }
}

void MainWindow::show_buffering ()
{
    if (aud_drct_get_playing () && ! aud_drct_get_ready ())
        setWindowTitle (_("Buffering ..."));
}

void MainWindow::title_change_cb ()
{
    auto title = aud_drct_get_title ();
    if (title)
        setWindowTitle (QString (title) + QString (" - Audacious"));
}

void MainWindow::playback_begin_cb ()
{
    update_play_pause ();

    int last_list = aud_playlist_by_unique_id (playing_id);
    auto last_widget = playlistTabs->playlistWidget (last_list);
    if (last_widget)
        last_widget->updatePlaybackIndicator ();

    int list = aud_playlist_get_playing ();
    auto widget = playlistTabs->playlistWidget (list);
    if (widget)
        widget->scrollToCurrent ();
    if (widget && widget != last_widget)
        widget->updatePlaybackIndicator ();

    playing_id = aud_playlist_get_unique_id (list);

    buffering_timer.start (250);
}

void MainWindow::playback_ready_cb ()
{
    title_change_cb ();
}

void MainWindow::pause_cb ()
{
    update_play_pause ();

    int list = aud_playlist_by_unique_id (playing_id);
    auto widget = playlistTabs->playlistWidget (list);
    if (widget)
        widget->updatePlaybackIndicator ();
}

void MainWindow::playback_stop_cb ()
{
    setWindowTitle ("Audacious");
    update_play_pause ();

    int last_list = aud_playlist_by_unique_id (playing_id);
    auto last_widget = playlistTabs->playlistWidget (last_list);
    if (last_widget)
        last_widget->updatePlaybackIndicator ();

    playing_id = -1;
}

void MainWindow::update_toggles_cb ()
{
    updateToggles ();
}

PluginWidget * MainWindow::find_dock_plugin (PluginHandle * plugin)
{
    for (PluginWidget * w : dock_widgets)
    {
        if (w->plugin () == plugin)
            return w;
    }

    return nullptr;
}

void MainWindow::add_dock_plugin_cb (PluginHandle * plugin)
{
    QWidget * widget = (QWidget *) aud_plugin_get_qt_widget (plugin);
    if (! widget)
        return;

    auto w = find_dock_plugin (plugin);
    if (! w)
    {
        w = new PluginWidget (plugin);
        dock_widgets.append (w);
    }

    w->setWidget (widget);

    if (! restoreDockWidget (w))
        addDockWidget (Qt::LeftDockWidgetArea, w);
}

void MainWindow::remove_dock_plugin_cb (PluginHandle * plugin)
{
    if (auto w = find_dock_plugin (plugin))
    {
        removeDockWidget (w);
        delete w->widget ();
    }
}

void MainWindow::add_dock_plugins ()
{
    for (PluginHandle * plugin : aud_plugin_list (PluginType::General))
    {
        if (aud_plugin_get_enabled (plugin))
            add_dock_plugin_cb (plugin);
    }

    for (PluginHandle * plugin : aud_plugin_list (PluginType::Vis))
    {
        if (aud_plugin_get_enabled (plugin))
            add_dock_plugin_cb (plugin);
    }
}

void MainWindow::remove_dock_plugins ()
{
    for (PluginHandle * plugin : aud_plugin_list (PluginType::General))
    {
        if (aud_plugin_get_enabled (plugin))
            remove_dock_plugin_cb (plugin);
    }

    for (PluginHandle * plugin : aud_plugin_list (PluginType::Vis))
    {
        if (aud_plugin_get_enabled (plugin))
            remove_dock_plugin_cb (plugin);
    }
}
