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

#include <vector>

#include <QAbstractListModel>
#include <QStringList>
#include <QVariant>

#include <libaudcore/hook.h>
#include <libaudcore/playlist.h>
#include <libaudcore/preferences.h>

class PluginHandle;

class MobilePluginModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        NameRole = Qt::UserRole + 1,
        BasenameRole,
        CategoryRole,
        EnabledRole,
        HasPreferencesRole,
        HasAboutRole
    };

    explicit MobilePluginModel(QObject * parent = nullptr);
    ~MobilePluginModel() override;

    int rowCount(const QModelIndex & parent = QModelIndex()) const override;
    QVariant data(const QModelIndex & index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool setEnabled(int row, bool enabled);

private:
    struct Entry
    {
        PluginHandle * plugin;
        QString category;
    };

    static bool pluginChanged(PluginHandle * plugin, void * data);
    void refreshPlugin(PluginHandle * plugin);

    std::vector<Entry> m_entries;
};

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
    ~MobileUiController() override;

    QAbstractItemModel * playlistModel() { return &m_playlist_model; }
    QAbstractItemModel * pluginModel() { return &m_plugin_model; }
    QStringList playlistNames() const { return m_playlist_names; }
    int activePlaylistIndex() const;
    QString playlistTitle() const;

    QString title() const { return m_title; }
    QString artist() const { return m_artist; }
    QString album() const { return m_album; }
    QString albumArt() const { return m_album_art; }
    QString albumArtBackground() const { return m_album_art_background; }

    int playingEntry() const { return m_playing_entry; }
    bool playing() const { return m_playing; }
    bool ready() const { return m_ready; }
    bool paused() const { return m_paused; }
    int position() const { return m_position; }
    int duration() const { return m_duration; }
    int volume() const { return m_volume; }
    bool repeat() const { return m_repeat; }
    bool shuffle() const { return m_shuffle; }
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
    Q_INVOKABLE void openNativePluginPreferences(const QString & basename);
    Q_INVOKABLE QVariantMap pluginPreferenceValues() const;
    Q_INVOKABLE void setPluginPreference(int id, const QVariant & value);
    Q_INVOKABLE void activatePluginPreference(int id);
    Q_INVOKABLE void closePluginPreferences(const QString & basename,
                                            bool apply);

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
    struct PreferenceBinding
    {
        const PreferencesWidget * widget;
        QVariantList choices;
    };

    QVariant preferenceValue(const PreferenceBinding & binding) const;

    void refreshPlaylists(bool reset_model = true);
    void refreshMetadata();
    void refreshPlayback();
    void refreshPosition();
    void refreshVolume();
    void refreshSettings();
    void playlistActivated();
    void playlistUpdated(Playlist::UpdateLevel level);
    void playlistPositionChanged(Playlist playlist);

    MobilePlaylistModel m_playlist_model;
    MobilePluginModel m_plugin_model;
    QStringList m_playlist_names;
    QString m_title;
    QString m_artist;
    QString m_album;
    QString m_album_art;
    QString m_album_art_background;
    int m_playing_entry = -1;
    bool m_playing = false;
    bool m_ready = false;
    bool m_paused = false;
    int m_position = 0;
    int m_duration = 0;
    int m_volume = 0;
    bool m_repeat = false;
    bool m_shuffle = false;

    PluginHandle * m_preferences_plugin = nullptr;
    const PluginPreferences * m_plugin_preferences = nullptr;
    std::vector<PreferenceBinding> m_preference_bindings;

    Timer<MobileUiController> m_timer{TimerRate::Hz4, this,
                                      &MobileUiController::refreshPosition};

    const HookReceiver<MobileUiController> m_activate_hook{
        "playlist activate", this, &MobileUiController::playlistActivated};
    const HookReceiver<MobileUiController, Playlist::UpdateLevel> m_update_hook{
        "playlist update", this, &MobileUiController::playlistUpdated};
    const HookReceiver<MobileUiController, Playlist> m_position_hook{
        "playlist position", this,
        &MobileUiController::playlistPositionChanged};

    const HookReceiver<MobileUiController> m_playback_begin_hook{
        "playback begin", this, &MobileUiController::refreshPlayback};
    const HookReceiver<MobileUiController> m_playback_ready_hook{
        "playback ready", this, &MobileUiController::refreshPlayback};
    const HookReceiver<MobileUiController> m_playback_pause_hook{
        "playback pause", this, &MobileUiController::refreshPlayback};
    const HookReceiver<MobileUiController> m_playback_unpause_hook{
        "playback unpause", this, &MobileUiController::refreshPlayback};
    const HookReceiver<MobileUiController> m_playback_stop_hook{
        "playback stop", this, &MobileUiController::refreshPlayback};
    const HookReceiver<MobileUiController> m_title_hook{
        "title change", this, &MobileUiController::refreshMetadata};
    const HookReceiver<MobileUiController> m_tuple_hook{
        "tuple change", this, &MobileUiController::refreshMetadata};
    const HookReceiver<MobileUiController> m_info_hook{
        "info change", this, &MobileUiController::refreshMetadata};
    const HookReceiver<MobileUiController> m_repeat_hook{
        "set repeat", this, &MobileUiController::refreshSettings};
    const HookReceiver<MobileUiController> m_shuffle_hook{
        "set shuffle", this, &MobileUiController::refreshSettings};
};

#endif
