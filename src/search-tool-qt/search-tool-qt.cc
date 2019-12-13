/*
 * search-tool-qt.cc
 * Copyright 2011-2019 John Lindgren, Ariadne Conill, and René J.V. Bertin
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

#include <glib.h>

#include <QBoxLayout>
#include <QContextMenuEvent>
#include <QDirIterator>
#include <QFileSystemWatcher>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QTreeView>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/mainloop.h>
#include <libaudcore/runtime.h>
#include <libaudqt/libaudqt.h>

#include "html-delegate.h"
#include "library.h"
#include "search-model.h"

#define CFG_ID "search-tool"
#define SEARCH_DELAY 300

class SearchToolQt : public GeneralPlugin
{
public:
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("Search Tool"),
        PACKAGE,
        nullptr, // about
        & prefs,
        PluginQtOnly
    };

    constexpr SearchToolQt () : GeneralPlugin (info, false) {}

    bool init ();
    void * get_qt_widget ();
    int take_message (const char * code, const void *, int);
};

EXPORT SearchToolQt aud_plugin_instance;

class SearchEntry : public QLineEdit
{
public:
    QTreeView * list = nullptr;

protected:
    void keyPressEvent (QKeyEvent * event)
    {
        auto CtrlShiftAlt = Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier;
        if (list && ! (event->modifiers () & CtrlShiftAlt) && event->key () == Qt::Key_Down)
        {
            list->setCurrentIndex (list->model ()->index (0, 0));
            list->setFocus (Qt::OtherFocusReason);
            return;
        }
        QLineEdit::keyPressEvent (event);
    }
};

class ResultsList : public QTreeView
{
public:
    QWidget * entry = nullptr;

protected:
    void keyPressEvent (QKeyEvent * event)
    {
        auto CtrlShiftAlt = Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier;
        if (entry && ! (event->modifiers () & CtrlShiftAlt) &&
            event->key () == Qt::Key_Up && currentIndex ().row () == 0)
        {
            entry->setFocus (Qt::OtherFocusReason);
            return;
        }
        QTreeView::keyPressEvent (event);
    }
};

class SearchWidget : public QWidget
{
public:
    SearchWidget ();

    void grab_focus () { m_search_entry.setFocus (Qt::OtherFocusReason); }

    void trigger_search ();
    void reset_monitor ();

private:
    void init_library ();
    void show_hide_widgets ();
    void search_timeout ();
    void library_updated ();
    void location_changed ();
    void walk_library_paths ();
    void setup_monitor ();

    void do_add (bool play, bool set_title);
    void action_play ();
    void action_create_playlist ();
    void action_add_to_playlist ();
    void show_context_menu (const QPoint & global_pos);

    Library m_library;
    SearchModel m_model;
    HtmlDelegate m_delegate;

    SmartPtr<QFileSystemWatcher> m_watcher;
    QStringList m_watcher_paths;

    QueuedFunc m_search_timer;
    bool m_search_pending = false;

    QLabel m_help_label, m_wait_label, m_stats_label;
    SearchEntry m_search_entry;
    ResultsList m_results_list;
    QPushButton m_refresh_btn;

    QLineEdit * m_file_entry;
};

static QPointer<SearchWidget> s_widget;

const char * const SearchToolQt::defaults[] = {
    "max_results", "20",
    "rescan_on_startup", "FALSE",
    "monitor", "FALSE",
    nullptr
};

const PreferencesWidget SearchToolQt::widgets[] = {
    WidgetSpin (N_("Number of results to show:"),
        WidgetInt (CFG_ID, "max_results", [] () { s_widget->trigger_search (); }),
         {10, 10000, 10}),
    WidgetCheck (N_("Rescan library at startup"),
        WidgetBool (CFG_ID, "rescan_on_startup")),
    WidgetCheck (N_("Monitor library for changes"),
        WidgetBool (CFG_ID, "monitor", [] () { s_widget->reset_monitor (); }))
};

const PluginPreferences SearchToolQt::prefs = {{widgets}};

static String get_uri ()
{
    auto to_uri = [] (const char * path)
        { return String (filename_to_uri (path)); };

    String path1 = aud_get_str (CFG_ID, "path");
    if (path1[0])
        return strstr (path1, "://") ? path1 : to_uri (path1);

    StringBuf path2 = filename_build ({g_get_home_dir (), "Music"});
    if (g_file_test (path2, G_FILE_TEST_EXISTS))
        return to_uri (path2);

    return to_uri (g_get_home_dir ());
}

SearchWidget::SearchWidget () :
    m_help_label (_("To import your music library into Audacious, "
     "choose a folder and then click the \"refresh\" icon.")),
    m_wait_label (_("Please wait ...")),
    m_refresh_btn (audqt::get_icon ("view-refresh"), QString ()),
    m_file_entry (audqt::file_entry_new (this, _("Choose Folder"),
     QFileDialog::Directory, QFileDialog::AcceptOpen))
{
    m_search_entry.setClearButtonEnabled (true);
    m_search_entry.setPlaceholderText (_("Search library"));

    m_help_label.setAlignment (Qt::AlignCenter);
    m_help_label.setContentsMargins (audqt::margins.EightPt);
    m_help_label.setWordWrap (true);

    m_wait_label.setAlignment (Qt::AlignCenter);
    m_wait_label.setContentsMargins (audqt::margins.EightPt);

    m_results_list.setFrameStyle (QFrame::NoFrame);
    m_results_list.setHeaderHidden (true);
    m_results_list.setIndentation (0);
    m_results_list.setModel (& m_model);
    m_results_list.setItemDelegate (& m_delegate);
    m_results_list.setSelectionMode (QTreeView::ExtendedSelection);
    m_results_list.setDragDropMode (QTreeView::DragOnly);
    m_results_list.setContextMenuPolicy (Qt::CustomContextMenu);

    m_stats_label.setAlignment (Qt::AlignCenter);
    m_stats_label.setContentsMargins (audqt::margins.TwoPt);

#ifdef Q_OS_MAC  // Mac-specific font tweaks
    m_search_entry.setFont (QApplication::font ("QTreeView"));
    m_stats_label.setFont (QApplication::font ("QSmallFont"));
#endif

    m_refresh_btn.setFlat (true);
    m_refresh_btn.setFocusPolicy (Qt::NoFocus);

    m_search_entry.list = & m_results_list;
    m_results_list.entry = & m_search_entry;

    auto hbox1 = audqt::make_hbox (nullptr);
    hbox1->setContentsMargins (audqt::margins.TwoPt);
    hbox1->addWidget (& m_search_entry);

    auto hbox2 = audqt::make_hbox (nullptr);
    hbox2->setContentsMargins (audqt::margins.TwoPt);
    hbox2->addWidget (m_file_entry);
    hbox2->addWidget (& m_refresh_btn);

    auto vbox = audqt::make_vbox (this, 0);
    vbox->addLayout (hbox1);
    vbox->addWidget (& m_help_label);
    vbox->addWidget (& m_wait_label);
    vbox->addWidget (& m_results_list);
    vbox->addWidget (& m_stats_label);
    vbox->addLayout (hbox2);

    audqt::file_entry_set_uri (m_file_entry, get_uri ());

    init_library ();
    reset_monitor ();

    QObject::connect (& m_search_entry, & QLineEdit::textEdited, this, & SearchWidget::trigger_search);
    QObject::connect (& m_search_entry, & QLineEdit::returnPressed, this, & SearchWidget::action_play);
    QObject::connect (& m_results_list, & QTreeView::activated, this, & SearchWidget::action_play);

    QObject::connect (& m_results_list, & QWidget::customContextMenuRequested,
     [this] (const QPoint & pos) { show_context_menu (m_results_list.mapToGlobal (pos)); });

    QObject::connect (m_file_entry, & QLineEdit::textChanged, [this] (const QString & text)
        { m_refresh_btn.setDisabled (text.isEmpty ()); });

    QObject::connect (m_file_entry, & QLineEdit::returnPressed, this, & SearchWidget::location_changed);
    QObject::connect (& m_refresh_btn, & QPushButton::clicked, this, & SearchWidget::location_changed);
}

void SearchWidget::init_library ()
{
    m_library.connect_update
     (aud::obj_member<SearchWidget, & SearchWidget::library_updated>, this);

    if (aud_get_bool (CFG_ID, "rescan_on_startup"))
        m_library.begin_add (get_uri ());

    m_library.check_ready_and_update (true);
}

void SearchWidget::show_hide_widgets ()
{
    if (m_library.playlist () == Playlist ())
    {
        m_wait_label.hide ();
        m_results_list.hide ();
        m_stats_label.hide ();
        m_help_label.show ();
    }
    else
    {
        m_help_label.hide ();

        if (m_library.is_ready ())
        {
            m_wait_label.hide ();
            m_results_list.show ();
            m_stats_label.show ();
        }
        else
        {
            m_results_list.hide ();
            m_stats_label.hide ();
            m_wait_label.show ();
        }
    }
}

void SearchWidget::search_timeout ()
{
    auto text = m_search_entry.text ().toUtf8 ();
    auto terms = str_list_to_index (str_tolower_utf8 (text), " ");
    m_model.do_search (terms, aud_get_int (CFG_ID, "max_results"));
    m_model.update ();

    int shown = m_model.num_items ();
    int hidden = m_model.num_hidden_items ();
    int total = shown + hidden;

    if (shown)
    {
        auto sel = m_results_list.selectionModel ();
        sel->select (m_model.index (0, 0), sel->Clear | sel->SelectCurrent);
    }

    if (hidden)
        m_stats_label.setText ((const char *)
         str_printf (dngettext (PACKAGE, "%d of %d result shown",
         "%d of %d results shown", total), shown, total));
    else
        m_stats_label.setText ((const char *)
         str_printf (dngettext (PACKAGE, "%d result", "%d results", total), total));

    m_search_timer.stop ();
    m_search_pending = false;
}

void SearchWidget::trigger_search ()
{
    m_search_timer.queue (SEARCH_DELAY,
     aud::obj_member<SearchWidget, & SearchWidget::search_timeout>, this);
    m_search_pending = true;
}

void SearchWidget::library_updated ()
{
    if (m_library.is_ready ())
    {
        m_model.create_database (m_library.playlist ());
        search_timeout ();
    }
    else
    {
        m_model.destroy_database ();
        m_model.update ();
        m_stats_label.clear ();
    }

    show_hide_widgets ();
}

void SearchWidget::location_changed ()
{
    auto uri = audqt::file_entry_get_uri (m_file_entry);
    if (! uri)
        return;

    audqt::file_entry_set_uri (m_file_entry, uri); // normalize path

    // if possible, store local path for compatibility with older versions
    StringBuf path = uri_to_filename (uri);
    aud_set_str (CFG_ID, "path", path ? path : uri);

    m_library.begin_add (uri);
    m_library.check_ready_and_update (true);
    reset_monitor ();
}

// QFileSystemWatcher doesn't support recursion, so we must do it ourselves.
// TODO: Since MacOS has an abysmally low default per-process FD limit, this
// means it probably won't work on MacOS with a huge media library.
// In the case of MacOS, we should use the FSEvents API instead.
void SearchWidget::walk_library_paths ()
{
    if (! m_watcher_paths.isEmpty ())
        m_watcher->removePaths (m_watcher_paths);

    m_watcher_paths.clear ();

    auto root = (QString) uri_to_filename (get_uri ());
    if (root.isEmpty ())
        return;

    m_watcher_paths.append (root);

    QDirIterator it (root, QDir::Dirs | QDir::NoDot | QDir::NoDotDot, QDirIterator::Subdirectories);
    while (it.hasNext ())
        m_watcher_paths.append (it.next ());

    m_watcher->addPaths (m_watcher_paths);
}

void SearchWidget::setup_monitor ()
{
    AUDINFO ("Starting monitoring.\n");
    m_watcher.capture (new QFileSystemWatcher);
    m_watcher_paths.clear ();

    QObject::connect (m_watcher.get (), & QFileSystemWatcher::directoryChanged,
     [this] (const QString &)
    {
        AUDINFO ("Library directory changed, refreshing library.\n");

        m_library.begin_add (get_uri ());
        m_library.check_ready_and_update (true);

        walk_library_paths ();
    });

    walk_library_paths ();
}

void SearchWidget::reset_monitor ()
{
    if (aud_get_bool (CFG_ID, "monitor"))
    {
        setup_monitor ();
    }
    else if (m_watcher)
    {
        AUDINFO ("Stopping monitoring.\n");
        m_watcher.clear ();
        m_watcher_paths.clear ();
    }
}

void SearchWidget::do_add (bool play, bool set_title)
{
    if (m_search_pending)
        search_timeout ();

    int n_items = m_model.num_items ();
    int n_selected = 0;

    auto list = m_library.playlist ();
    Index<PlaylistAddItem> add;
    String title;

    for (auto & idx : m_results_list.selectionModel ()->selectedRows ())
    {
        int i = idx.row ();
        if (i < 0 || i >= n_items)
            continue;

        auto & item = m_model.item_at (i);

        for (int entry : item.matches)
        {
            add.append (
                list.entry_filename (entry),
                list.entry_tuple (entry, Playlist::NoWait),
                list.entry_decoder (entry, Playlist::NoWait)
            );
        }

        n_selected ++;
        if (n_selected == 1)
            title = item.name;
    }

    auto list2 = Playlist::active_playlist ();
    list2.insert_items (-1, std::move (add), play);

    if (set_title && n_selected == 1)
        list2.set_title (title);
}

void SearchWidget::action_play ()
{
    Playlist::temporary_playlist ().activate ();
    do_add (true, false);
}

void SearchWidget::action_create_playlist ()
{
    Playlist::new_playlist ();
    do_add (false, true);
}

void SearchWidget::action_add_to_playlist ()
{
    if (m_library.playlist () != Playlist::active_playlist ())
        do_add (false, false);
}

void SearchWidget::show_context_menu (const QPoint & global_pos)
{
    auto menu = new QMenu (this);

    auto play_act = new QAction (audqt::get_icon ("media-playback-start"),
                                 audqt::translate_str (N_("_Play")), menu);
    auto create_act = new QAction (audqt::get_icon ("document-new"),
                                   audqt::translate_str (N_("_Create Playlist")), menu);
    auto add_act = new QAction (audqt::get_icon ("list-add"),
                                audqt::translate_str (N_("_Add to Playlist")), menu);

    QObject::connect (play_act, & QAction::triggered, this, & SearchWidget::action_play);
    QObject::connect (create_act, & QAction::triggered, this, & SearchWidget::action_create_playlist);
    QObject::connect (add_act, & QAction::triggered, this, & SearchWidget::action_add_to_playlist);

    menu->addAction (play_act);
    menu->addAction (create_act);
    menu->addAction (add_act);

    menu->setAttribute (Qt::WA_DeleteOnClose);
    menu->popup (global_pos);
}

bool SearchToolQt::init ()
{
    aud_config_set_defaults (CFG_ID, defaults);
    return true;
}

void * SearchToolQt::get_qt_widget ()
{
    if (! s_widget)
        s_widget = new SearchWidget;

    return s_widget;
}

int SearchToolQt::take_message (const char * code, const void *, int)
{
    if (! strcmp (code, "grab focus") && s_widget)
    {
        s_widget->grab_focus ();
        return 0;
    }

    return -1;
}
