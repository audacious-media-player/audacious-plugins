/*
 * search-tool-qt.cc
 * Copyright 2011-2017 John Lindgren and René J.V. Bertin
 * Copyright 2019 Ariadne Conill
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

class SearchWidget : public QWidget
{
public:
    SearchWidget ();
    ~SearchWidget ();

    void grab_focus () { s_search_entry->setFocus (Qt::OtherFocusReason); }

    void trigger_search ();
    void reset_monitor ();

private:
    void show_hide_widgets ();
    void search_timeout ();
    void library_updated ();
    void walk_library_paths ();
    void setup_monitor ();
    void destroy_monitor ();
    void init_library ();

    void do_add (bool play, bool set_title);
    void action_play ();
    void action_create_playlist ();
    void action_add_to_playlist ();
    void show_context_menu (const QPoint & global_pos);

    Library * s_library = nullptr;
    SearchModel s_model;

    QFileSystemWatcher * s_watcher = nullptr;
    QStringList s_watcher_paths;

    QueuedFunc s_search_timer;
    bool s_search_pending = false;

    QLabel * s_help_label, * s_wait_label, * s_stats_label;
    QLineEdit * s_search_entry;
    QTreeView * s_results_list;
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

class ResultsView : public QTreeView
{
public:
    ResultsView ()
        { setItemDelegate (& m_delegate); }

private:
    HtmlDelegate m_delegate;
};

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

void SearchWidget::show_hide_widgets ()
{
    if (s_library->playlist () == Playlist ())
    {
        s_wait_label->hide ();
        s_results_list->hide ();
        s_stats_label->hide ();
        s_help_label->show ();
    }
    else
    {
        s_help_label->hide ();

        if (s_library->is_ready ())
        {
            s_wait_label->hide ();
            s_results_list->show ();
            s_stats_label->show ();
        }
        else
        {
            s_results_list->hide ();
            s_stats_label->hide ();
            s_wait_label->show ();
        }
    }
}

void SearchWidget::search_timeout ()
{
    auto text = s_search_entry->text ().toUtf8 ();
    auto terms = str_list_to_index (str_tolower_utf8 (text), " ");
    s_model.do_search (terms, aud_get_int (CFG_ID, "max_results"));
    s_model.update ();

    int shown = s_model.num_items ();
    int hidden = s_model.num_hidden_items ();
    int total = shown + hidden;

    if (shown)
    {
        auto sel = s_results_list->selectionModel ();
        sel->select (s_model.index (0, 0), sel->Clear | sel->SelectCurrent);
    }

    if (hidden)
        s_stats_label->setText ((const char *)
         str_printf (dngettext (PACKAGE, "%d of %d result shown",
         "%d of %d results shown", total), shown, total));
    else
        s_stats_label->setText ((const char *)
         str_printf (dngettext (PACKAGE, "%d result", "%d results", total), total));

    s_search_timer.stop ();
    s_search_pending = false;
}

void SearchWidget::trigger_search ()
{
    s_search_timer.queue (SEARCH_DELAY,
     aud::obj_member<SearchWidget, & SearchWidget::search_timeout>, this);
    s_search_pending = true;
}

void SearchWidget::library_updated ()
{
    if (s_library->is_ready ())
    {
        s_model.create_database (s_library->playlist ());
        search_timeout ();
    }
    else
    {
        s_model.destroy_database ();
        s_model.update ();
        s_stats_label->clear ();
    }

    show_hide_widgets ();
}

// QFileSystemWatcher doesn't support recursion, so we must do it ourselves.
// TODO: Since MacOS has an abysmally low default per-process FD limit, this
// means it probably won't work on MacOS with a huge media library.
// In the case of MacOS, we should use the FSEvents API instead.
void SearchWidget::walk_library_paths ()
{
    if (! s_watcher_paths.isEmpty ())
        s_watcher->removePaths (s_watcher_paths);

    s_watcher_paths.clear ();

    auto root = (QString) uri_to_filename (get_uri ());
    if (root.isEmpty ())
        return;

    s_watcher_paths.append (root);

    QDirIterator it (root, QDir::Dirs | QDir::NoDot | QDir::NoDotDot, QDirIterator::Subdirectories);
    while (it.hasNext ())
        s_watcher_paths.append (it.next ());

    s_watcher->addPaths (s_watcher_paths);
}

void SearchWidget::setup_monitor ()
{
    AUDINFO ("Starting monitoring.\n");
    s_watcher = new QFileSystemWatcher;

    QObject::connect (s_watcher, & QFileSystemWatcher::directoryChanged, [&] (const QString &path) {
        AUDINFO ("Library directory changed, refreshing library.\n");

        s_library->begin_add (get_uri ());
        s_library->check_ready_and_update (true);

        walk_library_paths ();
    });

    walk_library_paths ();
}

void SearchWidget::destroy_monitor ()
{
    if (! s_watcher)
        return;

    AUDINFO ("Stopping monitoring.\n");
    delete s_watcher;
    s_watcher = nullptr;
    s_watcher_paths.clear ();
}

void SearchWidget::reset_monitor ()
{
    destroy_monitor ();

    if (aud_get_bool (CFG_ID, "monitor"))
        setup_monitor ();
}

void SearchWidget::init_library ()
{
    s_library = new Library;
    s_library->connect_update
     (aud::obj_member<SearchWidget, & SearchWidget::library_updated>, this);

    if (aud_get_bool (CFG_ID, "rescan_on_startup"))
        s_library->begin_add (get_uri ());

    s_library->check_ready_and_update (true);
    reset_monitor ();
}

SearchWidget::~SearchWidget ()
{
    destroy_monitor ();

    s_search_timer.stop ();
    s_search_pending = false;

    delete s_library;
    s_library = nullptr;

    s_model.destroy_database ();

    s_help_label = s_wait_label = s_stats_label = nullptr;
    s_search_entry = nullptr;
    s_results_list = nullptr;
}

void SearchWidget::do_add (bool play, bool set_title)
{
    if (s_search_pending)
        search_timeout ();

    int n_items = s_model.num_items ();
    int n_selected = 0;

    auto list = s_library->playlist ();
    Index<PlaylistAddItem> add;
    String title;

    for (auto & idx : s_results_list->selectionModel ()->selectedRows ())
    {
        int i = idx.row ();
        if (i < 0 || i >= n_items)
            continue;

        auto & item = s_model.item_at (i);

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
    if (s_library->playlist () != Playlist::active_playlist ())
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

SearchWidget::SearchWidget ()
{
    s_search_entry = new QLineEdit;
    s_search_entry->setClearButtonEnabled (true);
    s_search_entry->setPlaceholderText (_("Search library"));

    s_help_label = new QLabel (_("To import your music library into Audacious, "
     "choose a folder and then click the \"refresh\" icon."));
    s_help_label->setAlignment (Qt::AlignCenter);
    s_help_label->setContentsMargins (audqt::margins.EightPt);
    s_help_label->setWordWrap (true);

    s_wait_label = new QLabel (_("Please wait ..."));
    s_wait_label->setAlignment (Qt::AlignCenter);
    s_wait_label->setContentsMargins (audqt::margins.EightPt);

    s_results_list = new ResultsView;
    s_results_list->setFrameStyle (QFrame::NoFrame);
    s_results_list->setHeaderHidden (true);
    s_results_list->setIndentation (0);
    s_results_list->setModel (& s_model);
    s_results_list->setSelectionMode (QTreeView::ExtendedSelection);
    s_results_list->setDragDropMode (QTreeView::DragOnly);
    s_results_list->setContextMenuPolicy (Qt::CustomContextMenu);

    s_stats_label = new QLabel;
    s_stats_label->setAlignment (Qt::AlignCenter);
    s_stats_label->setContentsMargins (audqt::margins.TwoPt);

#ifdef Q_OS_MAC  // Mac-specific font tweaks
    s_search_entry->setFont (QApplication::font ("QTreeView"));
    s_stats_label->setFont (QApplication::font ("QSmallFont"));
#endif

    auto chooser = audqt::file_entry_new (nullptr, _("Choose Folder"),
     QFileDialog::Directory, QFileDialog::AcceptOpen);

    auto button = new QPushButton (audqt::get_icon ("view-refresh"), QString ());
    button->setFlat (true);
    button->setFocusPolicy (Qt::NoFocus);

    auto hbox1 = audqt::make_hbox (nullptr);
    hbox1->setContentsMargins (audqt::margins.TwoPt);
    hbox1->addWidget (s_search_entry);

    auto hbox2 = audqt::make_hbox (nullptr);
    hbox2->setContentsMargins (audqt::margins.TwoPt);
    hbox2->addWidget (chooser);
    hbox2->addWidget (button);

    auto vbox = audqt::make_vbox (this, 0);
    vbox->addLayout (hbox1);
    vbox->addWidget (s_help_label);
    vbox->addWidget (s_wait_label);
    vbox->addWidget (s_results_list);
    vbox->addWidget (s_stats_label);
    vbox->addLayout (hbox2);

    audqt::file_entry_set_uri (chooser, get_uri ());

    init_library ();

    QObject::connect (s_search_entry, & QLineEdit::textEdited, this, & SearchWidget::trigger_search);
    QObject::connect (s_search_entry, & QLineEdit::returnPressed, this, & SearchWidget::action_play);
    QObject::connect (s_results_list, & QTreeView::activated, this, & SearchWidget::action_play);

    QObject::connect (s_results_list, & QWidget::customContextMenuRequested,
     [this] (const QPoint & pos) { show_context_menu (s_results_list->mapToGlobal (pos)); });

    QObject::connect (chooser, & QLineEdit::textChanged, [button] (const QString & text)
        { button->setDisabled (text.isEmpty ()); });

    auto refresh = [this, chooser] () {
        String uri = audqt::file_entry_get_uri (chooser);
        if (uri)
        {
            audqt::file_entry_set_uri (chooser, uri);  // normalize path
            /* if possible, store local path for compatibility with older versions */
            StringBuf path = uri_to_filename (uri);
            aud_set_str (CFG_ID, "path", path ? path : uri);

            s_library->begin_add (uri);
            s_library->check_ready_and_update (true);
            reset_monitor ();
        }
    };

    QObject::connect (chooser, & QLineEdit::returnPressed, refresh);
    QObject::connect (button, & QPushButton::clicked, refresh);
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
