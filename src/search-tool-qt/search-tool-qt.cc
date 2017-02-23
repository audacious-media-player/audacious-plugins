/*
 * search-tool-qt.cc
 * Copyright 2011-2015 John Lindgren
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

#include <QAbstractListModel>
#include <QBoxLayout>
#include <QContextMenuEvent>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QTreeView>
#include <QUrl>

#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>
#include <libaudcore/mainloop.h>
#include <libaudcore/multihash.h>
#include <libaudcore/runtime.h>
#include <libaudqt/libaudqt.h>
#include <libaudqt/menu.h>

#define MAX_RESULTS 20
#define SEARCH_DELAY 300

class SearchToolQt : public GeneralPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Search Tool"),
        PACKAGE,
        nullptr, // about
        nullptr, // prefs
        PluginQtOnly
    };

    constexpr SearchToolQt () : GeneralPlugin (info, false) {}

    void * get_qt_widget ();
};

EXPORT SearchToolQt aud_plugin_instance;

enum class SearchField {
    Genre,
    Artist,
    Album,
    Title,
    count
};

struct Key
{
    SearchField field;
    String name;

    bool operator== (const Key & b) const
        { return field == b.field && name == b.name; }
    unsigned hash () const
        { return (unsigned) field + name.hash (); }
};

struct Item
{
    SearchField field;
    String name, folded;
    Item * parent;
    SimpleHash<Key, Item> children;
    Index<int> matches;

    Item (SearchField field, const String & name, Item * parent) :
        field (field),
        name (name),
        folded (str_tolower_utf8 (name)),
        parent (parent) {}

    Item (Item &&) = default;
    Item & operator= (Item &&) = default;
};

class ResultsModel : public QAbstractListModel
{
public:
    void update ();

protected:
    int rowCount (const QModelIndex & parent) const { return m_rows; }
    int columnCount (const QModelIndex & parent) const { return 1; }
    QVariant data (const QModelIndex & index, int role) const;

    Qt::ItemFlags flags (const QModelIndex & index) const
    {
        if (index.isValid ())
            return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
        else
            return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    }

    QStringList mimeTypes () const
    {
        return QStringList ("text/uri-list");
    }

    QMimeData * mimeData (const QModelIndexList & indexes) const;

private:
    int m_rows = 0;
};

class ResultsView : public QTreeView
{
protected:
    void contextMenuEvent (QContextMenuEvent * event);
};

static StringBuf create_item_label (int row);

static Playlist s_playlist;
static Index<String> search_terms;

/* Note: added_table is accessed by multiple threads.
 * When adding = true, it may only be accessed by the playlist add thread.
 * When adding = false, it may only be accessed by the UI thread.
 * adding may only be set by the UI thread while holding adding_lock. */
static TinyLock adding_lock;
static bool adding = false;
static SimpleHash<String, bool> added_table;

static SimpleHash<Key, Item> database;
static bool database_valid;
static Index<const Item *> items;
static int hidden_items;

static QueuedFunc search_timer;
static bool search_pending;

static ResultsModel model;
static QLabel * help_label, * wait_label, * stats_label;
static QTreeView * results_list;
static QMenu * menu;

void ResultsModel::update ()
{
    int rows = items.len ();
    int keep = aud::min (rows, m_rows);

    if (rows < m_rows)
    {
        beginRemoveRows (QModelIndex (), rows, m_rows - 1);
        m_rows = rows;
        endRemoveRows ();
    }
    else if (rows > m_rows)
    {
        beginInsertRows (QModelIndex (), m_rows, rows - 1);
        m_rows = rows;
        endInsertRows ();
    }

    if (keep > 0)
    {
        auto topLeft = createIndex (0, 0);
        auto bottomRight = createIndex (keep - 1, 0);
        emit dataChanged (topLeft, bottomRight);
    }
}

