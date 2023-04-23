/*
 * playback-history.cc
 * Copyright (C) 2023 Igor Kushnir <igorkuo@gmail.com>
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
#include <utility>

#include <QAbstractListModel>
#include <QEvent>
#include <QFont>
#include <QPointer>

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>
#include <libaudqt/treeview.h>

/**
 * Returns a representation of @p str suitable for printing via audlog::log().
 */
static const char * printable(const String & str)
{
    // Printing nullptr invokes undefined behavior.
    return str ? str : "";
}

static QString toQString(const String & str)
{
    // The explicit cast to const char * is necessary to compile with Qt 6,
    // because otherwise two implicit conversions would be necessary: from
    // String to const char * and from const char * to QByteArrayView.
    return QString::fromUtf8(static_cast<const char *>(str));
}

class PlaybackHistory : public GeneralPlugin
{
private:
    static constexpr const char * aboutText =
        N_("Playback History Plugin\n\n"
           "Copyright 2023 Igor Kushnir <igorkuo@gmail.com>\n\n"
           "This plugin tracks and provides access to playback history.\n\n"

           "History entries are stored only in memory and are lost\n"
           "on Audacious exit. When the plugin is disabled,\n"
           "playback history is not tracked at all.\n"
           "History entries are only added, never removed.\n"
           "The user can restart Audacious or close Playback History\n"
           "view to disable the plugin and clear the entries.\n\n"

           "Currently the playback history is actually an album history,\n"
           "designed and tailored for the users of Shuffle by Album mode.\n"
           "Extending the plugin to optionally track and display\n"
           "individual song history could be a useful future development.\n"
           "Feel free to contribute this feature.");

public:
    static constexpr PluginInfo info = {N_("Playback History"), PACKAGE,
                                        aboutText,
                                        nullptr, // prefs
                                        PluginQtOnly};

    constexpr PlaybackHistory() : GeneralPlugin(info, false) {}

    void * get_qt_widget() override;
    int take_message(const char * code, const void *, int) override;
};

EXPORT PlaybackHistory aud_plugin_instance;

class HistoryEntry
{
public:
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
     * Starts playing this entry.
     *
     * @return @c true in case of success.
     */
    bool play() const;

    /**
     * Prints @c this using @c AUDDBG.
     *
     * @param prefix a possibly empty string to be printed before @c this.
     *               A nonempty @p prefix should end with whitespace.
     */
    void debugPrint(const char * prefix) const;

    const String & album() const { return m_album; }
    String playlistTitle() const { return m_playlist.get_title(); }
    /** Returns the playlist Entry Number of this entry. */
    int entryNumber() const;

private:
    /**
     * Retrieves the album at @a m_playlistPosition in @a m_playlist and assigns
     * it to @p album.
     *
     * @return @c true if the album was retrieved successfully.
     */
    bool retrieveAlbum(String & album) const;

    String m_album;
    Playlist m_playlist; /**< the playlist, in which this entry was played */
    /** The position in @a m_playlist of the first played song from @a m_album.
     * When the user modifies the playlist, positions shift, but this should
     * happen rarely enough and therefore doesn't have to be handled perfectly.
     * A linear search for @a m_album in @a m_playlist is inefficient,
     * and thus is never performed by this plugin. */
    int m_playlistPosition = -1;
};

class HistoryModel : public QAbstractListModel
{
public:
    /**
     * Updates a cached font.
     *
     * Pass the view's font to this function when the view is created and
     * whenever its font changes.
     */
    void setFont(const QFont & font);

    /**
     * Plays the song of the item at @p index.
     *
     * Call this function when the user activates the item at @p index in the
     * view.
     */
    void activate(const QModelIndex & index);

    int rowCount(const QModelIndex & parent) const override;
    int columnCount(const QModelIndex & parent) const override;
    QVariant data(const QModelIndex & index, int role) const override;

private:
    enum
    {
        /** The column name is general, because a song title might optionally be
         * displayed in place of an album title in the future. */
        ColumnText,
        NColumns
    };

    // In this class the word "position" refers to an index into m_entries.

    bool isOutOfBounds(const QModelIndex & index) const;
    int modelRowFromPosition(int position) const;
    int positionFromIndex(const QModelIndex & index) const;

    /**
     * Updates the font of the item that corresponds to @p position.
     *
     * Call this function twice right after modifying @a m_playingPosition: pass
     * the previous and the current value of @a m_playingPosition to it.
     */
    void updateFontForPosition(int position);

    /**
     * Adds the newly played song into the playback history.
     */
    void playbackStarted();

    const HookReceiver<HistoryModel> activate_hook{
        "playback ready", this, &HistoryModel::playbackStarted};

