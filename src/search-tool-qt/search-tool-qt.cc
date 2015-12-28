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
#include <QPushButton>
#include <QTreeView>

#define AUD_PLUGIN_QT_ONLY
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
        PACKAGE
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

struct SearchState {
    Index<const Item *> items;
    int mask;
};

class ResultsModel : public QAbstractListModel
{
public:
    void update ();

protected:
    int rowCount (const QModelIndex & parent) const { return m_rows; }
    int columnCount (const QModelIndex & parent) const { return 1; }

    QVariant data (const QModelIndex & index, int role) const;

private:
    int m_rows = 0;
};

class ResultsView : public QTreeView
{
protected:
    void contextMenuEvent (QContextMenuEvent * event);
};

static StringBuf create_item_label (int row);

static int playlist_id;
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
    playlist_id = -1;

    for (int p = 0; playlist_id < 0 && p < aud_playlist_count (); p ++)
    {
        String title = aud_playlist_get_title (p);
        if (! strcmp (title, _("Library")))
            playlist_id = aud_playlist_get_unique_id (p);
    }
}

static int create_playlist ()
{
    int list = aud_playlist_get_blank ();
    aud_playlist_set_title (list, _("Library"));
    aud_playlist_set_active (list);
    playlist_id = aud_playlist_get_unique_id (list);
    return list;
}

static int get_playlist (bool require_added, bool require_scanned)
{
    if (playlist_id < 0)
        return -1;

    int list = aud_playlist_by_unique_id (playlist_id);

    if (list < 0)
    {
        playlist_id = -1;
        return -1;
    }

    if (require_added && aud_playlist_add_in_progress (list))
        return -1;
    if (require_scanned && aud_playlist_scan_in_progress (list))
        return -1;

    return list;
}

static String get_path ()
{
    String path1 = aud_get_str ("search-tool", "path");
    if (g_file_test (path1, G_FILE_TEST_EXISTS))
        return path1;

    StringBuf path2 = filename_build ({g_get_home_dir (), "Music"});
    if (g_file_test (path2, G_FILE_TEST_EXISTS))
        return String (path2);

    return String (g_get_home_dir ());
}

static void destroy_database ()
{
    items.clear ();
    hidden_items = 0;
    database.clear ();
    database_valid = false;
}

