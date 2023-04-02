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
#include <libaudcore/plugins.h>
#include <libaudcore/runtime.h>

#include <libaudqt/libaudqt.h>

#include "info_bar.h"
#include "menus.h"
#include "playlist-qt.h"
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

class DockWidget : public QDockWidget
{
public:
    DockWidget(QWidget * parent, audqt::DockItem * item)
        : QDockWidget(parent), m_item(item)
    {
        setObjectName(item->id());
        setWindowTitle(item->name());
        setWindowRole("plugin");
        setWidget(item->widget());
        setContextMenuPolicy(Qt::PreventContextMenu);

        item->set_host_data(this);
    }

    void destroy()
    {
        if (in_event)
            deleteLater();
        else
            delete this;
    }

protected:
    void closeEvent(QCloseEvent * event) override { close_for_event(event); }

    void keyPressEvent(QKeyEvent * event) override
    {
        auto mods = Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier;
        if (!(event->modifiers() & mods) && event->key() == Qt::Key_Escape &&
            isFloating())
        {
            close_for_event(event);
            return;
        }

        QDockWidget::keyPressEvent(event);
    }

private:
    void close_for_event(QEvent * event)
    {
        in_event = true;
        m_item->user_close();
        in_event = false;
        event->accept();
    }

    audqt::DockItem * m_item;
    bool in_event = false;
};

static QString get_config_name()
{
    int instance = aud_get_instance();
    return (instance == 1) ? QString("audacious")
                           : QString("audacious-%1").arg(instance);
}

static void toggle_search_tool(bool enable)
{
    if (enable)
        hook_call("qtui show search tool", nullptr);
    else
    {
        auto search_tool = aud_plugin_lookup_basename("search-tool-qt");
        if (search_tool)
            aud_plugin_enable(search_tool, false);
    }
}

static QToolButton * create_menu_button(QWidget * parent, QMenuBar * menubar)
{
    auto button = new QToolButton(parent);
    button->setIcon(QIcon::fromTheme("audacious"));
    button->setPopupMode(QToolButton::InstantPopup);
    button->setStyleSheet("QToolButton::menu-indicator { image: none; }");
    button->setToolTip(_("Menu"));

    for (auto action : menubar->actions())
        button->addAction(action);

    return button;
}

MainWindow::MainWindow()
    : m_config_name(get_config_name()), m_dialogs(this),
      m_menubar(qtui_build_menubar(this)),
      m_playlist_tabs(new PlaylistTabs(this)),
      m_center_widget(new QWidget(this)),
      m_center_layout(audqt::make_vbox(m_center_widget, 0)),
      m_infobar(new InfoBar(this)), m_statusbar(new StatusBar(this)),
      m_search_tool(aud_plugin_lookup_basename("search-tool-qt")),
      m_playlist_manager(aud_plugin_lookup_basename("playlist-manager-qt"))
{
    auto slider = new TimeSlider(this);

    const ToolBarItem items[] = {
        ToolBarCustom(create_menu_button(this, m_menubar), &m_menu_action),
        ToolBarAction("edit-find", N_("Search Library"), N_("Search Library"),
                      toggle_search_tool, &m_search_action),
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

    auto toolbar = new ToolBar(this, items);
    addToolBar(Qt::TopToolBarArea, toolbar);

    if (m_search_tool)
        aud_plugin_add_watch(m_search_tool, plugin_watcher, this);
    else
        m_search_action->setVisible(false);

    update_toggles();

    setStatusBar(m_statusbar);
    setCentralWidget(m_center_widget);

    m_center_layout->addWidget(m_playlist_tabs);
    m_center_layout->addWidget(m_infobar);

    setMenuBar(m_menubar);
    setDockNestingEnabled(true);
    setWindowRole("mainwindow");

    audqt::register_dock_host(this);

    if (aud_drct_get_playing())
    {
        playback_begin_cb();
        if (aud_drct_get_ready())
            title_change_cb();
    }
    else
        playback_stop_cb();

    read_settings();
    update_visibility();

    /* Make sure UI elements are visible, in case restoreState() hid
     * them. It's not clear exactly how they can get hidden in the first
     * place, but user screenshots show that it somehow happens, and in
     * that case we don't want them to be gone forever. */
    toolbar->show();
    for (auto w : findChildren<DockWidget *>())
        w->show();

    /* set initial keyboard focus on the playlist */
    m_playlist_tabs->currentPlaylistWidget()->setFocus(Qt::OtherFocusReason);
}

MainWindow::~MainWindow()
{
    QSettings settings(m_config_name, "QtUi");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());

    aud_set_int("qtui", "player_width", audqt::to_portable_dpi(width()));
    aud_set_int("qtui", "player_height", audqt::to_portable_dpi(height()));

    audqt::unregister_dock_host();

    if (m_search_tool)
        aud_plugin_remove_watch(m_search_tool, plugin_watcher, this);
}

