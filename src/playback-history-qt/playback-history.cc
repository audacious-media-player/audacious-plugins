/*
 * playback-history.cc
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

#include <algorithm>
#include <cassert>
#include <utility>

#include <QAbstractListModel>
#include <QEvent>
#include <QFont>
#include <QMetaObject>
#include <QPointer>

#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>
#include <libaudcore/templates.h>
#include <libaudqt/treeview.h>

/**
 * Returns a representation of @p str suitable for printing via audlog::log().
 */
static const char * printable(const String & str)
{
    // Printing nullptr invokes undefined behavior.
    return str ? str : "";
}

class PlaybackHistory : public GeneralPlugin
{
private:
    static constexpr const char * aboutText =
        N_("Playback History Plugin\n\n"
           "Copyright 2023-2024 Igor Kushnir <igorkuo@gmail.com>\n\n"
           "This plugin tracks and provides access to playback history.\n\n"

           "History entries are stored only in memory and are lost\n"
           "on Audacious exit. When the plugin is disabled,\n"
           "playback history is not tracked at all.\n"
           "History entries are only added, never removed automatically.\n"
           "The user can remove selected entries by pressing the Delete key.\n"
           "Restart Audacious or disable the plugin by closing\n"
           "Playback History view to clear the entries.\n\n"

           "Two history item granularities (modes) are supported.\n"
           "The user can select a mode in the plugin's settings.\n"
           "The Song mode is the default. Each played song is stored\n"
           "in history. Song titles are displayed in the list.\n"
           "When the Album mode is selected and multiple songs from\n"
           "a single album are played in a row, a single album entry\n"
           "is stored in history. Album names are displayed in the list.");

    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

public:
    static constexpr PluginInfo info = {N_("Playback History"), PACKAGE,
                                        aboutText, &prefs, PluginQtOnly};

    constexpr PlaybackHistory() : GeneralPlugin(info, false) {}

    void * get_qt_widget() override;
    int take_message(const char * code, const void *, int) override;
};

EXPORT PlaybackHistory aud_plugin_instance;

static constexpr const char * configSection = "playback-history";
static constexpr const char * configEntryType = "entry_type";

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
     * @param prefix a possibly empty string to be printed before @c this.
     *               A nonempty @p prefix should end with whitespace.
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

private:
    /**
     * Returns an untranslated human-readable designation of text() based on
     * type().
     */
    const char * untranslatedTextDesignation() const;

    /**
     * Retrieves the song title if type() is Song or the album name if type() is
     * Album at @a m_playlistPosition in @a m_playlist and assigns it to
     * @p text.
     *
     * @return @c true if the text was retrieved successfully.
     */
    bool retrieveText(String & text) const;

    /**
     * Returns @c true if @a m_playlist exists and @a m_playlistPosition still
     * points to the same playlist entry as at the time of last assignment.
     */
    bool isAvailable() const;

    String m_text;
    Playlist m_playlist; /**< the playlist, in which this entry was played */
    /** The position in @a m_playlist of the song titled text() if type() is
     * Song or of the first played song from the album named text() if type() is
     * Album.
     * When the user modifies the playlist, positions shift, but this should
     * happen rarely enough and therefore doesn't have to be handled perfectly.
     * A linear search for a song title or an album name equal to text() in
     * @a m_playlist is inefficient, and thus is never performed by this plugin.
     */
    int m_playlistPosition = -1;
    Type m_type = defaultType;
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
     * Gives keyboard focus to the playlist entry that corresponds to the item
     * at @p index and makes it the single selected entry in the playlist.
     *
     * Call this function when the user selects the item at @p index in the
     * view.
     */
    void makeCurrent(const QModelIndex & index) const;

    /**
     * Plays the song of the item at @p index.
     *
     * Call this function when the user activates the item at @p index in the
     * view.
     */
    void activate(const QModelIndex & index);

    /**
     * Returns @c true if rows are currently being removed from the model.
     */
    bool areRowsBeingRemoved() const { return m_areRowsBeingRemoved; }

    int rowCount(const QModelIndex & parent) const override;
    int columnCount(const QModelIndex & parent) const override;
    QVariant data(const QModelIndex & index, int role) const override;

    bool removeRows(int row, int count,
                    const QModelIndex & parent = QModelIndex()) override;

private:
    enum
    {
        ColumnText, /**< a song title or an album name */
        NColumns
    };

    // In this class the word "position" refers to an index into m_entries.

    bool isModelRowOutOfBounds(int row) const;
    bool isOutOfBounds(const QModelIndex & index) const;
    int modelRowFromPosition(int position) const;
    int positionFromModelRow(int row) const;
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
    /** The position of the entry that is currently playing
     * or was played last. -1 means "none". */
    int m_playingPosition = -1;
    bool m_areRowsBeingRemoved = false;
    /** a cached font used to highlight the item that is currently playing */
    QFont m_currentlyPlaingFont;
};

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
/** The optimization returns early from duplicate makeCurrent() invocations and
 * relies on a QMetaObject::invokeMethod() overload introduced in Qt 5.10. */
