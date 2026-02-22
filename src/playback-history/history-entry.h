/*
 * history-entry.h
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

#ifndef AUDACIOUS_PLAYBACK_HISTORY_ENTRY_H
#define AUDACIOUS_PLAYBACK_HISTORY_ENTRY_H

#include <libaudcore/objects.h>
#include <libaudcore/playlist.h>
#include <libaudcore/tuple.h>

class HistoryEntry
{
public:
    enum class Type
    {
        Song = Tuple::Field::Title,
        Album = Tuple::Field::Album
    };
    static constexpr Type defaultType = Type::Song;

    /**
     * Creates an invalid entry that can only be assigned to or destroyed.
     */
    HistoryEntry() = default;

    /**
     * Stores the currently playing entry in @c this.
     *
     * Call this function when a song playback starts.
     * @return @c true if the entry was retrieved and assigned successfully.
     * @note @c this remains or becomes invalid if @c false is returned.
     */
    bool assignPlayingEntry();

    /**
     * Gives keyboard focus to the corresponding playlist entry and makes it the
     * single selected entry in the playlist.
     */
    void makeCurrent() const;

    /**
     * Starts playing this entry.
     *
     * @return @c true in case of success.
     */
    bool play() const;

    /**
     * Prints @c this using @c AUDDBG.
     *
     * @param prefix a possibly empty string to be printed before @c this->
     *               A non-empty @p prefix should end with whitespace.
     */
    void debugPrint(const char * prefix) const;

    Type type() const { return m_type; }

    /**
     * Returns a translated human-readable designation of text() based on
     * type().
     */
    const char * translatedTextDesignation() const;

    /**
     * Returns this entry's song title if type() is Song or its album name if
     * type() is Album.
     */
    const String & text() const { return m_text; }

    Playlist playlist() const { return m_playlist; }
    String playlistTitle() const { return m_playlist.get_title(); }

    /** Returns the playlist Entry Number of this entry. */
    int entryNumber() const;

    /**
     * Retrieves the song title if type() is Song or the album name if type() is
     * Album at @a m_playlistPosition in @a m_playlist and assigns it to
     * @p text.
     *
     * @return @c true if the text was retrieved successfully.
     */
    bool retrieveText(String & text) const;

    /**
     * Checks whether to add the entry to the history, primarily depending on
     * the given previously playing entry.
     */
    bool shouldAppendEntry(const HistoryEntry & previousEntry) const;

private:
    String m_text;
    Type m_type = defaultType;
    Playlist m_playlist; /**< the playlist, in which this entry was played */

    /**
     * Returns an untranslated human-readable designation of text() based on
     * type().
     */
    const char * untranslatedTextDesignation() const;

    /**
     * Returns @c true if @a m_playlist exists and @a m_playlistPosition still
     * points to the same playlist entry as at the time of last assignment.
     */
    bool isAvailable() const;

    /**
     * The position in @a m_playlist of the song titled text() if type() is
     * Song or of the first played song from the album named text() if type() is
     * Album.
     * When the user modifies the playlist, positions shift, but this should
     * happen rarely enough and therefore doesn't have to be handled perfectly.
     * A linear search for a song title or an album name equal to text() in
     * @a m_playlist is inefficient, and thus is never performed by this plugin.
     */
    int m_playlistPosition = -1;
};

#endif // AUDACIOUS_PLAYBACK_HISTORY_ENTRY_H