QVariant ResultsModel::data (const QModelIndex & index, int role) const
{
    if (role == Qt::DisplayRole)
        return QString ((const char *) create_item_label (index.row ()));
    else
        return QVariant ();
}

static void find_playlist ()
{
    s_playlist = Playlist ();

    for (int p = 0; p < Playlist::n_playlists (); p ++)
    {
        auto playlist = Playlist::by_index (p);
        if (! strcmp (playlist.get_title (), _("Library")))
        {
            s_playlist = playlist;
            break;
        }
    }
}

static void create_playlist ()
{
    s_playlist = Playlist::blank_playlist ();
    s_playlist.set_title (_("Library"));
    s_playlist.active_playlist ();
}

static bool check_playlist (bool require_added, bool require_scanned)
{
    if (! s_playlist.exists ())
    {
        s_playlist = Playlist ();
        return false;
    }

    if (require_added && s_playlist.add_in_progress ())
        return false;
    if (require_scanned && s_playlist.scan_in_progress ())
        return false;

    return true;
}

static String get_uri ()
{
    auto to_uri = [] (const char * path)
        { return String (filename_to_uri (path)); };

    String path1 = aud_get_str ("search-tool", "path");
    if (path1[0])
        return strstr (path1, "://") ? path1 : to_uri (path1);

    StringBuf path2 = filename_build ({g_get_home_dir (), "Music"});
    if (g_file_test (path2, G_FILE_TEST_EXISTS))
        return to_uri (path2);

    return to_uri (g_get_home_dir ());
}

static void destroy_database ()
{
    items.clear ();
    hidden_items = 0;
    database.clear ();
    database_valid = false;
}

static void create_database ()
{
    destroy_database ();

    int entries = s_playlist.n_entries ();

    for (int e = 0; e < entries; e ++)
    {
        Tuple tuple = s_playlist.entry_tuple (e, Playlist::NoWait);

        aud::array<SearchField, String> fields;
        fields[SearchField::Genre] = tuple.get_str (Tuple::Genre);
        fields[SearchField::Artist] = tuple.get_str (Tuple::Artist);
        fields[SearchField::Album] = tuple.get_str (Tuple::Album);
        fields[SearchField::Title] = tuple.get_str (Tuple::Title);

        Item * parent = nullptr;
        SimpleHash<Key, Item> * hash = & database;

        for (auto f : aud::range<SearchField> ())
        {
            if (fields[f])
            {
                Key key = {f, fields[f]};
                Item * item = hash->lookup (key);

                if (! item)
                    item = hash->add (key, Item (f, fields[f], parent));

                item->matches.append (e);

                /* genre is outside the normal hierarchy */
                if (f != SearchField::Genre)
                {
                    parent = item;
                    hash = & item->children;
                }
            }
        }
    }

    database_valid = true;
}

static void search_recurse (SimpleHash<Key, Item> & domain, int mask, Index<const Item *> & results)
{
    domain.iterate ([mask, & results] (const Key & key, Item & item)
    {
        int count = search_terms.len ();
        int new_mask = mask;

        for (int t = 0, bit = 1; t < count; t ++, bit <<= 1)
        {
            if (! (new_mask & bit))
                continue; /* skip term if it is already found */

            if (strstr (item.folded, search_terms[t]))
                new_mask &= ~bit; /* we found it */
            else if (! item.children.n_items ())
                break; /* quit early if there are no children to search */
        }

        /* adding an item with exactly one child is redundant, so avoid it */
        if (! new_mask && item.children.n_items () != 1)
            results.append (& item);

        search_recurse (item.children, new_mask, results);
    });
}

static int item_compare (const Item * const & a, const Item * const & b)
{
    if (a->field < b->field)
        return -1;
    if (a->field > b->field)
        return 1;

    int val = str_compare (a->name, b->name);
    if (val)
        return val;

    if (a->parent)
        return b->parent ? item_compare (a->parent, b->parent) : 1;
    else
        return b->parent ? -1 : 0;
}