    Index<HistoryEntry> m_entries;
    /** the position of the entry that is
     * currently playing or was played last */
    int m_playingPosition = -1;
    /** a cached font used to highlight the item that is currently playing */
    QFont m_currentlyPlaingFont;
};

class HistoryView : public audqt::TreeView
{
public:
    HistoryView();

protected:
    void changeEvent(QEvent * event) override;

private:
    HistoryModel m_model;
};

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

    return retrieveAlbum(m_album);
}

bool HistoryEntry::play() const
{
    if (!m_playlist.exists())
    {
        AUDWARN("The activated entry's playlist has been deleted.\n");
        return false;
    }

    assert(m_playlistPosition >= 0);
    if (m_playlistPosition >= m_playlist.n_entries())
    {
        AUDWARN("The activated entry's position is now out of bounds.\n");
        return false;
    }

    String currentAlbumAtPlaylistPosition;
    if (!retrieveAlbum(currentAlbumAtPlaylistPosition))
        return false;

    // This check does not guarantee that the first played song from m_album
    // still resides at m_playlistPosition in m_playlist. In case the user
    // inserts or removes a few songs above m_playlistPosition, a different song
    // from the same album or a song from an unrelated album that happens to
    // have the same name goes undetected. But such coincidences should be much
    // more rare and less of a problem than the album inequality condition
    // checked here. Therefore, information that uniquely identifies the
    // referenced song is not stored in a history entry just for this case.
    if (currentAlbumAtPlaylistPosition != m_album)
    {
        AUDWARN("The album at the activated entry's playlist position has"
                " changed.\n");
        return false;
    }

    m_playlist.set_position(m_playlistPosition);
    m_playlist.start_playback();
    m_playlist.activate();
    return true;
}

void HistoryEntry::debugPrint(const char * prefix) const
{
    AUDDBG("%salbum=\"%s\", playlist=\"%s\", entry number=%d\n", prefix,
           printable(m_album), printable(playlistTitle()), entryNumber());
}

int HistoryEntry::entryNumber() const
{
    // Add 1 because a playlist position is 0-based
    // but an Entry Number in the UI is 1-based.
    return m_playlistPosition + 1;
}

bool HistoryEntry::retrieveAlbum(String & album) const
{
    String errorMessage;
    const auto tuple = m_playlist.entry_tuple(m_playlistPosition,
                                              Playlist::Wait, &errorMessage);
    if (errorMessage || tuple.state() != Tuple::Valid)
    {
        AUDWARN("Failed to retrieve metadata of entry #%d in playlist %s: %s\n",
                entryNumber(), printable(playlistTitle()),
                errorMessage ? printable(errorMessage)
                             : "Song info could not be read");
        return false;
    }

    album = tuple.get_str(Tuple::Album);
    return true;
}

void HistoryModel::setFont(const QFont & font)
{
    m_currentlyPlaingFont = font;
    m_currentlyPlaingFont.setBold(true);

    if (m_playingPosition >= 0)
        updateFontForPosition(m_playingPosition);
}

void HistoryModel::activate(const QModelIndex & index)
{
    if (isOutOfBounds(index))
        return;
    const int pos = positionFromIndex(index);

    // The "playback ready" hook is activated asynchronously, so
    // playbackStarted() for the playback initiated here will be invoked after
    // this function updates m_playingPosition and returns.
    if (!m_entries[pos].play())
        return;

    // Update m_playingPosition here to prevent the imminent playbackStarted()
    // invocation from appending a copy of the activated entry to m_entries.
    if (pos != m_playingPosition)
    {
        const int prevPlayingPosition = m_playingPosition;
        m_playingPosition = pos;
        if (prevPlayingPosition >= 0)
            updateFontForPosition(prevPlayingPosition);
        updateFontForPosition(m_playingPosition);
    }
}

int HistoryModel::rowCount(const QModelIndex & parent) const
{
    return parent.isValid() ? 0 : m_entries.len();
}

int HistoryModel::columnCount(const QModelIndex & parent) const
{
    return parent.isValid() ? 0 : NColumns;
}

QVariant HistoryModel::data(const QModelIndex & index, int role) const
{
    if (isOutOfBounds(index))
        return QVariant();
    const int pos = positionFromIndex(index);

    switch (role)
    {
    case Qt::DisplayRole:
        return toQString(m_entries[pos].album());
    case Qt::ToolTipRole:
    {
        const auto & entry = m_entries[pos];
        // The playlist title and entry number are rarely interesting and
        // therefore shown only in the tooltip.
        return QString::fromUtf8(_("<b>Album:</b> %1<br><b>Playlist:</b> %2"
                                   "<br><b>Entry Number:</b> %3"))
            .arg(toQString(entry.album()), toQString(entry.playlistTitle()),
                 QString::number(entry.entryNumber()));
    }
    case Qt::FontRole:
        if (pos == m_playingPosition)
            return m_currentlyPlaingFont;
        break;
    }

    return QVariant();
}

