/*
 * ui_skinselector.c
 * Copyright 1998-2003 XMMS Development Team
 * Copyright 2003-2004 BMP Development Team
 * Copyright 2011 John Lindgren
 *
 * This file is part of Audacious.
 *
 * Audacious is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2 or version 3 of the License.
 *
 * Audacious is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Audacious. If not, see <http://www.gnu.org/licenses/>.
 *
 * The Audacious team does not consider modular code linking to Audacious or
 * using our public API to be a derived work.
 */

#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include <QApplication>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QStandardItem>
#include <QTimer>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudqt/libaudqt.h>

#include "plugin.h"
#include "skin.h"
#include "skinselector.h"
#include "skins_util.h"
#include "view.h"

struct SkinNode {
    String name, desc, path;
};

enum SkinSelectorRoles {
    PixmapRole = Qt::UserRole + 1,
    TitleRole,
    SubtitleRole
};

static Index<SkinNode> skinlist;

static QPixmap skin_get_preview (const char * path)
{
    QPixmap preview;

    StringBuf archive_path;
    if (file_is_archive (path))
    {
        archive_path = archive_decompress (path);
        if (! archive_path)
            return preview;

        path = archive_path;
    }

    StringBuf preview_path = skin_pixmap_locate (path, "main");
    if (preview_path)
        preview.load ((const char *) preview_path);

    if (archive_path)
        del_directory (archive_path);

    return preview;
}

static QPixmap skin_get_thumbnail (const char * path)
{
    StringBuf base = filename_get_base (path);
    constexpr const char * format = "PNG";
    base.insert (-1, ".png");

    StringBuf thumbname = filename_build ({skins_get_skin_thumb_dir (), base});
    QPixmap thumb;

    if (g_file_test (thumbname, G_FILE_TEST_EXISTS))
        thumb.load ((const char *) thumbname, format);

    if (thumb.isNull ())
    {
        thumb = skin_get_preview (path);

        if (! thumb.isNull ())
        {
            make_directory (skins_get_skin_thumb_dir ());
            thumb.save ((const char *) thumbname, format);
        }
    }

    if (! thumb.isNull ())
    {
        return thumb.scaledToWidth (
            audqt::sizes.OneInch * 3 / 2,
            Qt::SmoothTransformation);
    }

    return thumb;
}

static void scan_skindir_func (const char * path, const char * basename)
{
    if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
    {
        if (file_is_archive (path))
            skinlist.append (String (archive_basename (basename)),
             String (_("Archived Winamp 2.x skin")), String (path));
    }
    else if (g_file_test (path, G_FILE_TEST_IS_DIR))
        skinlist.append (String (basename),
         String (_("Unarchived Winamp 2.x skin")), String (path));
}

static void skinlist_update ()
{
    skinlist.clear ();

    const char * user_skin_dir = skins_get_user_skin_dir ();
    if (g_file_test (user_skin_dir, G_FILE_TEST_EXISTS))
        dir_foreach (user_skin_dir, scan_skindir_func);

    StringBuf path = filename_build ({aud_get_path (AudPath::DataDir), "Skins"});
    dir_foreach (path, scan_skindir_func);

    const char * skinsdir = getenv ("SKINSDIR");
    if (skinsdir)
    {
        for (const String & dir : str_list_to_index (skinsdir, ":"))
            dir_foreach (dir, scan_skindir_func);
    }

    skinlist.sort ([] (const SkinNode & a, const SkinNode & b)
        { return str_compare (a.name, b.name); });
}

SkinSelectorItemDelegate::SkinSelectorItemDelegate (QObject * parent)
    : QStyledItemDelegate (parent)
{
}

QSize SkinSelectorItemDelegate::sizeHint (const QStyleOptionViewItem & option,
                                          const QModelIndex & index) const
{
    return QSize (350, 65);
}

void SkinSelectorItemDelegate::paint (QPainter * painter,
                                      const QStyleOptionViewItem & option,
                                      const QModelIndex & index) const
{
    painter->save ();

    QStyleOptionViewItem opt (option);
    QApplication::style ()->drawPrimitive (
        QStyle::PE_PanelItemViewItem,
        & opt,
        painter);

    QPixmap pixmap = index.data (PixmapRole).value<QPixmap> ();
    QString title = index.data (TitleRole).toString ();
    QString subtitle = index.data (SubtitleRole).toString ();

    QRect rect = option.rect;

    constexpr int margin = 2;
    constexpr int spacing = 4;
    constexpr int image_width = 144;
    constexpr int image_height = 61;

    QRect image_rect (
        rect.left () + margin,
        rect.top () + (rect.height () - image_height) / 2,
        image_width,
        image_height);

    QRect text_rect (
        image_rect.right () + 4 * spacing,
        rect.top (),
        rect.right () - image_rect.right (),
        rect.height ());

    if (! pixmap.isNull ())
    {
        painter->drawPixmap (
            rect.left () + margin,
            rect.top () + (rect.height () - pixmap.height ()) / 2,
            pixmap);
    }

    QFont title_font = option.font;
    title_font.setBold (true);

    QFont subtitle_font = option.font;
    subtitle_font.setItalic (true);

    QFontMetrics title_fm (title_font);
    QFontMetrics subtitle_fm (subtitle_font);

    int total_height = title_fm.height () + spacing + subtitle_fm.height ();
    int y = rect.top () + (rect.height () - total_height) / 2;

    QColor title_color = (option.state & QStyle::State_Selected)
        ? option.palette.highlightedText ().color ()
        : option.palette.text ().color ();

    QColor subtitle_color = (option.state & QStyle::State_Selected)
        ? option.palette.highlightedText ().color ()
        : option.palette.placeholderText ().color ();

    painter->setFont (title_font);
    painter->setPen (title_color);

    painter->drawText (
        QRect (text_rect.left (), y,
               text_rect.width (), title_fm.height ()),
        Qt::AlignLeft | Qt::AlignVCenter,
        title);

    painter->setFont (subtitle_font);
    painter->setPen (subtitle_color);

    painter->drawText (
        QRect (text_rect.left (),
               y + title_fm.height () + spacing,
               text_rect.width (),
               subtitle_fm.height ()),
        Qt::AlignLeft | Qt::AlignVCenter,
        subtitle);

    painter->restore ();
}