void MainWindow::closeEvent(QCloseEvent * e)
{
    bool handled = false;

    hook_call("window close", &handled);

    if (!handled)
    {
        e->accept();
        aud_quit();
    }
    else
        e->ignore();
}

void MainWindow::keyPressEvent(QKeyEvent * event)
{
    auto CtrlShiftAlt =
        Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier;
    if (!(event->modifiers() & CtrlShiftAlt) && event->key() == Qt::Key_Escape)
    {
        auto widget = m_playlist_tabs->currentPlaylistWidget();

        /* on the first press, set focus to the playlist */
        if (!widget->hasFocus())
        {
            widget->setFocus(Qt::OtherFocusReason);
            return;
        }

        /* on the second press, scroll to the current entry */
        if (widget->scrollToCurrent(true))
            return;

        /* on the third press, switch to the playing playlist */
        Playlist::playing_playlist().activate();

        /* ensure currentPlaylistWidget() is up to date */
        Playlist::process_pending_update();
        widget = m_playlist_tabs->currentPlaylistWidget();

        widget->scrollToCurrent(true);
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::read_settings()
{
    QSettings settings(m_config_name, "QtUi");

    if (!restoreGeometry(settings.value("geometry").toByteArray()))
    {
        // QWidget::restoreGeometry() can sometimes fail, e.g. due to
        // https://bugreports.qt.io/browse/QTBUG-86087. Try to at least
        // restore the correct player size in that case.
        resize(audqt::to_native_dpi(aud_get_int("qtui", "player_width")),
               audqt::to_native_dpi(aud_get_int("qtui", "player_height")));
    }

    restoreState(settings.value("windowState").toByteArray());
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

void MainWindow::update_toggles()
{
    if (m_search_tool)
        m_search_action->setChecked(aud_plugin_get_enabled(m_search_tool));

    bool stop_after = aud_get_bool("stop_after_current_song");
    m_stop_action->setVisible(!stop_after);
    m_stop_after_action->setVisible(stop_after);
    m_stop_after_action->setChecked(stop_after);

    m_record_action->setVisible(aud_drct_get_record_enabled());
    m_record_action->setChecked(aud_get_bool("record"));

    m_repeat_action->setChecked(aud_get_bool("repeat"));
    m_shuffle_action->setChecked(aud_get_bool("shuffle"));
}

void MainWindow::update_visibility()
{
    bool menu_visible = aud_get_bool("qtui", "menu_visible");
    m_menubar->setVisible(menu_visible);
    m_menu_action->setVisible(!menu_visible);

    m_infobar->setVisible(aud_get_bool("qtui", "infoarea_visible"));
    m_statusbar->setVisible(aud_get_bool("qtui", "statusbar_visible"));
}

void MainWindow::update_play_pause()
{
    if (!aud_drct_get_playing() || aud_drct_get_paused())
    {
        m_play_pause_action->setIcon(QIcon::fromTheme("media-playback-start"));
        m_play_pause_action->setText(_("Play"));
        m_play_pause_action->setToolTip(_("Play"));
    }
    else
    {
        m_play_pause_action->setIcon(QIcon::fromTheme("media-playback-pause"));
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

void MainWindow::show_dock_plugin(PluginHandle * plugin)
{
    aud_plugin_enable(plugin, true);

    auto item = audqt::DockItem::find_by_plugin(plugin);
    if (item)
        item->grab_focus();
}

void MainWindow::add_dock_item(audqt::DockItem * item)
{
    auto w = new DockWidget(this, item);

    if (!restoreDockWidget(w))
    {
        addDockWidget(Qt::LeftDockWidgetArea, w);
        // only the search tool is docked by default
        if (strcmp(item->id(), "search-tool-qt"))
            w->setFloating(true);
    }

    /* workaround for QTBUG-89144 */
    auto flags = w->windowFlags();
    if (flags & Qt::X11BypassWindowManagerHint)
        w->setWindowFlags(flags & ~Qt::X11BypassWindowManagerHint);

    w->show(); /* in case restoreDockWidget() hid it */
}

void MainWindow::focus_dock_item(audqt::DockItem * item)
{
    auto w = (DockWidget *)item->host_data();
    if (w->isFloating())
        w->activateWindow();
}

void MainWindow::remove_dock_item(audqt::DockItem * item)
{
    auto w = (DockWidget *)item->host_data();
    w->destroy();
}
