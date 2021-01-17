/*
 * main_window.cc
 * Copyright 2014 Michał Lipski
 * Copyright 2020 Ariadne Conill
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

#include <QToolButton>
#include <QSplitter>

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugins.h>
#include <libaudcore/runtime.h>

#include <libaudqt/libaudqt.h>

#include "main_window.h"
#include "info_bar.h"
#include "tool_bar.h"
#include "time_slider.h"
#include "playlist.h"
#include "playlist_tabs.h"
#include "playlist_selection.h"

namespace Moonstone {

MainWindow::MainWindow() :
    m_center_widget(new QWidget(this)),
    m_center_layout(audqt::make_vbox(m_center_widget, 0)),
    m_splitter(new QSplitter(this)),
    m_playlist_tabs(new PlaylistTabs),
    m_playlists_view(new PlaylistsView)
{
    resize(1000, 600);

    setCentralWidget(m_center_widget);

    auto slider = new TimeSlider(this);

    const ToolBarItem items[] = {
#if 0
        ToolBarAction("edit-find", N_("Search Library"), N_("Search Library"),
                      toggle_search_tool, &m_search_action),
#endif
        ToolBarAction("document-open", N_("Open Files"), N_("Open Files"),
                      []() { audqt::fileopener_show(audqt::FileMode::Open); }),
        ToolBarAction("list-add", N_("Add Files"), N_("Add Files"),
                      []() { audqt::fileopener_show(audqt::FileMode::Add); }),
        ToolBarSeparator(),
        ToolBarAction("media-skip-backward", N_("Previous"), N_("Previous"),
                      aud_drct_pl_prev),
        ToolBarAction("media-playback-start", N_("Play"), N_("Play"),
                      aud_drct_play_pause, &m_play_pause_action),
        ToolBarAction("media-playback-stop", N_("Stop"), N_("Stop"),
                      aud_drct_stop, &m_stop_action),
        ToolBarAction(
            "media-playback-stop", N_("Stop After This Song"),
            N_("Stop After This Song"),
            [](bool on) { aud_set_bool("stop_after_current_song", on); },
            &m_stop_after_action),
        ToolBarAction("media-skip-forward", N_("Next"), N_("Next"),
                      aud_drct_pl_next),
        ToolBarAction(
            "media-record", N_("Record Stream"), N_("Record Stream"),
            [](bool on) { aud_set_bool("record", on); }, &m_record_action),
        ToolBarSeparator(),
        ToolBarCustom(slider),
        ToolBarCustom(slider->label()),
        ToolBarSeparator(),
        ToolBarAction(
            "media-playlist-repeat", N_("Repeat"), N_("Repeat"),
            [](bool on) { aud_set_bool("repeat", on); }, &m_repeat_action),
        ToolBarAction(
            "media-playlist-shuffle", N_("Shuffle"), N_("Shuffle"),
            [](bool on) { aud_set_bool("shuffle", on); }, &m_shuffle_action),
        ToolBarCustom(audqt::volume_button_new(this))};

    m_toolbar = new ToolBar(this, items);
    m_infobar = new InfoBar(this, m_toolbar);

    m_center_layout->addWidget(m_infobar);
    m_center_layout->addWidget(m_splitter);

    m_splitter->addWidget(m_playlists_view);
    m_splitter->addWidget(m_playlist_tabs);
    m_splitter->setStretchFactor(1, 2);

    update_toggles();
}

void MainWindow::update_toggles()
{
#if 0
    if (m_search_tool)
        m_search_action->setChecked(aud_plugin_get_enabled(m_search_tool));
#endif

    bool stop_after = aud_get_bool("stop_after_current_song");
    m_stop_action->setVisible(!stop_after);
    m_stop_after_action->setVisible(stop_after);
    m_stop_after_action->setChecked(stop_after);

    m_record_action->setVisible(aud_drct_get_record_enabled());
    m_record_action->setChecked(aud_get_bool("record"));

    m_repeat_action->setChecked(aud_get_bool("repeat"));
    m_shuffle_action->setChecked(aud_get_bool("shuffle"));
}

void MainWindow::update_play_pause()
{
    if (!aud_drct_get_playing() || aud_drct_get_paused())
    {
        m_play_pause_action->setIcon(audqt::get_icon("media-playback-start"));
        m_play_pause_action->setText(_("Play"));
        m_play_pause_action->setToolTip(_("Play"));
    }
    else
    {
        m_play_pause_action->setIcon(audqt::get_icon("media-playback-pause"));
        m_play_pause_action->setText(_("Pause"));
        m_play_pause_action->setToolTip(_("Pause"));
    }
}

void MainWindow::title_change_cb()
{
    auto title = aud_drct_get_title();
    if (title)
    {
        set_title(QString(title) + QString(" - Audacious"));
        m_buffering_timer.stop();
    }
}

void MainWindow::playback_begin_cb()
{
    update_play_pause();

    auto last_widget = m_playlist_tabs->playlistWidget(m_last_playing.index());
    if (last_widget)
        last_widget->updatePlaybackIndicator();

    auto playing = Playlist::playing_playlist();

    auto widget = m_playlist_tabs->playlistWidget(playing.index());
    if (widget)
        widget->scrollToCurrent();
    if (widget && widget != last_widget)
        widget->updatePlaybackIndicator();

    m_last_playing = playing;

    m_buffering_timer.queue(250, [this]() { set_title(_("Buffering ...")); });
}

void MainWindow::pause_cb()
{
    update_play_pause();

    auto widget = m_playlist_tabs->playlistWidget(m_last_playing.index());
    if (widget)
        widget->updatePlaybackIndicator();
}

void MainWindow::playback_stop_cb()
{
    set_title("Audacious");
    m_buffering_timer.stop();

    update_play_pause();

    auto last_widget = m_playlist_tabs->playlistWidget(m_last_playing.index());
    if (last_widget)
        last_widget->updatePlaybackIndicator();

    m_last_playing = Playlist();
}

void MainWindow::set_title(const QString & title)
{
    int instance = aud_get_instance();
    if (instance == 1)
        QMainWindow::setWindowTitle(title);
    else
        QMainWindow::setWindowTitle(
            QString("%1 (%2)").arg(title).arg(instance));
}

}
