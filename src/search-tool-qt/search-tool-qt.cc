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

#include <string.h>
#include <glib.h>

#include <QBoxLayout>
#include <QContextMenuEvent>
#include <QDirIterator>
#include <QFileSystemWatcher>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QTreeView>

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/mainloop.h>
#include <libaudcore/runtime.h>
#include <libaudqt/libaudqt.h>
#include <libaudqt/menu.h>

#include "html-delegate.h"
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

static void trigger_search ();
static void reset_monitor ();

const char * const SearchToolQt::defaults[] = {
    "max_results", "20",
    "rescan_on_startup", "FALSE",
    "monitor", "FALSE",
    nullptr
};

const PreferencesWidget SearchToolQt::widgets[] = {
    WidgetSpin (N_("Number of results to show:"),
        WidgetInt (CFG_ID, "max_results", trigger_search),
         {10, 10000, 10}),
    WidgetCheck (N_("Rescan library at startup"),
        WidgetBool (CFG_ID, "rescan_on_startup")),
    WidgetCheck (N_("Monitor library for changes"),
        WidgetBool (CFG_ID, "monitor", reset_monitor))
};

const PluginPreferences SearchToolQt::prefs = {{widgets}};

class ResultsView : public QTreeView
{
public:
    ResultsView ()
        { setItemDelegate (& m_delegate); }

protected:
    void contextMenuEvent (QContextMenuEvent * event);

private:
    HtmlDelegate m_delegate;
};

class Library
{
public:
    Library () { find_playlist (); }
    ~Library () { set_adding (false); }

    Playlist playlist () const { return m_playlist; }
    bool is_ready () const { return m_is_ready; }

    void begin_add (const char * uri);
    void check_ready_and_update (bool force);

private:
    void find_playlist ();
    void create_playlist ();
    bool check_playlist (bool require_added, bool require_scanned);
    void set_adding (bool adding);

    void add_complete (void);
    void scan_complete (void);
    void playlist_update (void);

    static bool filter_cb (const char * filename, void *);
    static void signal_update (); /* implemented externally */

    Playlist m_playlist;
    bool m_is_ready = false;
    SimpleHash<String, bool> m_added_table;

    /* to allow safe callback access from playlist add thread */
    static aud::spinlock s_adding_lock;
    static Library * s_adding_library;

    HookReceiver<Library>
     hook1 {"playlist add complete", this, & Library::add_complete},
     hook2 {"playlist scan complete", this, & Library::scan_complete},
     hook3 {"playlist update", this, & Library::playlist_update};
};

aud::spinlock Library::s_adding_lock;
Library * Library::s_adding_library = nullptr;

static Library * s_library = nullptr;

static QFileSystemWatcher * s_watcher;
static QStringList s_watcher_paths;

static QueuedFunc s_search_timer;
static bool s_search_pending;

static SearchModel s_model;
static QLabel * s_help_label, * s_wait_label, * s_stats_label;
static QLineEdit * s_search_entry;
static QTreeView * s_results_list;
static QMenu * s_menu;

void Library::find_playlist ()
{
    m_playlist = Playlist ();

    for (int p = 0; p < Playlist::n_playlists (); p ++)
    {
        auto playlist = Playlist::by_index (p);
        if (! strcmp (playlist.get_title (), _("Library")))
        {
            m_playlist = playlist;
            break;
        }
    }
}

void Library::create_playlist ()
{
    m_playlist = Playlist::blank_playlist ();
    m_playlist.set_title (_("Library"));
    m_playlist.active_playlist ();
}

bool Library::check_playlist (bool require_added, bool require_scanned)
{
    if (! m_playlist.exists ())
    {
        m_playlist = Playlist ();
        return false;
    }

    if (require_added && m_playlist.add_in_progress ())
        return false;
    if (require_scanned && m_playlist.scan_in_progress ())
        return false;

    return true;
}

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

void Library::set_adding (bool adding)
{
    auto lh = s_adding_lock.take ();
    s_adding_library = adding ? this : nullptr;
}

bool Library::filter_cb (const char * filename, void *)
{
    bool add = false;
    auto lh = s_adding_lock.take ();

    if (s_adding_library)
    {
        bool * added = s_adding_library->m_added_table.lookup (String (filename));

        if ((add = ! added))
            s_adding_library->m_added_table.add (String (filename), true);
        else
            (* added) = true;
    }

    return add;
}

void Library::begin_add (const char * uri)
{
    if (s_adding_library)
        return;

    if (! check_playlist (false, false))
        create_playlist ();

    m_added_table.clear ();

    int entries = m_playlist.n_entries ();

    for (int entry = 0; entry < entries; entry ++)
    {
        String filename = m_playlist.entry_filename (entry);

        if (! m_added_table.lookup (filename))
        {
            m_playlist.select_entry (entry, false);
            m_added_table.add (filename, false);
        }
        else
            m_playlist.select_entry (entry, true);
    }

    m_playlist.remove_selected ();

    set_adding (true);

    Index<PlaylistAddItem> add;
    add.append (String (uri));
    m_playlist.insert_filtered (-1, std::move (add), filter_cb, nullptr, false);
}

