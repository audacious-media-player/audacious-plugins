/*
 * playlist_columns.cc
 * Copyright 2017 Eugene Paskevich
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

#include "playlist_columns.h"

#include <QAbstractListModel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QListView>
#include <QToolButton>

#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/index.h>
#include <libaudcore/runtime.h>

const char * const pl_col_names[PL_COLS] = {
    N_("Now Playing"),
    N_("Entry number"),
    N_("Title"),
    N_("Artist"),
    N_("Year"),
    N_("Album"),
    N_("Album artist"),
    N_("Track"),
    N_("Genre"),
    N_("Queue position"),
    N_("Length"),
    N_("File path"),
    N_("File name"),
    N_("Custom title"),
    N_("Bitrate"),
    N_("Comment")
};

int pl_num_cols;
int pl_cols[PL_COLS];
int pl_col_widths[PL_COLS];

const char * const pl_col_keys[PL_COLS] = {
    "playing",
    "number",
    "title",
    "artist",
    "year",
    "album",
    "album-artist",
    "track",
    "genre",
    "queued",
    "length",
    "path",
    "filename",
    "custom",
    "bitrate",
    "comment"
};

const int pl_default_widths[PL_COLS] = {
    25,   // playing
    25,   // entry number
    275,  // title
    175,  // artist
    50,   // year
    175,  // album
    175,  // album artist
    75,   // track
    100,  // genre
    25,   // queue position
    75,   // length
    275,  // path
    275,  // filename
    275,  // custom title
    75,   // bitrate
    275   // comment
};

class ListView;
void transfer (ListView * source);

class ListView : public QListView
{
public:
    ListView (QWidget * parent = Q_NULLPTR);
    ~ListView () {}

    QModelIndexList selected () const { return QListView::selectedIndexes(); }

private:
    void doubleClicked () { transfer (this); }
};

ListView::ListView (QWidget * parent) :
    QListView (parent)
{
    setViewMode (QListView::ListMode);
    setMovement (QListView::Free);
    setAlternatingRowColors (true);
    setSelectionMode (QAbstractItemView::ExtendedSelection);

    connect (this, & QAbstractItemView::doubleClicked, this, & ListView::doubleClicked);
}

ListView * chosen = nullptr, * avail = nullptr;

void pl_col_init ()
{
    pl_num_cols = 0;

    String columns = aud_get_str ("qtui", "playlist_columns");
    Index<String> index = str_list_to_index (columns, " ");

    int count = aud::min (index.len (), (int) PL_COLS);

    for (int c = 0; c < count; c ++)
    {
        const String & column = index[c];

        int i = 0;
        while (i < PL_COLS && strcmp (column, pl_col_keys[i]))
            i ++;

        if (i == PL_COLS)
            break;

        pl_cols[pl_num_cols ++] = i;
    }

    auto widths = str_list_to_index (aud_get_str ("qtui", "column_widths"), ", ");
    int nwidths = aud::min (widths.len (), (int) PL_COLS);

    for (int i = 0; i < nwidths; i ++)
        pl_col_widths[i] = str_to_int (widths[i]);
    for (int i = nwidths; i < PL_COLS; i ++)
        pl_col_widths[i] = pl_default_widths[i];
}

class ListModel : public QAbstractListModel
{
public:
    ListModel (QObject * parent = Q_NULLPTR) : QAbstractListModel (parent) {}
    ~ListModel () { m_data.clear (); }

    int rowCount (const QModelIndex & parent = QModelIndex ()) const { return m_data.len (); }
    int columnCount (const QModelIndex & parent = QModelIndex ()) const { return 1; }

    bool insertRows (int row, int count, const QModelIndex & parent = QModelIndex());
    bool addItem (int data);
    bool removeRows (int row, int count, const QModelIndex & parent = QModelIndex());
    bool removeIndex (const QModelIndex & index);

    bool setData (const QModelIndex & index, const QVariant & value, int role = Qt::UserRole);
    QVariant data (const QModelIndex & index, int role = Qt::DisplayRole) const;

private:
    Index<int> m_data;

    void repopulate ();

    const HookReceiver<ListModel>
        hook {"qtui update playlist columns from ui", this, & ListModel::repopulate};
};

bool ListModel::insertRows (int row, int count, const QModelIndex & parent)
{
    if (parent.isValid () || row < 0 || row > rowCount() || count <= 0)
        return false;

    beginInsertRows (parent, row, row + count);
    m_data.insert (row, count);
    endInsertRows ();

    return true;
}

bool ListModel::addItem (int data)
{
    auto cnt = rowCount ();
    if (! insertRow (cnt))
        return false;

    return setData (index (cnt, 0), data);
}

bool ListModel::removeRows (int row, int count, const QModelIndex & parent)
{
    if (parent.isValid () || row < 0 || row >= rowCount() || count <= 0 || row + count > rowCount ())
        return false;

    beginRemoveRows (parent, row, row + count);
    m_data.remove (row, count);
    endRemoveRows ();

    return true;
}

bool ListModel::removeIndex (const QModelIndex & index)
{
    if (! index.isValid ())
        return false;

    return removeRow (index.row ());
}

bool ListModel::setData (const QModelIndex & index, const QVariant & value, int role)
{
    if (! index.isValid () || role != Qt::UserRole)
        return false;

    m_data[index.row ()] = value.toInt ();
    emit dataChanged (index, index, QVector<int> (Qt::UserRole, Qt::DisplayRole));

    return true;
}

QVariant ListModel::data (const QModelIndex & index, int role) const
{
    if (index.isValid ())
        switch (role)
        {
            case Qt::DisplayRole:
                return QString (pl_col_names[m_data[index.row ()]]);
            case Qt::UserRole:
                return m_data[index.row ()];
        }

    return QVariant ();
}

bool in_populate = false;

void populate_models ()
{
    auto avail_model = dynamic_cast<ListModel *> (avail->model ());
    auto chosen_model = dynamic_cast<ListModel *> (chosen->model ());

    in_populate = true;

    avail_model->removeRows (0, avail_model->rowCount ());
    chosen_model->removeRows (0, chosen_model->rowCount ());

    bool added[PL_COLS] = {};

    for (int i = 0; i < pl_num_cols; i ++)
    {
        if (! added[pl_cols[i]])
        {
            added[pl_cols[i]] = true;
            chosen_model->addItem (pl_cols[i]);
        }
    }

    for (int i = 0; i < PL_COLS; i ++)
    {
        if (! added[i])
            avail_model->addItem (i);
    }

    in_populate = false;
}

void ListModel::repopulate ()
{
    // needed only for one model
    if (chosen->model () == this)
        populate_models ();
}

void apply_changes ()
{
    if (in_populate)
        return;

    int cols = chosen->model ()->rowCount ();

    for (pl_num_cols = 0; pl_num_cols < cols; pl_num_cols ++)
    {
        QModelIndex idx = chosen->model ()->index (pl_num_cols, 0);
        int column = chosen->model ()->data (idx, Qt::UserRole).toInt ();

        pl_cols[pl_num_cols] = column;
    }

    hook_call ("qtui update playlist columns", nullptr);

    pl_col_save ();
}

void transfer (ListView * source)
{
    ListModel * src, * dst;
    if (source == chosen)
    {
        src = dynamic_cast<ListModel *> (chosen->model ());
        dst = dynamic_cast<ListModel *> (avail->model ());
    }
    else
    {
        src = dynamic_cast<ListModel *> (avail->model ());
        dst = dynamic_cast<ListModel *> (chosen->model ());
    }

    auto list = source->selected ();
    source->clearSelection ();
    QList<int> vals;

    while (! list.isEmpty ())
    {
        vals.append(src->data (list.takeFirst (), Qt::UserRole).toInt ());
    }

    while (! vals.isEmpty ())
    {
        auto val = vals.takeFirst ();
        QModelIndexList toremove;

        while (! (toremove = src->match (src->index (0, 0), Qt::UserRole, val, 1, Qt::MatchExactly)).isEmpty ())
        {
            auto item = toremove.takeFirst ();
            if (src->removeIndex (item))
                dst->addItem (val);
        }
    }
}

void add_column ()
{
    transfer (avail);
}

void remove_column ()
{
    transfer (chosen);
}

void destroy_cb ()
{
    chosen = nullptr;
    avail = nullptr;
}

void * pl_col_create_chooser ()
{
    auto widget = new QWidget ();
    QObject::connect(widget, & QObject::destroyed, destroy_cb);

    auto layout = new QHBoxLayout (widget);
    widget->setLayout (layout);

    layout->setContentsMargins (0, 0, 0, 0);
    layout->setSpacing (4);

    avail = new ListView (widget);
    chosen = new ListView (widget);
    auto avail_model = new ListModel (avail);
    auto chosen_model = new ListModel (chosen);
    avail->setModel (avail_model);
    chosen->setModel (chosen_model);

    populate_models ();

    QObject::connect (chosen_model, & QAbstractItemModel::dataChanged, apply_changes);
    QObject::connect (chosen_model, & QAbstractItemModel::rowsRemoved, apply_changes);

    auto tb_right = new QToolButton (widget);
    auto tb_left = new QToolButton (widget);
    tb_right->setArrowType (Qt::RightArrow);
    tb_left->setArrowType (Qt::LeftArrow);
    QObject::connect (tb_right, & QToolButton::clicked, add_column);
    QObject::connect (tb_left, & QToolButton::clicked, remove_column);

    auto layout2 = new QVBoxLayout ();
    layout2->addWidget (new QLabel (_("Available columns"), widget));
    layout2->addWidget (avail);
    layout->addLayout (layout2);

    layout2 = new QVBoxLayout ();
    layout2->addWidget (tb_right);
    layout2->addWidget (tb_left);
    layout->addLayout (layout2);

    layout2 = new QVBoxLayout ();
    layout2->addWidget (new QLabel (_("Displayed columns"), widget));
    layout2->addWidget (chosen);
    layout->addLayout (layout2);

    return widget;
}

void pl_col_save ()
{
    Index<String> index;
    for (int i = 0; i < pl_num_cols; i ++)
        index.append (String (pl_col_keys[pl_cols[i]]));

    aud_set_str ("qtui", "playlist_columns", index_to_str_list (index, " "));
    aud_set_str ("qtui", "column_widths", int_array_to_str (pl_col_widths, PL_COLS));
}
