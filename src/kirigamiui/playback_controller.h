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

#ifndef AUDACIOUS_KIRIGAMI_PLAYBACK_CONTROLLER_H
#define AUDACIOUS_KIRIGAMI_PLAYBACK_CONTROLLER_H

#include <QObject>
#include <QString>

#include <libaudcore/hook.h>
#include <libaudcore/playlist.h>
#include <libaudcore/runtime.h>

class MobilePlaybackController : public QObject
{
    Q_OBJECT

public:
    explicit MobilePlaybackController(QObject * parent = nullptr);

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

    void playPause();
    void stop();
    void previous();
    void next();
    void seek(int position);
    void setVolume(int volume);
    void setRepeat(bool repeat);
    void setShuffle(bool shuffle);

signals:
    void metadataChanged();
    void playingEntryChanged();
    void playbackChanged();
    void positionChanged();
    void volumeChanged();
    void settingsChanged();

private:
    void refreshMetadata();
    void refreshPlayback();
    void refreshPosition();
    void refreshVolume();
    void refreshSettings();
    void playlistPositionChanged(Playlist playlist);

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

    Timer<MobilePlaybackController> m_timer{
        TimerRate::Hz4, this, &MobilePlaybackController::refreshPosition};

    const HookReceiver<MobilePlaybackController> m_activate_hook{
        "playlist activate", this,
        &MobilePlaybackController::refreshPlayback};
    const HookReceiver<MobilePlaybackController, Playlist> m_position_hook{
        "playlist position", this,
        &MobilePlaybackController::playlistPositionChanged};
    const HookReceiver<MobilePlaybackController> m_playback_begin_hook{
        "playback begin", this, &MobilePlaybackController::refreshPlayback};
    const HookReceiver<MobilePlaybackController> m_playback_ready_hook{
        "playback ready", this, &MobilePlaybackController::refreshPlayback};
    const HookReceiver<MobilePlaybackController> m_playback_pause_hook{
        "playback pause", this, &MobilePlaybackController::refreshPlayback};
    const HookReceiver<MobilePlaybackController> m_playback_unpause_hook{
        "playback unpause", this,
        &MobilePlaybackController::refreshPlayback};
    const HookReceiver<MobilePlaybackController> m_playback_stop_hook{
        "playback stop", this, &MobilePlaybackController::refreshPlayback};
    const HookReceiver<MobilePlaybackController> m_title_hook{
        "title change", this, &MobilePlaybackController::refreshMetadata};
    const HookReceiver<MobilePlaybackController> m_tuple_hook{
        "tuple change", this, &MobilePlaybackController::refreshMetadata};
    const HookReceiver<MobilePlaybackController> m_info_hook{
        "info change", this, &MobilePlaybackController::refreshMetadata};
    const HookReceiver<MobilePlaybackController> m_repeat_hook{
        "set repeat", this, &MobilePlaybackController::refreshSettings};
    const HookReceiver<MobilePlaybackController> m_shuffle_hook{
        "set shuffle", this, &MobilePlaybackController::refreshSettings};
};

#endif