static int item_compare_pass1 (const Item * const & a, const Item * const & b)
{
    if (a->matches.len () > b->matches.len ())
        return -1;
    if (a->matches.len () < b->matches.len ())
        return 1;

    return item_compare (a, b);
}

static void do_search ()
{
    items.clear ();
    hidden_items = 0;

    if (! database_valid)
        return;

    /* effectively limits number of search terms to 32 */
    search_recurse (database, (1 << search_terms.len ()) - 1, items);

    /* first sort by number of songs per item */
    items.sort (item_compare_pass1);

    /* limit to items with most songs */
    if (items.len () > MAX_RESULTS)
    {
        hidden_items = items.len () - MAX_RESULTS;
        items.remove (MAX_RESULTS, -1);
    }

    /* sort by item type, then item name */
    items.sort (item_compare);
}

static bool filter_cb (const char * filename, void * unused)
{
    bool add = false;
    tiny_lock (& adding_lock);

    if (adding)
    {
        bool * added = added_table.lookup (String (filename));

        if ((add = ! added))
            added_table.add (String (filename), true);
        else
            (* added) = true;
    }

    tiny_unlock (& adding_lock);
    return add;
}

static void begin_add (const char * uri)
{
    if (adding)
        return;

    if (! check_playlist (false, false))
        create_playlist ();

    /* if possible, store local path for compatibility with older versions */
    StringBuf path = uri_to_filename (uri);
    aud_set_str ("search-tool", "path", path ? path : uri);

    added_table.clear ();

    int entries = s_playlist.n_entries ();

    for (int entry = 0; entry < entries; entry ++)
    {
        String filename = s_playlist.entry_filename (entry);

        if (! added_table.lookup (filename))
        {
            s_playlist.select_entry (entry, false);
            added_table.add (filename, false);
        }
        else
            s_playlist.select_entry (entry, true);
    }

    s_playlist.remove_selected ();

    tiny_lock (& adding_lock);
    adding = true;
    tiny_unlock (& adding_lock);

    Index<PlaylistAddItem> add;
    add.append (String (uri));
    s_playlist.insert_filtered (-1, std::move (add), filter_cb, nullptr, false);
}

static void show_hide_widgets ()
{
    if (s_playlist == Playlist ())
    {
        wait_label->hide ();
        results_list->hide ();
        stats_label->hide ();
        help_label->show ();
    }
    else
    {
        help_label->hide ();

        if (database_valid)
        {
            wait_label->hide ();
            results_list->show ();
            stats_label->show ();
        }
        else
        {
            results_list->hide ();
            stats_label->hide ();
            wait_label->show ();
        }
    }
}

static void search_timeout (void * = nullptr)
{
    do_search ();

    model.update ();

    if (items.len ())
    {
        auto sel = results_list->selectionModel ();
        sel->select (model.index (0, 0), sel->Clear | sel->SelectCurrent);
    }

    int total = items.len () + hidden_items;

    if (hidden_items)
        stats_label->setText ((const char *)
         str_printf (dngettext (PACKAGE, "%d of %d result shown",
         "%d of %d results shown", total), items.len (), total));
    else
        stats_label->setText ((const char *)
         str_printf (dngettext (PACKAGE, "%d result", "%d results", total), total));

    search_timer.stop ();
    search_pending = false;
}

static void update_database ()
{
    if (check_playlist (true, true))
    {
        create_database ();
        search_timeout ();
    }
    else
    {
        destroy_database ();
        model.update ();
        stats_label->clear ();
    }

    show_hide_widgets ();
}