#define OPTIMIZE_MAKE_CURRENT
#endif

class HistoryView : public audqt::TreeView
{
public:
    HistoryView();

protected:
    void changeEvent(QEvent * event) override;
    void currentChanged(const QModelIndex & current,
                        const QModelIndex & previous) override;

private:
    void makeCurrent(const QModelIndex & index);

    HistoryModel m_model;
#ifdef OPTIMIZE_MAKE_CURRENT
    QModelIndex m_newlyCurrentIndex;
#endif
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

    const auto entryType = aud_get_int(configSection, configEntryType);
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
    // 1) select a history entry; 2) activate another playlist;
    // 3) give focus to History view; 4) press the Enter key.
    m_playlist.activate();

    return true;
}

void HistoryEntry::debugPrint(const char * prefix) const
{
    AUDDBG("%s%s=\"%s\", playlist=\"%s\", entry number=%d\n", prefix,
           untranslatedTextDesignation(), printable(m_text),
           printable(playlistTitle()), entryNumber());
}

const char * HistoryEntry::translatedTextDesignation() const
{
    switch (m_type)
    {
    case Type::Song:
        return _("Title");
    case Type::Album:
        return _("Album");
    }
    Q_UNREACHABLE();
}

int HistoryEntry::entryNumber() const
{
    // Add 1 because a playlist position is 0-based
    // but an Entry Number in the UI is 1-based.
    return m_playlistPosition + 1;
}

const char * HistoryEntry::untranslatedTextDesignation() const
{
    switch (m_type)
    {
    case Type::Song:
        return "title";
    case Type::Album:
        return "album";
    }
    Q_UNREACHABLE();
}

bool HistoryEntry::retrieveText(String & text) const
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
    // But such coincidences should be much more rare
    // and less of a problem than the text inequality condition
    // checked here. Therefore, information that uniquely identifies the
    // referenced song is not stored in a history entry just for this case.
    if (currentTextAtPlaylistPosition != m_text)
    {
        AUDWARN("The %s at the selected entry's playlist position has"
                " changed.\n",
                untranslatedTextDesignation());
        return false;
    }

    return true;
}

void HistoryModel::setFont(const QFont & font)
{
    m_currentlyPlaingFont = font;
    m_currentlyPlaingFont.setBold(true);

    if (m_playingPosition >= 0)
        updateFontForPosition(m_playingPosition);
}

void HistoryModel::makeCurrent(const QModelIndex & index) const
{
    if (isOutOfBounds(index))
        return;
    const int pos = positionFromIndex(index);
    m_entries[pos].makeCurrent();
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
    // This does not prevent appending a different-type counterpart entry if the
    // type of the activated entry does not match the currently configured
    // History Item Granularity. Such a scenario is uncommon (happens only when
    // the user switches between the History modes), and so is not specially
    // handled or optimized for.
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
        return QString(m_entries[pos].text());
    case Qt::ToolTipRole:
    {
        const auto & entry = m_entries[pos];
        // The playlist title and entry number are rarely interesting and
        // therefore shown only in the tooltip.
        return QString(
            str_printf(_("<b>%s:</b> %s<br><b>Playlist:</b> %s<br>"
                         "<b>Entry Number:</b> %d"),
                       entry.translatedTextDesignation(),
                       static_cast<const char *>(entry.text()),
                       static_cast<const char *>(entry.playlistTitle()),
                       entry.entryNumber()));
    }
    case Qt::FontRole:
        if (pos == m_playingPosition)
            return m_currentlyPlaingFont;
        break;
    }

    return QVariant();
}

bool HistoryModel::removeRows(int row, int count, const QModelIndex & parent)
{
    if (count <= 0 || parent.isValid())
        return false;

    const int lastRowToRemove = row + count - 1;
    if (isModelRowOutOfBounds(row) || isModelRowOutOfBounds(lastRowToRemove))
        return false;

    const int pos = std::min(positionFromModelRow(row),
                             positionFromModelRow(lastRowToRemove));
    // pos is the lesser of the positions that correspond to the first and last
    // removed model rows. Remove the range [pos, pos + count) from m_entries.

    m_areRowsBeingRemoved = true;
    beginRemoveRows(QModelIndex(), row, lastRowToRemove);

    if (m_playingPosition >= pos && m_playingPosition < pos + count)
        m_playingPosition = -1; // unset the playing position before removing it
    else if (m_playingPosition > pos)
    {
        // Shift the playing position to account for the removed range.
        m_playingPosition -= count;
    }

    m_entries.remove(pos, count);

    endRemoveRows();
    m_areRowsBeingRemoved = false;

    return true;
}