static void create_database (int list)
{
    destroy_database ();

    int entries = aud_playlist_entry_count (list);

    for (int e = 0; e < entries; e ++)
    {
        Tuple tuple = aud_playlist_entry_get_tuple (list, e, Playlist::Nothing);

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

static void search_cb (const Key & key, Item & item, void * _state)
{
    SearchState * state = (SearchState *) _state;

    int oldmask = state->mask;
    int count = search_terms.len ();

    for (int t = 0, bit = 1; t < count; t ++, bit <<= 1)
    {
        if (! (state->mask & bit))
            continue; /* skip term if it is already found */

        if (strstr (item.folded, search_terms[t]))
            state->mask &= ~bit; /* we found it */
        else if (! item.children.n_items ())
            break; /* quit early if there are no children to search */
    }

    /* adding an item with exactly one child is redundant, so avoid it */
    if (! state->mask && item.children.n_items () != 1)
        state->items.append (& item);

    item.children.iterate (search_cb, state);

    state->mask = oldmask;
}

static int item_compare (const Item * const & a, const Item * const & b, void *)
{
    if (a->field < b->field)
        return -1;
    if (a->field > b->field)
        return 1;

    int val = str_compare (a->name, b->name);
    if (val)
        return val;

    if (a->parent)
        return b->parent ? item_compare (a->parent, b->parent, nullptr) : 1;
    else
        return b->parent ? -1 : 0;
}

static int item_compare_pass1 (const Item * const & a, const Item * const & b, void *)
{
    if (a->matches.len () > b->matches.len ())
        return -1;
    if (a->matches.len () < b->matches.len ())
        return 1;

    return item_compare (a, b, nullptr);
}

static void do_search ()
{
    items.clear ();
    hidden_items = 0;

    if (! database_valid)
        return;

    SearchState state;

    /* effectively limits number of search terms to 32 */
    state.mask = (1 << search_terms.len ()) - 1;

    database.iterate (search_cb, & state);

    items = std::move (state.items);

    /* first sort by number of songs per item */
    items.sort (item_compare_pass1, nullptr);

    /* limit to items with most songs */
    if (items.len () > MAX_RESULTS)
    {
        hidden_items = items.len () - MAX_RESULTS;
        items.remove (MAX_RESULTS, -1);
    }

    /* sort by item type, then item name */
    items.sort (item_compare, nullptr);
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

static void begin_add (const char * path)
{
    if (adding)
        return;

    int list = get_playlist (false, false);

    if (list < 0)
        list = create_playlist ();

    aud_set_str ("search-tool", "path", path);

    added_table.clear ();

    int entries = aud_playlist_entry_count (list);

    for (int entry = 0; entry < entries; entry ++)
    {
        String filename = aud_playlist_entry_get_filename (list, entry);

        if (! added_table.lookup (filename))
        {
            aud_playlist_entry_set_selected (list, entry, false);
            added_table.add (filename, false);
        }
        else
            aud_playlist_entry_set_selected (list, entry, true);
    }

    aud_playlist_delete_selected (list);

    tiny_lock (& adding_lock);
    adding = true;
    tiny_unlock (& adding_lock);

    Index<PlaylistAddItem> add;
    add.append (String (filename_to_uri (path)));
    aud_playlist_entry_insert_filtered (list, -1, std::move (add), filter_cb, nullptr, false);
}

static void show_hide_widgets ()
{
    if (playlist_id < 0)
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
    StringBuf stats = str_printf (dngettext (PACKAGE, "%d result",
     "%d results", total), total);

    if (hidden_items)
    {
        stats.insert (-1, " ");
        stats.combine (str_printf (dngettext (PACKAGE, "(%d hidden)",
         "(%d hidden)", hidden_items), hidden_items));
    }

    stats_label->setText ((const char *) stats);

    search_timer.stop ();
    search_pending = false;
}

static void update_database ()
{
    int list = get_playlist (true, true);

    if (list >= 0)
    {
        create_database (list);
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

static void add_complete_cb (void * unused, void * unused2)
{
    int list = get_playlist (true, false);
    if (list < 0)
        return;

    if (adding)
    {
        tiny_lock (& adding_lock);
        adding = false;
        tiny_unlock (& adding_lock);

        int entries = aud_playlist_entry_count (list);

        for (int entry = 0; entry < entries; entry ++)
        {
            String filename = aud_playlist_entry_get_filename (list, entry);
            bool * added = added_table.lookup (filename);

            aud_playlist_entry_set_selected (list, entry, ! added || ! (* added));
        }

        added_table.clear ();

        /* don't clear the playlist if nothing was added */
        if (aud_playlist_selected_count (list) < aud_playlist_entry_count (list))
            aud_playlist_delete_selected (list);
        else
            aud_playlist_select_all (list, false);

        aud_playlist_sort_by_scheme (list, Playlist::Path);
    }

    if (! database_valid && ! aud_playlist_update_pending (list))
        update_database ();
}

static void scan_complete_cb (void * unused, void * unused2)
{
    int list = get_playlist (true, true);
    if (list < 0)
        return;

    if (! database_valid && ! aud_playlist_update_pending (list))
        update_database ();
}

static void playlist_update_cb (void * data, void * unused)
{
    if (! database_valid)
        update_database ();
    else
    {
        int list = get_playlist (true, true);
        if (list < 0 || aud_playlist_update_detail (list).level >= Playlist::Metadata)
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

    int list = aud_playlist_by_unique_id (playlist_id);
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
                aud_playlist_entry_get_filename (list, entry),
                aud_playlist_entry_get_tuple (list, entry, Playlist::Nothing)
            );
        }

        n_selected ++;
        if (n_selected == 1)
            title = item->name;
    }

    int list2 = aud_playlist_get_active ();
    aud_playlist_entry_insert_batch (list2, -1, std::move (add), play);

    if (set_title && n_selected == 1)
        aud_playlist_set_title (list2, title);
}

static void action_play ()
{
    aud_playlist_set_active (aud_playlist_get_temporary ());
    do_add (true, false);
}

static void action_create_playlist ()
{
    aud_playlist_new ();
    do_add (false, true);
}

static void action_add_to_playlist ()
{
    if (aud_playlist_by_unique_id (playlist_id) != aud_playlist_get_active ())
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

void * SearchToolQt::get_qt_widget ()
{
    auto widget = new QWidget;
    auto vbox = new QVBoxLayout (widget);
    vbox->setContentsMargins (0, 0, 0, 0);

    auto entry = new QLineEdit;
    entry->setPlaceholderText (_("Search library"));
    vbox->addWidget (entry);

    help_label = new QLabel (_("To import your music library into Audacious, "
     "choose a folder and then click the \"refresh\" icon."));
    help_label->setAlignment (Qt::AlignCenter);
    help_label->setWordWrap (true);
    vbox->addWidget (help_label);

    wait_label = new QLabel (_("Please wait ..."));
    wait_label->setAlignment (Qt::AlignCenter);
    vbox->addWidget (wait_label);

    results_list = new ResultsView;
    results_list->setHeaderHidden (true);
    results_list->setIndentation (0);
    results_list->setModel (& model);
    results_list->setSelectionMode (QTreeView::ExtendedSelection);
    vbox->addWidget (results_list);

    stats_label = new QLabel;
    stats_label->setAlignment (Qt::AlignCenter);
    vbox->addWidget (stats_label);

    auto hbox = new QHBoxLayout;
    vbox->addLayout (hbox);

    auto chooser = new QLineEdit;
    hbox->addWidget (chooser);

    auto button = new QPushButton (QIcon::fromTheme ("view-refresh"), QString ());
    button->setFlat (true);
    hbox->addWidget (button);

    char * utf8 = g_filename_to_utf8 (get_path (), -1, nullptr, nullptr, nullptr);
    chooser->setText (utf8);
    g_free (utf8);

    search_init ();

    QObject::connect (vbox, & QObject::destroyed, search_cleanup);
    QObject::connect (entry, & QLineEdit::returnPressed, action_play);
    QObject::connect (results_list, & QTreeView::doubleClicked, action_play);

    QObject::connect (entry, & QLineEdit::textEdited, [] (const QString & text)
    {
        search_terms = str_list_to_index (str_tolower_utf8 (text.toUtf8 ()), " ");
        search_timer.queue (SEARCH_DELAY, search_timeout, nullptr);
        search_pending = true;
    });

    QObject::connect (button, & QPushButton::clicked, [chooser] ()
    {
        char * path = g_filename_from_utf8 (chooser->text ().toUtf8 (), -1,
         nullptr, nullptr, nullptr);

        if (path && path[0])
        {
            begin_add (path);
            update_database ();
        }

        g_free (path);
    });

    return widget;
}