bool HistoryModel::isOutOfBounds(const QModelIndex & index) const
{
    if (!index.isValid())
    {
        AUDWARN("Invalid index.\n");
        return true;
    }
    if (index.row() >= m_entries.len())
    {
        AUDWARN("Index row is out of bounds: %d >= %d\n", index.row(),
                m_entries.len());
        return true;
    }
    return false;
}

int HistoryModel::modelRowFromPosition(int position) const
{
    assert(position >= 0);
    assert(position < m_entries.len());
    // Reverse the order of entries here in order to:
    // 1) display most recently played entries at the top of the view and thus
    // avoid scrolling to the bottom of the view each time a new entry is added;
    // 2) efficiently append new elements to m_entries.
    // This code must be kept in sync with playbackStarted().
    return m_entries.len() - 1 - position;
}

int HistoryModel::positionFromIndex(const QModelIndex & index) const
{
    assert(!isOutOfBounds(index));
    // modelRowFromPosition() is an involution (self-inverse function),
    // and thus this inverse function delegates to it.
    return modelRowFromPosition(index.row());
}

void HistoryModel::updateFontForPosition(int position)
{
    const int row = modelRowFromPosition(position);
    const auto topLeft = createIndex(row, 0);
    // Optimize for the current single-column implementation, but also support
    // NColumns > 1 to prevent a bug here if a column is added in the future.
    if constexpr (NColumns == 1)
        emit dataChanged(topLeft, topLeft, {Qt::FontRole});
    else
    {
        assert(NColumns > 1);
        const auto bottomRight = createIndex(row, NColumns - 1);
        emit dataChanged(topLeft, bottomRight, {Qt::FontRole});
    }
}

void HistoryModel::playbackStarted()
{
    HistoryEntry entry;
    if (!entry.assignPlayingEntry())
        return;

    entry.debugPrint("Started playing ");
    AUDDBG("playing position=%d, entry count=%d\n", m_playingPosition,
           m_entries.len());

    if (m_playingPosition >= 0 &&
        m_entries[m_playingPosition].album() == entry.album())
    {
        // In all probability, a song from the same album started playing, in
        // which case there is nothing to do. Much less likely, a different
        // album with the same name, separated by other albums from the
        // previously played one, was just randomly selected.
        // Ignore this possibility here. The users concerned about such an
        // occurrence are advised to edit metadata in their music collections
        // and ensure unique album names. The unique album names would also
        // prevent Audacious from erroneously playing same-name albums
        // in order if they happen to end up adjacent in a playlist.
        return;
    }

    const int prevPlayingPosition = m_playingPosition;

    // The last played entry appears at the top of the view. Therefore, the new
    // entry is inserted at row 0.
    // This code must be kept in sync with modelRowFromPosition().
    beginInsertRows(QModelIndex(), 0, 0);
    // Update m_playingPosition during the row insertion to avoid
    // updating the font for the new playing position separately below.
    m_playingPosition = m_entries.len();
    m_entries.append(std::move(entry));
    endInsertRows();

    if (prevPlayingPosition >= 0)
        updateFontForPosition(prevPlayingPosition);
}

HistoryView::HistoryView()
{
    AUDDBG("creating\n");

    // Hide the header to save vertical space. A single-column caption is not
    // very useful, because the user can learn the meaning of an item's text
    // by examining its tooltip.
    setHeaderHidden(true);

    setAllColumnsShowFocus(true);
    setFrameShape(QFrame::NoFrame);
    setIndentation(0);

    m_model.setFont(font());
    setModel(&m_model);
    connect(this, &QTreeView::activated, &m_model, &HistoryModel::activate);
}

void HistoryView::changeEvent(QEvent * event)
{
    if (event->type() == QEvent::FontChange)
        m_model.setFont(font());

    audqt::TreeView::changeEvent(event);
}

static QPointer<HistoryView> s_history_view;

void * PlaybackHistory::get_qt_widget()
{
    assert(!s_history_view);
    s_history_view = new HistoryView;
    return s_history_view;
}

int PlaybackHistory::take_message(const char * code, const void *, int)
{
    if (!strcmp(code, "grab focus") && s_history_view)
    {
        s_history_view->setFocus(Qt::OtherFocusReason);
        return 0;
    }

    return -1;
}
