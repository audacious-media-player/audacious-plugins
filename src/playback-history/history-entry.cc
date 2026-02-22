/*
 * history-entry.cc
 * Copyright (C) 2023-2024 Igor Kushnir <igorkuo@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <cassert>

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>

#include "history-entry.h"
#include "preferences.h"

/**
 * Returns a representation of @p str suitable for printing via audlog::log().
 */
static const char * printable(const String & str)
{
    // Printing nullptr invokes undefined behavior.
    return str ? str : "";
}

bool HistoryEntry::assignPlayingEntry()
{
    m_playlist = Playlist::playing_playlist();
    if (!m_playlist.exists())
    {
        AUDWARN("Playback just started but no playlist is playing.\n");
        return false;
    }

    m_playlistPosition = m_playlist.get_position();
    if (m_playlistPosition == -1)
    {
        AUDWARN("Playback just started but the playing playlist %s "
                "has no playing entry.\n",
                printable(playlistTitle()));
        return false;
    }
    assert(m_playlistPosition >= 0);
    assert(m_playlistPosition < m_playlist.n_entries());

    int entryType = aud_get_int(configSection, configEntryType);
    if (entryType == static_cast<int>(Type::Song) ||
        entryType == static_cast<int>(Type::Album))
    {
        m_type = static_cast<Type>(entryType);
    }
    else
    {
        AUDWARN("Invalid %s.%s config value: %d.\n", configSection,
                configEntryType, entryType);
        m_type = defaultType;
    }

    return retrieveText(m_text);
}

void HistoryEntry::makeCurrent() const
{
    if (!isAvailable())
        return;

    m_playlist.select_all(false);
    m_playlist.select_entry(m_playlistPosition, true);
    m_playlist.set_focus(m_playlistPosition);
    m_playlist.activate();
}

bool HistoryEntry::play() const
{
    if (!isAvailable())
        return false;

    m_playlist.set_position(m_playlistPosition);
    m_playlist.start_playback();

    // Double-clicking a history entry makes it current just before activation.
    // In this case m_playlist is already active here. However, m_playlist is
    // not active here if the user performs the following steps:
    // 1) select a history entry;
    // 2) activate another playlist;
    // 3) give focus to History view;
    // 4) press the Enter key.
    m_playlist.activate();

    return true;
}

void HistoryEntry::debugPrint(const char * prefix) const
{
    AUDDBG("%s%s=\"%s\", playlist=\"%s\", entry number=%d\n", prefix,
           untranslatedTextDesignation(), printable(m_text),
           printable(playlistTitle()), entryNumber());
}

int HistoryEntry::entryNumber() const
{
    // Add 1 because a playlist position is 0-based
    // but an Entry Number in the UI is 1-based.
    return m_playlistPosition + 1;
}

const char * HistoryEntry::translatedTextDesignation() const
{
    return (m_type == Type::Album) ? _("Album") : _("Title");
}

const char * HistoryEntry::untranslatedTextDesignation() const
{
    return (m_type == Type::Album) ? "album" : "title";
}

bool HistoryEntry::retrieveText(String & text) const
{
    String errorMessage;
    const Tuple tuple = m_playlist.entry_tuple(m_playlistPosition,
                                               Playlist::Wait, &errorMessage);
    if (errorMessage || tuple.state() != Tuple::Valid)
    {
        AUDWARN("Failed to retrieve metadata of entry #%d in playlist %s: %s\n",
                entryNumber(), printable(playlistTitle()),
                errorMessage ? printable(errorMessage)
                             : "Song info could not be read");
        return false;
    }

    text = tuple.get_str(static_cast<Tuple::Field>(m_type));
    return true;
}

bool HistoryEntry::isAvailable() const
{
    if (!m_playlist.exists())
    {
        AUDWARN("The selected entry's playlist has been deleted.\n");
        return false;
    }

    assert(m_playlistPosition >= 0);
    if (m_playlistPosition >= m_playlist.n_entries())
    {
        AUDWARN("The selected entry's position is now out of bounds.\n");
        return false;
    }

    String currentTextAtPlaylistPosition;
    if (!retrieveText(currentTextAtPlaylistPosition))
        return false;

    // Text equality does not guarantee that the song, for which this history
    // entry was created, still resides at m_playlistPosition in m_playlist. In
    // case the user inserts or removes a few songs above m_playlistPosition:
    // * if type() is Song, a different song with the same title is unnoticed;
    // * if type() is Album, a different song from the same album or a song from
    //   an unrelated album that happens to have the same name goes undetected.
    // But such coincidences should be much more rare and less of a problem than
    // the text inequality condition checked here. Therefore, information that
    // uniquely identifies the referenced song is not stored in a history entry
    // just for this case.
    if (currentTextAtPlaylistPosition != m_text)
    {
        AUDWARN(
            "The %s at the selected entry's playlist position has changed.\n",
            untranslatedTextDesignation());
        return false;
    }

    return true;
}

bool HistoryEntry::shouldAppendEntry(const HistoryEntry & previousEntry) const
{
    if (previousEntry.type() != type() ||
        previousEntry.playlist() != playlist())
    {
        return true; // the two entries are very different indeed
    }

    // When the entry numbers differ, either the entries point to two
    // different songs or the user has modified the playlist and playlist
    // positions have shifted, which invalidated previousEntry. Either
    // way, the entry should be appended if the type is Song.
    if (type() == HistoryEntry::Type::Song &&
        previousEntry.entryNumber() != entryNumber())
    {
        return true;
    }

    // If the type is Song, equal entry numbers but differing song titles
    // mean that the user has modified the playlist and playlist positions
    // have shifted. Append the entry in this case, because it is not the
    // same as the previous one.
    // If the type is Album, let us assume the Shuffle by Album playback
    // mode is enabled. Then equal album names, in all probability,
    // mean that a song from the same album started playing, in which case
    // the entry should not be appended. Much less likely, a different
    // album with the same name, separated by other albums from the
    // previously played one, was just randomly selected.
    // Ignore this possibility here. The users concerned about such an
    // occurrence are advised to edit metadata in their music collections
    // and ensure unique album names. The unique album names would also
    // prevent Audacious from erroneously playing same-name albums
    // in order if they happen to end up adjacent in a playlist.
    return previousEntry.text() != text();
}