static void show_hide_widgets ()
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

static void search_timeout (void * = nullptr)
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

static void trigger_search ()
{
    s_search_timer.queue (SEARCH_DELAY, search_timeout, nullptr);
    s_search_pending = true;
}

void Library::signal_update ()
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

void Library::check_ready_and_update (bool force)
{
    bool now_ready = check_playlist (true, true);
    if (now_ready != m_is_ready || force)
    {
        m_is_ready = now_ready;
        signal_update ();
    }
}

void Library::add_complete ()
{
    if (! check_playlist (true, false))
        return;

    if (s_adding_library)
    {
        set_adding (false);

        int entries = m_playlist.n_entries ();

        for (int entry = 0; entry < entries; entry ++)
        {
            String filename = m_playlist.entry_filename (entry);
            bool * added = m_added_table.lookup (filename);

            m_playlist.select_entry (entry, ! added || ! (* added));
        }

        m_added_table.clear ();

        /* don't clear the playlist if nothing was added */
        if (m_playlist.n_selected () < entries)
            m_playlist.remove_selected ();
        else
            m_playlist.select_all (false);

        m_playlist.sort_entries (Playlist::Path);
    }

    if (! m_playlist.update_pending ())
        check_ready_and_update (false);
}

void Library::scan_complete ()
{
    if (! m_playlist.update_pending ())
        check_ready_and_update (false);
}

void Library::playlist_update ()
{
    check_ready_and_update (m_playlist.update_detail ().level >= Playlist::Metadata);
}

// QFileSystemWatcher doesn't support recursion, so we must do it ourselves.
// TODO: Since MacOS has an abysmally low default per-process FD limit, this
// means it probably won't work on MacOS with a huge media library.
// In the case of MacOS, we should use the FSEvents API instead.
static void walk_library_paths ()
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

static void setup_monitor ()
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

static void destroy_monitor ()
{
    if (! s_watcher)
        return;

    AUDINFO ("Stopping monitoring.\n");
    delete s_watcher;
    s_watcher = nullptr;
    s_watcher_paths.clear ();
}

static void reset_monitor ()
{
    destroy_monitor ();

    if (aud_get_bool (CFG_ID, "monitor"))
        setup_monitor ();
}

static void search_init ()
{
    s_library = new Library;

    if (aud_get_bool (CFG_ID, "rescan_on_startup"))
        s_library->begin_add (get_uri ());

    s_library->check_ready_and_update (true);
    reset_monitor ();
}

static void search_cleanup ()
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

    delete s_menu;
    s_menu = nullptr;
}

static void do_add (bool play, bool set_title)
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

static void action_play ()
{
    Playlist::temporary_playlist ().activate ();
    do_add (true, false);
}

static void action_create_playlist ()
{
    Playlist::new_playlist ();
    do_add (false, true);
}

static void action_add_to_playlist ()
{
    if (s_library->playlist () != Playlist::active_playlist ())
        do_add (false, false);
}

void ResultsView::contextMenuEvent (QContextMenuEvent * event)
{
    static const audqt::MenuItem items[] = {
        audqt::MenuCommand ({N_("_Play"), "media-playback-start"}, action_play),
        audqt::MenuCommand ({N_("_Create Playlist"), "document-new"}, action_create_playlist),
        audqt::MenuCommand ({N_("_Add to Playlist"), "list-add"}, action_add_to_playlist)
    };

    if (! s_menu)
        s_menu = audqt::menu_build ({items});

    s_menu->popup (event->globalPos ());
}

bool SearchToolQt::init ()
{
    aud_config_set_defaults (CFG_ID, defaults);
    return true;
}

void * SearchToolQt::get_qt_widget ()
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

    auto widget = new QWidget;
    auto vbox = audqt::make_vbox (widget, 0);

    vbox->addLayout (hbox1);
    vbox->addWidget (s_help_label);
    vbox->addWidget (s_wait_label);
    vbox->addWidget (s_results_list);
    vbox->addWidget (s_stats_label);
    vbox->addLayout (hbox2);

    audqt::file_entry_set_uri (chooser, get_uri ());

    search_init ();

    QObject::connect (widget, & QObject::destroyed, search_cleanup);
    QObject::connect (s_search_entry, & QLineEdit::textEdited, trigger_search);
    QObject::connect (s_search_entry, & QLineEdit::returnPressed, action_play);
    QObject::connect (s_results_list, & QTreeView::activated, action_play);

    QObject::connect (chooser, & QLineEdit::textChanged, [button] (const QString & text)
        { button->setDisabled (text.isEmpty ()); });

    auto refresh = [chooser] () {
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

    return widget;
}

int SearchToolQt::take_message (const char * code, const void *, int)
{
    if (! strcmp (code, "grab focus") && s_search_entry)
    {
        s_search_entry->setFocus (Qt::OtherFocusReason);
        return 0;
    }

    return -1;
}
