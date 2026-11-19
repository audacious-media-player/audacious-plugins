/*
 * Copyright 2026 Audacious developers and others
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
 */

#ifndef AUDACIOUS_KIRIGAMI_PLAYLIST_CONTROLLER_H
#define AUDACIOUS_KIRIGAMI_PLAYLIST_CONTROLLER_H

#include <QAbstractListModel>
#include <QStringList>

#include <libaudcore/hook.h>
#include <libaudcore/playlist.h>

class MobilePlaylistModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        TitleRole = Qt::UserRole + 1,
        ArtistRole,
        AlbumRole,
        DurationRole,
        PlayingRole,
        QueuePositionRole
    };

    explicit MobilePlaylistModel(QObject * parent = nullptr);

    int rowCount(const QModelIndex & parent = QModelIndex()) const override;
    QVariant data(const QModelIndex & index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setPlaylist(Playlist playlist);
    void refresh();
    void applyPendingUpdate();
    void refreshPlayback();
    void refreshEntry(int entry);

private:
    Playlist m_playlist;
    int m_rows = 0;
};

class MobilePlaylistController : public QObject
{
    Q_OBJECT

public:
    explicit MobilePlaylistController(QObject * parent = nullptr);

    QAbstractItemModel * model() { return &m_model; }
    QStringList names() const { return m_names; }
    int activeIndex() const;
    QString title() const;

    void playEntry(int entry);
    void toggleQueued(int entry);
    void removeEntry(int entry);
    void activatePlaylist(int index);
    void newPlaylist();
    void refreshPlayback();

signals:
    void playlistsChanged();

private:
    void refreshPlaylists(bool reset_model = true);
    void playlistActivated();
    void playlistUpdated(Playlist::UpdateLevel level);
    void playlistPositionChanged(Playlist playlist);

    MobilePlaylistModel m_model;
    QStringList m_names;

    const HookReceiver<MobilePlaylistController> m_activate_hook{
        "playlist activate", this,
        &MobilePlaylistController::playlistActivated};
    const HookReceiver<MobilePlaylistController, Playlist::UpdateLevel>
        m_update_hook{"playlist update", this,
                      &MobilePlaylistController::playlistUpdated};
    const HookReceiver<MobilePlaylistController, Playlist> m_position_hook{
        "playlist position", this,
        &MobilePlaylistController::playlistPositionChanged};
};

#endif
