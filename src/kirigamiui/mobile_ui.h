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

#ifndef AUDACIOUS_KIRIGAMI_MOBILE_UI_H
#define AUDACIOUS_KIRIGAMI_MOBILE_UI_H

#include <QQuickItem>
#include <QStringList>
#include <QVariant>

#include "playback_controller.h"
#include "playlist_controller.h"
#include "plugin_controller.h"
#include "settings_controller.h"
#include "song_info_controller.h"

class MobileUiController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel * playlistModel READ playlistModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel * pluginModel READ pluginModel CONSTANT)
    Q_PROPERTY(
        QStringList playlistNames READ playlistNames NOTIFY playlistsChanged)
    Q_PROPERTY(int activePlaylistIndex READ activePlaylistIndex NOTIFY
                   playlistsChanged)
    Q_PROPERTY(QString playlistTitle READ playlistTitle NOTIFY playlistsChanged)

    Q_PROPERTY(QString title READ title NOTIFY metadataChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY metadataChanged)
    Q_PROPERTY(QString album READ album NOTIFY metadataChanged)
    Q_PROPERTY(QString albumArt READ albumArt NOTIFY metadataChanged)
    Q_PROPERTY(QString albumArtBackground READ albumArtBackground NOTIFY
                   metadataChanged)

    Q_PROPERTY(int playingEntry READ playingEntry NOTIFY playingEntryChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playbackChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY playbackChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY playbackChanged)
    Q_PROPERTY(int position READ position NOTIFY positionChanged)
    Q_PROPERTY(int duration READ duration NOTIFY positionChanged)
    Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(bool repeat READ repeat NOTIFY settingsChanged)
    Q_PROPERTY(bool shuffle READ shuffle NOTIFY settingsChanged)
    Q_PROPERTY(QString applicationVersion READ applicationVersion CONSTANT)
    Q_PROPERTY(QString copyrightText READ copyrightText CONSTANT)

public:
    explicit MobileUiController(QObject * parent = nullptr);

    QAbstractItemModel * playlistModel() { return m_playlists.model(); }
    QAbstractItemModel * pluginModel() { return m_plugins.model(); }
    QStringList playlistNames() const { return m_playlists.names(); }
    int activePlaylistIndex() const { return m_playlists.activeIndex(); }
    QString playlistTitle() const { return m_playlists.title(); }

    QString title() const { return m_playback.title(); }
    QString artist() const { return m_playback.artist(); }
    QString album() const { return m_playback.album(); }
    QString albumArt() const { return m_playback.albumArt(); }
    QString albumArtBackground() const
    {
        return m_playback.albumArtBackground();
    }

    int playingEntry() const { return m_playback.playingEntry(); }
    bool playing() const { return m_playback.playing(); }
    bool ready() const { return m_playback.ready(); }
    bool paused() const { return m_playback.paused(); }
    int position() const { return m_playback.position(); }
    int duration() const { return m_playback.duration(); }
    int volume() const { return m_playback.volume(); }
    bool repeat() const { return m_playback.repeat(); }
    bool shuffle() const { return m_playback.shuffle(); }
    QString applicationVersion() const;
    QString copyrightText() const;

    Q_INVOKABLE void playPause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void next();
    Q_INVOKABLE void seek(int position);
    Q_INVOKABLE void setVolume(int volume);
    Q_INVOKABLE void setRepeat(bool repeat);
    Q_INVOKABLE void setShuffle(bool shuffle);

    Q_INVOKABLE void playEntry(int entry);
    Q_INVOKABLE void toggleQueued(int entry);
    Q_INVOKABLE void removeEntry(int entry);
    Q_INVOKABLE QVariantMap openSongInfo(int entry);
    Q_INVOKABLE bool saveSongInfo(int session, const QVariantMap & values);
    Q_INVOKABLE void closeSongInfo(int session);
    Q_INVOKABLE void activatePlaylist(int index);
    Q_INVOKABLE void newPlaylist();

    Q_INVOKABLE void openFiles();
    Q_INVOKABLE void addFiles();
    Q_INVOKABLE void showPreferences();
    Q_INVOKABLE void hidePreferences();
    Q_INVOKABLE void showAbout();
    Q_INVOKABLE void hideAbout();
    Q_INVOKABLE void requestQuit();

    Q_INVOKABLE bool boolSetting(const QString & name) const;
    Q_INVOKABLE int intSetting(const QString & name) const;
    Q_INVOKABLE void setBoolSetting(const QString & name, bool value);
    Q_INVOKABLE void setIntSetting(const QString & name, int value);

    Q_INVOKABLE bool setPluginEnabled(int row, bool enabled);
    Q_INVOKABLE QVariantMap openPluginPreferences(const QString & basename);
    Q_INVOKABLE QVariantMap pluginPreferenceValues() const;
    Q_INVOKABLE void setPluginPreference(int id, const QVariant & value);
    Q_INVOKABLE void activatePluginPreference(int id);
    Q_INVOKABLE void closePluginPreferences(const QString & basename,
                                            bool apply);
    Q_INVOKABLE QQuickItem * createNativePreferenceItem(QQuickItem * parent,
                                                        int id);

signals:
    void playlistsChanged();
    void metadataChanged();
    void playingEntryChanged();
    void playbackChanged();
    void positionChanged();
    void volumeChanged();
    void settingsChanged();
    void preferencesRequested();
    void preferencesDismissed();
    void aboutRequested();
    void aboutDismissed();

private:
    MobilePlaylistController m_playlists;
    MobilePlaybackController m_playback;
    MobilePluginController m_plugins;
    MobileSongInfoController m_song_info;
    MobileSettingsController m_settings;
};

#endif
