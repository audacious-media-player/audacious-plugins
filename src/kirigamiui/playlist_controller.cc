/*
 * Copyright 2026 Audacious developers and others
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "playlist_controller.h"

#include <libaudcore/audstrings.h>
#include <libaudcore/tuple.h>

namespace
{

QString toQString(const String & string)
{
    return string ? QString::fromUtf8((const char *)string) : QString();
}

QString entryTitle(Playlist playlist, int entry, const Tuple & tuple)
{
    auto title = tuple.get_str(Tuple::Title);
    if (!title)
        title = tuple.get_str(Tuple::FormattedTitle);
    if (title)
        return QString::fromUtf8((const char *)title);

    auto filename = playlist.entry_filename(entry);
    auto basename = uri_get_display_base(filename);
    return QString::fromUtf8((const char *)basename);
}

} // namespace

MobilePlaylistModel::MobilePlaylistModel(QObject * parent)
    : QAbstractListModel(parent), m_playlist(Playlist::active_playlist()),
      m_rows(m_playlist.n_entries())
{
}

int MobilePlaylistModel::rowCount(const QModelIndex & parent) const
{
    return parent.isValid() || !m_playlist.exists() ? 0 : m_rows;
}

QVariant MobilePlaylistModel::data(const QModelIndex & index, int role) const
{
    if (!index.isValid() || !m_playlist.exists() || index.row() < 0 ||
        index.row() >= m_playlist.n_entries())
        return {};

    const int entry = index.row();
    const Tuple tuple = m_playlist.entry_tuple(entry, Playlist::NoWait);

    switch (role)
    {
    case TitleRole:
        return entryTitle(m_playlist, entry, tuple);
    case ArtistRole:
        return toQString(tuple.get_str(Tuple::Artist));
    case AlbumRole:
        return toQString(tuple.get_str(Tuple::Album));
    case DurationRole:
    {
        const int length = tuple.get_int(Tuple::Length);
        if (length < 0)
            return QStringLiteral("--:--");

        auto duration = str_format_time(length);
        return QString::fromUtf8((const char *)duration);
    }
    case PlayingRole:
        return m_playlist == Playlist::playing_playlist() &&
               entry == m_playlist.get_position();
    case QueuePositionRole:
    {
        const int queue_position = m_playlist.queue_find_entry(entry);
        return queue_position < 0 ? 0 : queue_position + 1;
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> MobilePlaylistModel::roleNames() const
{
    return {{TitleRole, "title"},     {ArtistRole, "artist"},
            {AlbumRole, "album"},     {DurationRole, "duration"},
            {PlayingRole, "playing"}, {QueuePositionRole, "queuePosition"}};
}

void MobilePlaylistModel::setPlaylist(Playlist playlist)
{
    if (playlist == m_playlist)
        return;

    beginResetModel();
    m_playlist = playlist;
    m_rows = m_playlist.exists() ? m_playlist.n_entries() : 0;
    endResetModel();
}

void MobilePlaylistModel::refresh()
{
    beginResetModel();
    m_rows = m_playlist.exists() ? m_playlist.n_entries() : 0;
    endResetModel();
}

void MobilePlaylistModel::applyPendingUpdate()
{
    if (!m_playlist.exists())
        return;

    const auto update = m_playlist.update_detail();
    if (update.level == Playlist::NoUpdate)
        return;

    const int rows = m_playlist.n_entries();
    const int changed = rows - update.before - update.after;

    if (update.level == Playlist::Structure)
    {
        const int removed = m_rows - update.before - update.after;
        if (removed > 0)
        {
            beginRemoveRows({}, update.before, update.before + removed - 1);
            m_rows -= removed;
            endRemoveRows();
        }

        if (changed > 0)
        {
            beginInsertRows({}, update.before, update.before + changed - 1);
            m_rows += changed;
            endInsertRows();
        }

        if (m_rows != rows)
            refresh();
    }
    else if (update.level == Playlist::Metadata && changed > 0)
    {
        emit dataChanged(index(update.before),
                         index(update.before + changed - 1));
    }

    if (update.queue_changed && m_rows > 0)
        emit dataChanged(index(0), index(m_rows - 1), {QueuePositionRole});
}

void MobilePlaylistModel::refreshPlayback()
{
    if (rowCount() > 0)
        emit dataChanged(index(0), index(rowCount() - 1), {PlayingRole});
}

void MobilePlaylistModel::refreshEntry(int entry)
{
    if (entry >= 0 && entry < rowCount())
        emit dataChanged(index(entry), index(entry));
}

MobilePlaylistController::MobilePlaylistController(QObject * parent)
    : QObject(parent), m_model(this)
{
    refreshPlaylists();
}

int MobilePlaylistController::activeIndex() const
{
    return Playlist::active_playlist().index();
}

QString MobilePlaylistController::title() const
{
    return toQString(Playlist::active_playlist().get_title());
}

void MobilePlaylistController::playEntry(int entry)
{
    auto playlist = Playlist::active_playlist();
    if (entry < 0 || entry >= playlist.n_entries())
        return;

    playlist.set_position(entry);
    playlist.start_playback();
}

void MobilePlaylistController::toggleQueued(int entry)
{
    auto playlist = Playlist::active_playlist();
    if (entry < 0 || entry >= playlist.n_entries())
        return;

    const int queue_position = playlist.queue_find_entry(entry);
    if (queue_position >= 0)
        playlist.queue_remove(queue_position);
    else
        playlist.queue_insert(-1, entry);

    m_model.refreshEntry(entry);
}

void MobilePlaylistController::removeEntry(int entry)
{
    auto playlist = Playlist::active_playlist();
    if (entry >= 0 && entry < playlist.n_entries())
        playlist.remove_entry(entry);
}

void MobilePlaylistController::activatePlaylist(int index)
{
    if (index >= 0 && index < Playlist::n_playlists())
        Playlist::by_index(index).activate();
}

void MobilePlaylistController::newPlaylist() { Playlist::new_playlist(); }

void MobilePlaylistController::refreshPlayback()
{
    m_model.refreshPlayback();
}

void MobilePlaylistController::refreshPlaylists(bool reset_model)
{
    QStringList names;
    for (int i = 0; i < Playlist::n_playlists(); i++)
        names.append(toQString(Playlist::by_index(i).get_title()));

    if (reset_model)
        m_model.setPlaylist(Playlist::active_playlist());

    if (names != m_names || reset_model)
    {
        m_names = names;
        emit playlistsChanged();
    }
}

void MobilePlaylistController::playlistActivated()
{
    refreshPlaylists();
    refreshPlayback();
}

void MobilePlaylistController::playlistUpdated(Playlist::UpdateLevel level)
{
    m_model.applyPendingUpdate();
    if (level >= Playlist::Metadata)
        refreshPlaylists(false);
}

void MobilePlaylistController::playlistPositionChanged(Playlist playlist)
{
    Q_UNUSED(playlist)
    refreshPlayback();
}
