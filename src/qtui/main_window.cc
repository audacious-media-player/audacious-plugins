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

#include "info_bar.h"
#include "menus.h"
#include "playlist.h"
#include "playlist_tabs.h"
#include "settings.h"
#include "status_bar.h"
#include "time_slider.h"
#include "tool_bar.h"

#include <QAction>
#include <QBoxLayout>
#include <QCloseEvent>
#include <QDockWidget>
#include <QLabel>
#include <QMenuBar>
#include <QSettings>
#include <QToolButton>

class PluginWidget : public QDockWidget
{
public:
    PluginWidget (PluginHandle * plugin) :
        m_plugin (plugin)
    {
        setObjectName (aud_plugin_get_basename (plugin));
        setWindowTitle (aud_plugin_get_name (plugin));
        setContextMenuPolicy (Qt::PreventContextMenu);
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

static QString get_config_name ()
{
    int instance = aud_get_instance ();
    return (instance == 1) ?
           QString ("audacious") :
           QString ("audacious-%1").arg (instance);
}

static void toggle_search_tool (bool enable)
{
    auto search_tool = aud_plugin_lookup_basename ("search-tool-qt");
    if (search_tool)
        aud_plugin_enable (search_tool, enable);
}

MainWindow::MainWindow () :
    m_config_name (get_config_name ()),
    m_dialogs (this),
    m_menubar (qtui_build_menubar (this)),
    m_playlist_tabs (new PlaylistTabs (this)),
    m_center_widget (new QWidget (this)),
    m_center_layout (audqt::make_vbox (m_center_widget, 0)),
    m_infobar (new InfoBar (this)),
    m_statusbar (new StatusBar (this)),
    m_search_tool (aud_plugin_lookup_basename ("search-tool-qt")),
    m_playlist_manager (aud_plugin_lookup_basename ("playlist-manager-qt"))
{
#if defined(Q_OS_WIN32) || defined(Q_OS_MAC)
    QIcon::setThemeName ("QtUi");

    QStringList paths = QIcon::themeSearchPaths ();
    paths.prepend (aud_get_path (AudPath::DataDir));
    QIcon::setThemeSearchPaths (paths);
#endif

    auto slider = new TimeSlider (this);

    const ToolBarItem items[] = {
        ToolBarAction ("edit-find", N_("Search Library"), N_("Search Library"), toggle_search_tool, & m_search_action),
        ToolBarAction ("document-open", N_("Open Files"), N_("Open Files"),
            [] () { audqt::fileopener_show (audqt::FileMode::Open); }),
        ToolBarAction ("list-add", N_("Add Files"), N_("Add Files"),
            [] () { audqt::fileopener_show (audqt::FileMode::Add); }),
        ToolBarSeparator (),
        ToolBarAction ("media-skip-backward", N_("Previous"), N_("Previous"), aud_drct_pl_prev),
        ToolBarAction ("media-playback-play", N_("Play"), N_("Play"), aud_drct_play_pause, & m_play_pause_action),
        ToolBarAction ("media-playback-stop", N_("Stop"), N_("Stop"), aud_drct_stop, & m_stop_action),
        ToolBarAction ("media-playback-stop", N_("Stop After This Song"), N_("Stop After This Song"),
            [] (bool on) { aud_set_bool (nullptr, "stop_after_current_song", on); }, & m_stop_after_action),
        ToolBarAction ("media-skip-forward", N_("Next"), N_("Next"), aud_drct_pl_next),
        ToolBarAction ("media-record", N_("Record Stream"), N_("Record Stream"),
            [] (bool on) { aud_set_bool (nullptr, "record", on); }, & m_record_action),
        ToolBarSeparator (),
        ToolBarCustom (slider),
        ToolBarCustom (slider->label ()),
        ToolBarSeparator (),
        ToolBarAction ("media-playlist-repeat", N_("Repeat"), N_("Repeat"),
            [] (bool on) { aud_set_bool (nullptr, "repeat", on); }, & m_repeat_action),
        ToolBarAction ("media-playlist-shuffle", N_("Shuffle"), N_("Shuffle"),
            [] (bool on) { aud_set_bool (nullptr, "shuffle", on); }, & m_shuffle_action),
        ToolBarCustom (audqt::volume_button_new (this))
    };

    addToolBar (Qt::TopToolBarArea, new ToolBar (this, items));
    setUnifiedTitleAndToolBarOnMac (true);

    if (m_search_tool)
        aud_plugin_add_watch (m_search_tool, plugin_watcher, this);
    else
        m_search_action->setVisible (false);

    update_toggles ();

    setStatusBar (m_statusbar);
    setCentralWidget (m_center_widget);

    m_center_layout->addWidget (m_playlist_tabs);
    m_center_layout->addWidget (m_infobar);

    setMenuBar (m_menubar);
    setDockNestingEnabled (true);
    add_dock_plugins ();

    if (aud_drct_get_playing ())
    {
        playback_begin_cb ();
        if (aud_drct_get_ready ())
            title_change_cb ();
    }
    else
        playback_stop_cb ();

    read_settings ();
    update_visibility ();
}

MainWindow::~MainWindow ()
{
    QSettings settings (m_config_name, "QtUi");
    settings.setValue ("geometry", saveGeometry ());
    settings.setValue ("windowState", saveState ());

    remove_dock_plugins ();

    if (m_search_tool)
        aud_plugin_remove_watch (m_search_tool, plugin_watcher, this);
}

void MainWindow::closeEvent (QCloseEvent * e)
{
    bool handled = false;

    hook_call ("window close", & handled);

    if (! handled)
        aud_quit ();

    e->ignore ();
}

void MainWindow::keyPressEvent (QKeyEvent * event)
{
    if (event->modifiers () == Qt::NoModifier && event->key () == Qt::Key_Escape)
    {
        auto widget = m_playlist_tabs->currentPlaylistWidget ();

        if (widget->hasFocus ())
            widget->scrollToCurrent (true);
        else
            widget->setFocus (Qt::OtherFocusReason);

        return;
    }

    QMainWindow::keyPressEvent (event);
}

void MainWindow::read_settings ()
{
    QSettings settings (m_config_name, "QtUi");

    if (! restoreGeometry (settings.value ("geometry").toByteArray ()))
        resize (audqt::to_native_dpi (768), audqt::to_native_dpi (480));

    restoreState (settings.value ("windowState").toByteArray ());
}

void MainWindow::set_title (const QString & title)
{
    int instance = aud_get_instance ();
    if (instance == 1)
        QMainWindow::setWindowTitle (title);
    else
        QMainWindow::setWindowTitle (QString ("%1 (%2)").arg (title).arg (instance));
}

void MainWindow::update_toggles ()
{
    if (m_search_tool)
        m_search_action->setChecked (aud_plugin_get_enabled (m_search_tool));

    bool stop_after = aud_get_bool (nullptr, "stop_after_current_song");
    m_stop_action->setVisible (! stop_after);
    m_stop_after_action->setVisible (stop_after);
    m_stop_after_action->setChecked (stop_after);

    m_record_action->setVisible (aud_drct_get_record_enabled ());
    m_record_action->setChecked (aud_get_bool (nullptr, "record"));

    m_repeat_action->setChecked (aud_get_bool (nullptr, "repeat"));
    m_shuffle_action->setChecked (aud_get_bool (nullptr, "shuffle"));
}

void MainWindow::update_visibility ()
{
    m_menubar->setVisible (aud_get_bool ("qtui", "menu_visible"));
    m_infobar->setVisible (aud_get_bool ("qtui", "infoarea_visible"));
    m_statusbar->setVisible (aud_get_bool ("qtui", "statusbar_visible"));
}

void MainWindow::update_play_pause ()
{
    if (! aud_drct_get_playing () || aud_drct_get_paused ())
    {
        m_play_pause_action->setIcon (QIcon::fromTheme ("media-playback-start"));
        m_play_pause_action->setText (_("Play"));
        m_play_pause_action->setToolTip (_("Play"));
    }
    else
    {
        m_play_pause_action->setIcon (QIcon::fromTheme ("media-playback-pause"));
        m_play_pause_action->setText (_("Pause"));
        m_play_pause_action->setToolTip (_("Pause"));
    }
}

void MainWindow::title_change_cb ()
{
    auto title = aud_drct_get_title ();
    if (title)
    {
        set_title (QString (title) + QString (" - Audacious"));
        m_buffering_timer.stop ();
    }
}

void MainWindow::playback_begin_cb ()
{
    update_play_pause ();

    auto last_widget = m_playlist_tabs->playlistWidget (m_last_playing.index ());
    if (last_widget)
        last_widget->updatePlaybackIndicator ();

    auto playing = Playlist::playing_playlist ();
    auto widget = m_playlist_tabs->playlistWidget (playing.index ());
    if (widget)
        widget->scrollToCurrent ();
    if (widget && widget != last_widget)
        widget->updatePlaybackIndicator ();

    m_last_playing = playing;

    m_buffering_timer.queue (250, aud::obj_member<MainWindow, & MainWindow::buffering_cb>, this);
}

void MainWindow::buffering_cb ()
{
    set_title (_("Buffering ..."));
}

void MainWindow::pause_cb ()
{
    update_play_pause ();

    auto widget = m_playlist_tabs->playlistWidget (m_last_playing.index ());
    if (widget)
        widget->updatePlaybackIndicator ();
}

void MainWindow::playback_stop_cb ()
{
    set_title ("Audacious");
    m_buffering_timer.stop ();

    update_play_pause ();

    auto last_widget = m_playlist_tabs->playlistWidget (m_last_playing.index ());
    if (last_widget)
        last_widget->updatePlaybackIndicator ();

    m_last_playing = Playlist ();
}

PluginWidget * MainWindow::find_dock_plugin (PluginHandle * plugin)
{
    for (PluginWidget * w : m_dock_widgets)
    {
        if (w->plugin () == plugin)
            return w;
    }

    return nullptr;
}

void MainWindow::show_dock_plugin (PluginHandle * plugin)
{
    aud_plugin_enable (plugin, true);
    aud_plugin_send_message (plugin, "grab focus", nullptr, 0);
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
        m_dock_widgets.append (w);
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