SkinSelectorView::SkinSelectorView (QWidget * parent) :
    QListView (parent),
    m_model (new SkinSelectorModel (this))
{
    setModel (m_model);
    setItemDelegate (new SkinSelectorItemDelegate (this));

    setDragDropMode (QAbstractItemView::DropOnly);
    setEditTriggers (QAbstractItemView::NoEditTriggers);
    setSelectionBehavior (QAbstractItemView::SelectRows);
    setSelectionMode (QAbstractItemView::SingleSelection);
    setVerticalScrollMode (QAbstractItemView::ScrollPerPixel);

    setMinimumHeight (sizeHint ().height ());
    setAlternatingRowColors (true);
    setUniformItemSizes (true);

    refresh ();
}

void SkinSelectorView::refresh ()
{
    skinlist_update ();
    m_model->refresh ();

    QModelIndex index = m_model->activeIndex ();
    if (! index.isValid ())
        return;

    QSignalBlocker blocker (selectionModel ());
    setCurrentIndex (index);
    blocker.unblock ();

    QTimer::singleShot (0, this, [this] {
        QModelIndex index = m_model->activeIndex ();
        if (index.isValid ())
            scrollTo (index, QAbstractItemView::PositionAtCenter);
    });
}

QSize SkinSelectorView::sizeHint () const
{
    QSize size = QListView::sizeHint ();
    size.setHeight (audqt::sizes.OneInch * 3 / 2);
    return size;
}

void SkinSelectorView::dragEnterEvent (QDragEnterEvent * event)
{
    dragMoveEvent (event);
}

void SkinSelectorView::dragMoveEvent (QDragMoveEvent * event)
{
    if (event->mimeData ()->hasUrls ())
        event->acceptProposedAction ();
}

void SkinSelectorView::dropEvent (QDropEvent * event)
{
    const QMimeData * mimedata = event->mimeData ();
    if (! mimedata->hasUrls ())
        return;

    bool installed_skin = false;

    for (const auto & url : mimedata->urls ())
    {
        if (! url.isLocalFile ())
            continue;

        QByteArray local_file = url.toLocalFile ().toUtf8 ();
        const char * path = local_file.constData ();

        if (str_has_suffix_nocase (path, ".wsz") ||
            str_has_suffix_nocase (path, ".zip"))
        {
            if (! skin_load (path))
                continue;

            view_apply_skin ();
            skin_install_skin (path);
            installed_skin = true;
        }
    }

    if (installed_skin)
    {
        refresh ();
        event->acceptProposedAction ();
    }
}

void SkinSelectorView::currentChanged (const QModelIndex & current,
                                       const QModelIndex & previous)
{
    QListView::currentChanged (current, previous);

    int row = current.row ();
    if (row < 0 || row >= skinlist.len ())
        return;

    if (! skin_load (skinlist[row].path))
        return;

    view_apply_skin ();
    m_model->setActiveIndex (current);
}

SkinSelectorModel::SkinSelectorModel (QObject * parent) :
    QStandardItemModel (parent)
{
}

void SkinSelectorModel::setActiveIndex (const QModelIndex & index)
{
    m_active_item = index.isValid () ? itemFromIndex (index) : nullptr;
}

QModelIndex SkinSelectorModel::activeIndex () const
{
    return m_active_item ? indexFromItem (m_active_item) : QModelIndex ();
}

void SkinSelectorModel::refresh ()
{
    beginResetModel ();

    m_active_item = nullptr;
    removeRows (0, rowCount ());

    int active_row = -1;
    String current_path = aud_get_str ("skins", "skin");
    int n_skins = skinlist.len ();

    for (int i = 0; i < n_skins; i ++)
    {
        const SkinNode & node = skinlist[i];
        auto * item = new QStandardItem;

        item->setData (skin_get_thumbnail ((const char *) node.path), PixmapRole);
        item->setData (QString::fromUtf8 ((const char *) node.name), TitleRole);
        item->setData (QString::fromUtf8 ((const char *) node.desc), SubtitleRole);

        appendRow (item);

        if (active_row < 0 && ! strcmp (current_path, skinlist[i].path))
            active_row = i;
    }

    QModelIndex index = this->index (active_row, 0);
    if (index.isValid ())
        setActiveIndex (index);

    endResetModel ();
}
