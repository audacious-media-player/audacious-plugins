/*
 * Copyright 2026 Audacious developers and others
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "mobile_ui.h"

#include <libaudcore/hook.h>
#include <libaudcore/runtime.h>
#include <libaudqt/libaudqt.h>

MobileUiController::MobileUiController(QObject * parent)
    : QObject(parent), m_about(this), m_playlists(this), m_playback(this),
      m_plugins(this), m_song_info(this), m_settings(this)
{
    connect(&m_playlists, &MobilePlaylistController::playlistsChanged, this,
            &MobileUiController::playlistsChanged);
    connect(&m_playback, &MobilePlaybackController::metadataChanged, this,
            &MobileUiController::metadataChanged);
    connect(&m_playback, &MobilePlaybackController::playingEntryChanged, this,
            &MobileUiController::playingEntryChanged);
    connect(&m_playback, &MobilePlaybackController::playbackChanged, this,
            &MobileUiController::playbackChanged);
    connect(&m_playback, &MobilePlaybackController::positionChanged, this,
            &MobileUiController::positionChanged);
    connect(&m_playback, &MobilePlaybackController::volumeChanged, this,
            &MobileUiController::volumeChanged);
    connect(&m_playback, &MobilePlaybackController::settingsChanged, this,
            &MobileUiController::settingsChanged);

    connect(&m_playback, &MobilePlaybackController::playbackChanged,
            &m_playlists, &MobilePlaylistController::refreshPlayback);
}

void MobileUiController::playPause() { m_playback.playPause(); }

void MobileUiController::stop() { m_playback.stop(); }

void MobileUiController::previous() { m_playback.previous(); }

void MobileUiController::next() { m_playback.next(); }

void MobileUiController::seek(int position) { m_playback.seek(position); }

void MobileUiController::setVolume(int volume) { m_playback.setVolume(volume); }

void MobileUiController::setRepeat(bool repeat)
{
    m_playback.setRepeat(repeat);
}

void MobileUiController::setShuffle(bool shuffle)
{
    m_playback.setShuffle(shuffle);
}

void MobileUiController::playEntry(int entry) { m_playlists.playEntry(entry); }

void MobileUiController::toggleQueued(int entry)
{
    m_playlists.toggleQueued(entry);
}

void MobileUiController::removeEntry(int entry)
{
    m_playlists.removeEntry(entry);
}

QVariantMap MobileUiController::openSongInfo(int entry)
{
    return m_song_info.open(entry);
}

bool MobileUiController::saveSongInfo(int session, const QVariantMap & values)
{
    return m_song_info.save(session, values);
}

void MobileUiController::closeSongInfo(int session)
{
    m_song_info.close(session);
}

void MobileUiController::activatePlaylist(int index)
{
    m_playlists.activatePlaylist(index);
}

void MobileUiController::newPlaylist() { m_playlists.newPlaylist(); }

void MobileUiController::openFiles()
{
    audqt::fileopener_show(audqt::FileMode::Open);
}

void MobileUiController::addFiles()
{
    audqt::fileopener_show(audqt::FileMode::Add);
}

void MobileUiController::showPreferences() { emit preferencesRequested(); }

void MobileUiController::hidePreferences() { emit preferencesDismissed(); }

void MobileUiController::showAbout() { emit aboutRequested(); }

void MobileUiController::hideAbout() { emit aboutDismissed(); }

void MobileUiController::requestQuit()
{
    bool handled = false;
    hook_call("window close", &handled);
    if (!handled)
        aud_quit();
}

bool MobileUiController::boolSetting(const QString & name) const
{
    return m_settings.boolValue(name);
}

int MobileUiController::intSetting(const QString & name) const
{
    return m_settings.intValue(name);
}

void MobileUiController::setBoolSetting(const QString & name, bool value)
{
    m_settings.setBoolValue(name, value);
}

void MobileUiController::setIntSetting(const QString & name, int value)
{
    m_settings.setIntValue(name, value);
}

bool MobileUiController::setPluginEnabled(int row, bool enabled)
{
    return m_plugins.setEnabled(row, enabled);
}

QVariantMap
MobileUiController::openPluginPreferences(const QString & basename)
{
    return m_plugins.openPreferences(basename);
}

QVariantMap MobileUiController::pluginPreferenceValues() const
{
    return m_plugins.preferenceValues();
}

void MobileUiController::setPluginPreference(int id, const QVariant & value)
{
    m_plugins.setPreference(id, value);
}

void MobileUiController::activatePluginPreference(int id)
{
    m_plugins.activatePreference(id);
}

void MobileUiController::closePluginPreferences(const QString & basename,
                                                bool apply)
{
    m_plugins.closePreferences(basename, apply);
}

QQuickItem *
MobileUiController::createNativePreferenceItem(QQuickItem * parent, int id)
{
    return m_plugins.createNativePreferenceItem(parent, id);
}
