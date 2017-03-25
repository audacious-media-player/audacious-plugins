/*
 * playlist-manager-qt.cc
 * Copyright 2015 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <QAbstractListModel>
#include <QBoxLayout>
#include <QFont>
#include <QGuiApplication>
#include <QHeaderView>
#include <QIcon>
#include <QMouseEvent>
#include <QToolButton>
#include <QTreeView>

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/plugin.h>
#include <libaudqt/libaudqt.h>

class PlaylistManagerQt : public GeneralPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Playlist Manager"),
        PACKAGE,
        nullptr, // about
        nullptr, // prefs
        PluginQtOnly
    };

    constexpr PlaylistManagerQt () : GeneralPlugin (info, false) {}

    void * get_qt_widget ();
    int take_message (const char * code, const void *, int);
};

EXPORT PlaylistManagerQt aud_plugin_instance;

class PlaylistsModel : public QAbstractListModel
{
public:
    enum {
        ColumnTitle,
        ColumnEntries,
        NColumns
    };

    PlaylistsModel () :
        m_rows (Playlist::n_playlists ()),
        m_playing (Playlist::playing_playlist ().index ()),
        m_bold (QGuiApplication::font ())
    {
        m_bold.setBold (true);
    }

    void update (Playlist::UpdateLevel level);

protected:
    int rowCount (const QModelIndex & parent) const { return m_rows; }
    int columnCount (const QModelIndex & parent) const { return NColumns; }

    Qt::DropActions supportedDropActions () const { return Qt::MoveAction; }

    Qt::ItemFlags flags (const QModelIndex & index) const
    {
        if (index.isValid ())
            return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
        else
            return Qt::ItemIsSelectable | Qt::ItemIsDropEnabled | Qt::ItemIsEnabled;
    }

    QVariant data (const QModelIndex & index, int role) const;
    QVariant headerData (int section, Qt::Orientation orientation, int role) const;

private:
    void update_rows (int row, int count);
    void update_playing ();

    const HookReceiver<PlaylistsModel>
     activate_hook {"playlist set playing", this, & PlaylistsModel::update_playing};

    int m_rows, m_playing;
    QFont m_bold;
};

class PlaylistsView : public QTreeView
{
public:
    PlaylistsView ();

protected:
    void currentChanged (const QModelIndex & current, const QModelIndex & previous);
    void keyPressEvent (QKeyEvent * event);
    void mouseDoubleClickEvent (QMouseEvent * event);
    void dropEvent (QDropEvent * event);

private:
    PlaylistsModel m_model;

    void update (Playlist::UpdateLevel level);
    void update_sel ();

    void play_current ()
        { Playlist::by_index (currentIndex ().row ()).start_playback (); }

    const HookReceiver<PlaylistsView, Playlist::UpdateLevel>
     update_hook {"playlist update", this, & PlaylistsView::update};
    const HookReceiver<PlaylistsView>
     activate_hook {"playlist activate", this, & PlaylistsView::update_sel};

    int m_in_update = 0;
};

QVariant PlaylistsModel::data (const QModelIndex & index, int role) const
{
    switch (role)
    {
    case Qt::DisplayRole:
    {
        auto list = Playlist::by_index (index.row ());
        switch (index.column ())
        {
        case ColumnTitle:
            return QString (list.get_title ());
        case ColumnEntries:
            return list.n_entries ();
        }
    }
        break;

    case Qt::FontRole:
        if (index.row () == m_playing)
            return m_bold;
        break;

    case Qt::TextAlignmentRole:
        if (index.column () == ColumnEntries)
            return Qt::AlignRight;
        break;
    }

    return QVariant ();
}

QVariant PlaylistsModel::headerData (int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
        case ColumnTitle:
            return QString (_("Title"));
        case ColumnEntries:
            return QString (_("Entries"));
        }
    }

    return QVariant ();
}

void PlaylistsModel::update_rows (int row, int count)
{
    if (count < 1)
        return;

    auto topLeft = createIndex (row, 0);
    auto bottomRight = createIndex (row + count - 1, NColumns - 1);
    emit dataChanged (topLeft, bottomRight);
}

void PlaylistsModel::update_playing ()
{
    int playing = Playlist::playing_playlist ().index ();

    if (playing != m_playing)
    {
        if (m_playing >= 0)
            update_rows (m_playing, 1);
        if (playing >= 0)
            update_rows (playing, 1);

        m_playing = playing;
    }
}

void PlaylistsModel::update (const Playlist::UpdateLevel level)
{
    int rows = Playlist::n_playlists ();

    if (level == Playlist::Structure)
    {
        if (rows < m_rows)
        {
            beginRemoveRows (QModelIndex (), rows, m_rows - 1);
            m_rows = rows;
            endRemoveRows ();
        }
        else if (rows > m_rows)
        {
            beginInsertRows (QModelIndex (), m_rows, rows - 1);
            m_rows = rows;
            endInsertRows ();
        }
    }

    if (level >= Playlist::Metadata)
    {
        update_rows (0, m_rows);
        m_playing = Playlist::playing_playlist ().index ();
    }
    else
        update_playing ();
}

PlaylistsView::PlaylistsView ()
{
    m_in_update ++;
    setModel (& m_model);
    update_sel ();
    m_in_update --;

    auto hdr = header ();
    hdr->setStretchLastSection (false);
    hdr->setSectionResizeMode (PlaylistsModel::ColumnTitle, QHeaderView::Stretch);
    hdr->setSectionResizeMode (PlaylistsModel::ColumnEntries, QHeaderView::Interactive);
    hdr->resizeSection (PlaylistsModel::ColumnEntries, audqt::to_native_dpi (64));

    setDragDropMode (InternalMove);
    setFrameShape (QFrame::NoFrame);
    setIndentation (0);
}

void PlaylistsView::currentChanged (const QModelIndex & current, const QModelIndex & previous)
{
    QTreeView::currentChanged (current, previous);
    if (! m_in_update)
        Playlist::by_index (current.row ()).activate ();
}

void PlaylistsView::keyPressEvent (QKeyEvent * event)
{
    if (event->modifiers () == Qt::NoModifier)
    {
        switch (event->key ())
        {
        case Qt::Key_Enter:
        case Qt::Key_Return:
            play_current ();
            return;
        default:
            break;
        }
    }

    QTreeView::keyPressEvent (event);
}

void PlaylistsView::mouseDoubleClickEvent (QMouseEvent * event)
{
    if (event->button () == Qt::LeftButton)
        play_current ();
}

void PlaylistsView::dropEvent (QDropEvent * event)
{
    if (event->source () != this || event->proposedAction () != Qt::MoveAction)
        return;

    int from = currentIndex ().row ();
    if (from < 0)
        return;

    int to;
    switch (dropIndicatorPosition ())
    {
        case AboveItem: to = indexAt (event->pos ()).row (); break;
        case BelowItem: to = indexAt (event->pos ()).row () + 1; break;
        case OnViewport: to = Playlist::n_playlists (); break;
        default: return;
    }

    Playlist::reorder_playlists (from, (to > from) ? to - 1 : to, 1);
    event->acceptProposedAction ();
}

void PlaylistsView::update (Playlist::UpdateLevel level)
{
    m_in_update ++;
    m_model.update (level);
    update_sel ();
    m_in_update --;
}

void PlaylistsView::update_sel ()
{
    m_in_update ++;
    auto sel = selectionModel ();
    auto current = m_model.index (Playlist::active_playlist ().index (), 0);
    sel->setCurrentIndex (current, sel->ClearAndSelect | sel->Rows);
    m_in_update --;
}

static PlaylistsView * s_playlists_view = nullptr;

static QToolButton * new_tool_button (const char * text, const char * icon)
{
    auto button = new QToolButton;
    button->setIcon (QIcon::fromTheme (icon));
    button->setText (audqt::translate_str (text));
    button->setToolButtonStyle (Qt::ToolButtonTextBesideIcon);
    return button;
}

void * PlaylistManagerQt::get_qt_widget ()
{
    s_playlists_view = new PlaylistsView;

    QObject::connect (s_playlists_view, & QObject::destroyed, [] () {
        s_playlists_view = nullptr;
    });

    auto new_button = new_tool_button (N_("_New"), "document-new");
    QObject::connect (new_button, & QToolButton::clicked, Playlist::new_playlist);

    auto rename_button = new_tool_button (N_("Ren_ame"), "insert-text");
    QObject::connect (rename_button, & QToolButton::clicked, [] () {
        audqt::playlist_show_rename (Playlist::active_playlist ());
    });

    auto remove_button = new_tool_button (N_("_Remove"), "edit-delete");
    QObject::connect (remove_button, & QToolButton::clicked, [] () {
        audqt::playlist_confirm_delete (Playlist::active_playlist ());
    });

    auto hbox = audqt::make_hbox (nullptr);
    hbox->setContentsMargins (audqt::margins.TwoPt);

    hbox->addWidget (new_button);
    hbox->addWidget (rename_button);
    hbox->addStretch (1);
    hbox->addWidget (remove_button);

    auto widget = new QWidget;
    auto vbox = audqt::make_vbox (widget, 0);

    vbox->addWidget (s_playlists_view, 1);
    vbox->addLayout (hbox);

    return widget;
}

int PlaylistManagerQt::take_message (const char * code, const void *, int)
{
    if (! strcmp (code, "grab focus") && s_playlists_view)
    {
        s_playlists_view->setFocus (Qt::OtherFocusReason);
        return 0;
    }

    return -1;
}