static void add_complete_cb (void *, void *)
{
    if (! check_playlist (true, false))
        return;

    if (adding)
    {
        tiny_lock (& adding_lock);
        adding = false;
        tiny_unlock (& adding_lock);

        int entries = s_playlist.n_entries ();

        for (int entry = 0; entry < entries; entry ++)
        {
            String filename = s_playlist.entry_filename (entry);
            bool * added = added_table.lookup (filename);

            s_playlist.select_entry (entry, ! added || ! (* added));
        }

        added_table.clear ();

        /* don't clear the playlist if nothing was added */
        if (s_playlist.n_selected () < entries)
            s_playlist.remove_selected ();
        else
            s_playlist.select_all (false);

        s_playlist.sort_entries (Playlist::Path);
    }

    if (! database_valid && ! s_playlist.update_pending ())
        update_database ();
}

static void scan_complete_cb (void *, void *)
{
    if (! check_playlist (true, true))
        return;

    if (! database_valid && ! s_playlist.update_pending ())
        update_database ();
}

static void playlist_update_cb (void *, void *)
{
    if (! database_valid || ! check_playlist (true, true) ||
        s_playlist.update_detail ().level >= Playlist::Metadata)
    {
        update_database ();
    }
}

static void search_init ()
{
    find_playlist ();

    update_database ();

    hook_associate ("playlist add complete", add_complete_cb, nullptr);
    hook_associate ("playlist scan complete", scan_complete_cb, nullptr);
    hook_associate ("playlist update", playlist_update_cb, nullptr);
}

static void search_cleanup ()
{
    hook_dissociate ("playlist add complete", add_complete_cb);
    hook_dissociate ("playlist scan complete", scan_complete_cb);
    hook_dissociate ("playlist update", playlist_update_cb);

    search_timer.stop ();
    search_pending = false;

    search_terms.clear ();
    items.clear ();

    tiny_lock (& adding_lock);
    adding = false;
    tiny_unlock (& adding_lock);

    added_table.clear ();
    destroy_database ();

    delete menu;
    menu = nullptr;
}

