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

#include <cassert>
#include <cstring>
#include <utility>

#include <QAbstractListModel>
#include <QEvent>
#include <QFont>
#include <QMetaObject>
#include <QPointer>

#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/index.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>
#include <libaudcore/templates.h>
#include <libaudqt/treeview.h>

#include "../playback-history/history-entry.h"
#include "../playback-history/preferences.h"

class PlaybackHistoryQt : public GeneralPlugin
{
private:
    static constexpr const char * about = aboutText;
    static const PluginPreferences prefs;

public:
    static constexpr PluginInfo info = {N_("Playback History"), PACKAGE,
                                        about, &prefs, PluginQtOnly};

    constexpr PlaybackHistoryQt() : GeneralPlugin(info, false) {}

    bool init() override;
    void * get_qt_widget() override;
    int take_message(const char * code, const void *, int) override;
};

EXPORT PlaybackHistoryQt aud_plugin_instance;

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
    QModelIndex m_newlyCurrentIndex;
};

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
                   str_printf(_("<b>%s:</b> %s\n"
                                "<b>Playlist:</b> %s\n"
                                "<b>Entry Number:</b> %d"),
                              entry.translatedTextDesignation(),
                              static_cast<const char *>(entry.text()),
                              static_cast<const char *>(entry.playlistTitle()),
                              entry.entryNumber()))
            // Make life easier for translators by sharing the markup template.
            // GTK and Qt use different line breaks though, so replace them.
            .replace("\n", "<br>");
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

    const int pos = aud::min(positionFromModelRow(row),
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

    int n_entries = m_entries.len();
    entry.debugPrint("Started playing ");
    AUDDBG("playing position=%d, entry count=%d\n", m_playingPosition,
           n_entries);

    if (m_playingPosition >= 0 && m_playingPosition < n_entries)
    {
        const auto & prevPlayingEntry = m_entries[m_playingPosition];
        if (!entry.shouldAppendEntry(prevPlayingEntry))
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

    m_model.makeCurrent(index);
}

static QPointer<HistoryView> s_history_view;

bool PlaybackHistoryQt::init()
{
    aud_config_set_defaults(configSection, defaults);
    return true;
}

void * PlaybackHistoryQt::get_qt_widget()
{
    assert(!s_history_view);
    s_history_view = new HistoryView;
    return s_history_view;
}

int PlaybackHistoryQt::take_message(const char * code, const void *, int)
{
    if (!strcmp(code, "grab focus") && s_history_view)
    {
        s_history_view->setFocus(Qt::OtherFocusReason);
        return 0;
    }

    return -1;
}

const PluginPreferences PlaybackHistoryQt::prefs = {{widgets}};