bool HistoryModel::isModelRowOutOfBounds(int row) const
{
    if (row >= 0 && row < m_entries.len())
        return false;
    AUDWARN("Model row is out of bounds: %d is not in the range [0, %d)\n", row,
            m_entries.len());
    return true;
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

int HistoryModel::positionFromModelRow(int row) const
{
    assert(!isModelRowOutOfBounds(row));
    // modelRowFromPosition() is an involution (self-inverse function),
    // and thus this inverse function delegates to it.
    return modelRowFromPosition(row);
}

int HistoryModel::positionFromIndex(const QModelIndex & index) const
{
    assert(!isOutOfBounds(index));
    return positionFromModelRow(index.row());
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

    const auto shouldAppendEntry = [this, &entry] {
        if (m_playingPosition < 0)
            return true;
        const auto & prevPlayingEntry = m_entries[m_playingPosition];

        if (prevPlayingEntry.type() != entry.type() ||
            prevPlayingEntry.playlist() != entry.playlist())
        {
            return true; // the two entries are very different indeed
        }

        // When the entry numbers differ, either the entries point to two
        // different songs or the user has modified the playlist and playlist
        // positions have shifted, which invalidated prevPlayingEntry. Either
        // way, the entry should be appended if the type is Song.
        if (entry.type() == HistoryEntry::Type::Song &&
            prevPlayingEntry.entryNumber() != entry.entryNumber())
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
        return prevPlayingEntry.text() != entry.text();
    }();
    if (!shouldAppendEntry)
        return;

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

    // Allow removing multiple items at once by pressing the Delete key.
    setSelectionMode(ExtendedSelection);

    m_model.setFont(font());
    setModel(&m_model);
    connect(this, &QTreeView::activated, &m_model, &HistoryModel::activate);

    // Overriding currentChanged() is not sufficient, because a mouse click on
    // a current item should make the corresponding playlist entry current but
    // does not invoke currentChanged().
    // Connect makeCurrent() to QTreeView::pressed rather than
    // QTreeView::clicked for two reasons:
    // 1) Any mouse button click, not only left click,
    //    makes the clicked history view item current.
    // 2) The item becomes current during mousePressEvent(),
    //    not mouseReleaseEvent().
    connect(this, &QTreeView::pressed, this, &HistoryView::makeCurrent);
}

void HistoryView::changeEvent(QEvent * event)
{
    if (event->type() == QEvent::FontChange)
        m_model.setFont(font());

    audqt::TreeView::changeEvent(event);
}

void HistoryView::currentChanged(const QModelIndex & current,
                                 const QModelIndex & previous)
{
    audqt::TreeView::currentChanged(current, previous);

    AUDDBG("currentChanged: %d => %d\n", previous.row(), current.row());

    // currentChanged() is called repeatedly while rows are being removed.
    // Debug output when there are 4 history entries, the 4th entry is current
    // and all 4 entries are removed:
    // currentChanged: 3 => 2
    // currentChanged: 2 => 1
    // currentChanged: 1 => 0
    // currentChanged: 0 => -1
    // History entry removal is not an explicit selection of an item, and
    // therefore should not affect playlist focus and selection.
    if (m_model.areRowsBeingRemoved())
        return;

    // Connecting makeCurrent() to QTreeView::pressed makes clicked entries
    // current. This code makes an entry selected via keyboard current.
    // In case of keyboard navigation, the previous current index is invalid
    // only when focus is transferred to the history view. A focus transfer is
    // not an explicit selection of an item, and therefore should not affect
    // playlist focus and selection.
    if (previous.isValid() && current.isValid())
        makeCurrent(current);
}

void HistoryView::makeCurrent(const QModelIndex & index)
{
    // QAbstractItemView::pressed is only emitted when the index is valid.
    assert(index.isValid());

#ifdef OPTIMIZE_MAKE_CURRENT
    AUDDBG("makeCurrent: %d => %d\n", m_newlyCurrentIndex.row(), index.row());

    // Clicking on a noncurrent item calls currentChanged() and emits
    // QTreeView::pressed. This function is invoked twice in a row then.
    // Return early from the second call to avoid redundant work.
    if (index == m_newlyCurrentIndex)
        return;
    m_newlyCurrentIndex = index;

    // Events are normally not processed between the corresponding
    // QTreeView::pressed emission and currentChanged() call. So invalidating
    // m_newlyCurrentIndex when control returns to the event loop works here.
    [[maybe_unused]] const bool invoked = QMetaObject::invokeMethod(
        this, [this] { m_newlyCurrentIndex = {}; }, Qt::QueuedConnection);
    assert(invoked);
#else
    AUDDBG("makeCurrent: %d\n", index.row());
#endif

    m_model.makeCurrent(index);
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

const char * const PlaybackHistory::defaults[] = {
    configEntryType,
    aud::numeric_string<static_cast<int>(HistoryEntry::defaultType)>::str,
    nullptr};

const PreferencesWidget PlaybackHistory::widgets[] = {
    WidgetLabel(N_("<b>History Item Granularity</b>")),
    WidgetRadio(N_("Song"), WidgetInt(configSection, configEntryType),
                {static_cast<int>(HistoryEntry::Type::Song)}),
    WidgetRadio(N_("Album"), WidgetInt(configSection, configEntryType),
                {static_cast<int>(HistoryEntry::Type::Album)})};

const PluginPreferences PlaybackHistory::prefs = {{widgets}};