static void do_add (bool play, bool set_title)
{
    if (search_pending)
        search_timeout ();

    int n_items = items.len ();
    int n_selected = 0;

    Index<PlaylistAddItem> add;
    String title;

    for (auto & idx : results_list->selectionModel ()->selectedRows ())
    {
        int i = idx.row ();
        if (i < 0 || i >= n_items)
            continue;

        const Item * item = items[i];

        for (int entry : item->matches)
        {
            add.append (
                s_playlist.entry_filename (entry),
                s_playlist.entry_tuple (entry, Playlist::NoWait),
                s_playlist.entry_decoder (entry, Playlist::NoWait)
            );
        }

        n_selected ++;
        if (n_selected == 1)
            title = item->name;
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
    if (s_playlist != Playlist::active_playlist ())
        do_add (false, false);
}

static StringBuf create_item_label (int row)
{
    if (row < 0 || row >= items.len ())
        return StringBuf ();

    const Item * item = items[row];
    StringBuf string = str_concat ({item->name, "\n"});

    if (item->field != SearchField::Title)
    {
        string.insert (-1, " ");
        string.combine (str_printf (dngettext (PACKAGE, "%d song", "%d songs",
         item->matches.len ()), item->matches.len ()));
    }

    if (item->field == SearchField::Genre)
    {
        string.insert (-1, " ");
        string.insert (-1, _("of this genre"));
    }

    while ((item = item->parent))
    {
        string.insert (-1, " ");
        string.insert (-1, (item->field == SearchField::Album) ? _("on") : _("by"));
        string.insert (-1, " ");
        string.insert (-1, item->name);
    }

    return string;
}

void ResultsView::contextMenuEvent (QContextMenuEvent * event)
{
    static const audqt::MenuItem items[] = {
        audqt::MenuCommand ({N_("_Play"), "media-playback-start"}, action_play),
        audqt::MenuCommand ({N_("_Create Playlist"), "document-new"}, action_create_playlist),
        audqt::MenuCommand ({N_("_Add to Playlist"), "list-add"}, action_add_to_playlist)
    };

    if (! menu)
        menu = audqt::menu_build ({items});

    menu->popup (event->globalPos ());
}

QMimeData * ResultsModel::mimeData (const QModelIndexList & indexes) const
{
    if (search_pending)
        search_timeout ();

    s_playlist.select_all (false);

    QList<QUrl> urls;
    for (auto & index : indexes)
    {
        int row = index.row ();
        if (row < 0 || row >= items.len ())
            continue;

        for (int entry : items[row]->matches)
        {
            urls.append (QString (s_playlist.entry_filename (entry)));
            s_playlist.select_entry (entry, true);
        }
    }

    s_playlist.cache_selected ();

    auto data = new QMimeData;
    data->setUrls (urls);
    return data;
}

void * SearchToolQt::get_qt_widget ()
{
    auto entry = new QLineEdit;
    entry->setContentsMargins (audqt::margins.TwoPt);
    entry->setClearButtonEnabled (true);
    entry->setPlaceholderText (_("Search library"));

    help_label = new QLabel (_("To import your music library into Audacious, "
     "choose a folder and then click the \"refresh\" icon."));
    help_label->setAlignment (Qt::AlignCenter);
    help_label->setContentsMargins (audqt::margins.EightPt);
    help_label->setWordWrap (true);

    wait_label = new QLabel (_("Please wait ..."));
    wait_label->setAlignment (Qt::AlignCenter);
    wait_label->setContentsMargins (audqt::margins.EightPt);

    results_list = new ResultsView;
    results_list->setFrameStyle (QFrame::NoFrame);
    results_list->setHeaderHidden (true);
    results_list->setIndentation (0);
    results_list->setModel (& model);
    results_list->setSelectionMode (QTreeView::ExtendedSelection);
    results_list->setDragDropMode (QTreeView::DragOnly);

    stats_label = new QLabel;
    stats_label->setAlignment (Qt::AlignCenter);
    stats_label->setContentsMargins (audqt::margins.TwoPt);

    auto chooser = new QLineEdit;

    auto button = new QPushButton (QIcon::fromTheme ("view-refresh"), QString ());
    button->setFlat (true);
    button->setFocusPolicy (Qt::NoFocus);

    auto hbox = audqt::make_hbox (nullptr);
    hbox->setContentsMargins (audqt::margins.TwoPt);

    hbox->addWidget (chooser);
    hbox->addWidget (button);

    auto widget = new QWidget;
    auto vbox = audqt::make_vbox (widget, 0);

    vbox->addWidget (entry);
    vbox->addWidget (help_label);
    vbox->addWidget (wait_label);
    vbox->addWidget (results_list);
    vbox->addWidget (stats_label);
    vbox->addLayout (hbox);

    String uri = get_uri ();
    StringBuf path = uri_to_filename (uri, false);
    chooser->setText (path ? path : uri);

    search_init ();

    QObject::connect (widget, & QObject::destroyed, search_cleanup);
    QObject::connect (entry, & QLineEdit::returnPressed, action_play);
    QObject::connect (results_list, & QTreeView::doubleClicked, action_play);

    QObject::connect (entry, & QLineEdit::textEdited, [] (const QString & text)
    {
        search_terms = str_list_to_index (str_tolower_utf8 (text.toUtf8 ()), " ");
        search_timer.queue (SEARCH_DELAY, search_timeout, nullptr);
        search_pending = true;
    });

    QObject::connect (chooser, & QLineEdit::textEdited, [button] (const QString & text)
        { button->setDisabled (text.isEmpty ()); });

    auto refresh = [chooser] () {
        QByteArray path = chooser->text ().toUtf8 ();
        if (! path.isEmpty ())
        {
            begin_add (strstr (path, "://") ? path : filename_to_uri (path));
            update_database ();
        }
    };

    QObject::connect (chooser, & QLineEdit::returnPressed, refresh);
    QObject::connect (button, & QPushButton::clicked, refresh);

    return widget;
}
