/*
 * playlist_model.cc
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

#include <QApplication>
#include <QIcon>
#include <QMimeData>
#include <QUrl>

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudqt/libaudqt.h>

#include "playlist_model.h"

const char * const PlaylistModel::labels[] = {
    N_("Entry Number"),   N_("Title"),        N_("Artist"),    N_("Year"),
    N_("Album"),          N_("Album Artist"), N_("Track"),     N_("Genre"),
    N_("Queue Position"), N_("Length"),       N_("File Path"), N_("File Name"),
    N_("Custom Title"),   N_("Bitrate"),      N_("Comment"),   N_("Publisher"),
    N_("Catalog Number")};

static const Tuple::Field s_fields[] = {
    Tuple::Invalid,        Tuple::Title,       Tuple::Artist, Tuple::Year,
    Tuple::Album,          Tuple::AlbumArtist, Tuple::Track,  Tuple::Genre,
    Tuple::Invalid,        Tuple::Length,      Tuple::Path,   Tuple::Basename,
    Tuple::FormattedTitle, Tuple::Bitrate,     Tuple::Comment, Tuple::Publisher,
    Tuple::CatalogNum};

static_assert(aud::n_elems(PlaylistModel::labels) == PlaylistModel::n_cols,
              "update PlaylistModel::labels");
static_assert(aud::n_elems(s_fields) == PlaylistModel::n_cols,
              "update s_fields");

PlaylistModel::PlaylistModel(QObject * parent, Playlist playlist)
    : QAbstractListModel(parent), m_playlist(playlist),
      m_rows(playlist.n_entries())
{
}

int PlaylistModel::rowCount(const QModelIndex & parent) const
{
    return parent.isValid() ? 0 : m_rows;
}

int PlaylistModel::columnCount(const QModelIndex & parent) const
{
    return 1 + n_cols;
}

void PlaylistModel::setFont(const QFont & font)
{
    m_bold = font;
    m_bold.setBold(true);
}

void PlaylistModel::setPlayingCol(int playing_col)
{
    m_playing_col = playing_col;
    entriesChanged(0, m_rows);
}

QVariant PlaylistModel::alignment(int col) const
{
    switch (col)
    {
    case Length:
        return static_cast<Qt::Alignment::Int>(Qt::AlignRight |
                                               Qt::AlignVCenter);
    default:
        return static_cast<Qt::Alignment::Int>(Qt::AlignLeft |
                                               Qt::AlignVCenter);
    }
}

QVariant PlaylistModel::data(const QModelIndex & index, int role) const
{
    int col = index.column() - 1;
    if (col < 0 || col >= n_cols)
        return QVariant();

    Tuple tuple;
    int val = -1;

    switch (role)
    {
    case Qt::DisplayRole:
        if (s_fields[col] != Tuple::Invalid)
        {
            tuple = m_playlist.entry_tuple(index.row(), Playlist::NoWait);

            switch (tuple.get_value_type(s_fields[col]))
            {
            case Tuple::Empty:
                return QVariant();
            case Tuple::String:
                return QString(tuple.get_str(s_fields[col]));
            case Tuple::Int:
                val = tuple.get_int(s_fields[col]);
                break;
            }
        }

        switch (col)
        {
        case EntryNumber:
            return QString("%1").arg(index.row() + 1);
        case QueuePos:
            return queuePos(index.row());
        case Length:
            return QString(str_format_time(val));
        case Bitrate:
            return QString("%1 kbit/s").arg(val);
        default:
            return QString("%1").arg(val);
        }

    case Qt::FontRole:
        if (index.row() == m_playlist.get_position())
            return m_bold;
        break;

    case Qt::TextAlignmentRole:
        return alignment(col);

    case Qt::DecorationRole:
        if (col == m_playing_col && index.row() == m_playlist.get_position())
        {
            const char * icon_name = "media-playback-stop";

            if (m_playlist == Playlist::playing_playlist())
                icon_name = aud_drct_get_paused() ? "media-playback-pause"
                                                  : "media-playback-start";

            return QIcon::fromTheme(icon_name);
        }
        else if (col == m_playing_col)
        {
            // Reserve space for the icon in other rows so that row
            // heights don't change at song change. Undocumented but
            // longstanding Qt behavior is that any isValid(),
            // non-isNull() variant in the DecorationRole will set the
            // QStyleOptionViewItem::HasDecoration feature, which will
            // generally cause the QStyle to reserve space for an icon.
            // See QStyledItemDelegate::initStyleOption() and
            // QCommonStylePrivate::viewItemSize().
            return QVariant(true);
        }
        break;
    }
    return QVariant();
}

QVariant PlaylistModel::headerData(int section, Qt::Orientation orientation,
                                   int role) const
{
    if (orientation != Qt::Horizontal)
        return QVariant();

    int col = section - 1;
    if (col < 0 || col >= n_cols)
        return QVariant();

    switch (role)
    {
    case Qt::DisplayRole:
        switch (col)
        {
        case EntryNumber:
            return QString("#");
        case QueuePos:
            return QString(_("Q#"));
        case Track:
            return QString(_("T#"));
        case CatalogNum:
            return QString(_("C#"));
        }

        return QString(_(labels[col]));

    case Qt::TextAlignmentRole:
        return alignment(col);

    default:
        return QVariant();
    }
}

Qt::ItemFlags PlaylistModel::flags(const QModelIndex & index) const
{
    if (index.isValid())
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    else
        return Qt::ItemIsSelectable | Qt::ItemIsDropEnabled | Qt::ItemIsEnabled;
}

QStringList PlaylistModel::mimeTypes() const
{
    return QStringList("text/uri-list");
}

QMimeData * PlaylistModel::mimeData(const QModelIndexList & indexes) const
{
    /* we assume that <indexes> contains the selected entries */
    m_playlist.cache_selected();

    QList<QUrl> urls;
    int prev = -1;

    for (auto & index : indexes)
    {
        int row = index.row();
        if (row != prev) /* skip multiple cells in same row */
        {
            urls.append(QString(m_playlist.entry_filename(row)));
            prev = row;
        }
    }

    auto data = new QMimeData;
    data->setUrls(urls);
    return data;
}

bool PlaylistModel::dropMimeData(const QMimeData * data, Qt::DropAction action,
                                 int row, int column,
                                 const QModelIndex & parent)
{
    if (action != Qt::CopyAction || !data->hasUrls())
        return false;

    Index<PlaylistAddItem> items;
    for (auto & url : data->urls())
        items.append(String(url.toEncoded()));

    m_playlist.insert_items(row, std::move(items), false);
    return true;
}

void PlaylistModel::entriesAdded(int row, int count)
{
    if (count < 1)
        return;

    int last = row + count - 1;
    beginInsertRows(QModelIndex(), row, last);
    m_rows += count;
    endInsertRows();
}

void PlaylistModel::entriesRemoved(int row, int count)
{
    if (count < 1)
        return;

    int last = row + count - 1;
    beginRemoveRows(QModelIndex(), row, last);
    m_rows -= count;
    endRemoveRows();
}

void PlaylistModel::entriesChanged(int row, int count)
{
    if (count < 1)
        return;

    int bottom = row + count - 1;
    auto topLeft = createIndex(row, 0);
    auto bottomRight = createIndex(bottom, columnCount() - 1);
    emit dataChanged(topLeft, bottomRight);
}

QString PlaylistModel::queuePos(int row) const
{
    int at = m_playlist.queue_find_entry(row);
    if (at < 0)
        return QString();
    else
        return QString("#%1").arg(at + 1);
}

/* ---------------------------------- */

void PlaylistProxyModel::setFilter(const char * filter)
{
    m_searchTerms = str_list_to_index(filter, " ");
    invalidateFilter();
}

bool PlaylistProxyModel::filterAcceptsRow(int source_row,
                                          const QModelIndex &) const
{
    if (!m_searchTerms.len())
        return true;

    Tuple tuple = m_playlist.entry_tuple(source_row);

    String strings[] = {tuple.get_str(Tuple::Title),
                        tuple.get_str(Tuple::Artist),
                        tuple.get_str(Tuple::Album),
                        tuple.get_str(Tuple::Basename)};

    for (auto & term : m_searchTerms)
    {
        bool found = false;

        for (auto & s : strings)
        {
            if (s && strstr_nocase_utf8(s, term))
            {
                found = true;
                break;
            }
        }

        if (!found)
            return false;
    }

    return true;
}
