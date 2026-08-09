/*  BMP - Cross-platform multimedia player
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The Audacious team does not consider modular code linking to
 *  Audacious or using our public API to be a derived work.
 */

#ifndef SKINS_UI_SKINSELECTOR_H
#define SKINS_UI_SKINSELECTOR_H

#include <QListView>
#include <QStandardItemModel>
#include <QStyledItemDelegate>

class SkinSelectorModel : public QStandardItemModel
{
public:
    explicit SkinSelectorModel (QObject * parent = nullptr);

    void setActiveIndex (const QModelIndex & index);
    QModelIndex activeIndex () const;
    void refresh ();

private:
    QStandardItem * m_active_item = nullptr;
};

class SkinSelectorView : public QListView
{
public:
    explicit SkinSelectorView (QWidget * parent = nullptr);
    void refresh ();

protected:
    QSize sizeHint () const override;
    void dragEnterEvent (QDragEnterEvent * event) override;
    void dragMoveEvent (QDragMoveEvent * event) override;
    void dropEvent (QDropEvent * event) override;
    void currentChanged (const QModelIndex & current,
                         const QModelIndex & previous) override;
private:
    SkinSelectorModel * m_model;
};

class SkinSelectorItemDelegate : public QStyledItemDelegate
{
public:
    explicit SkinSelectorItemDelegate (QObject * parent = nullptr);

    void paint (QPainter * painter,
                const QStyleOptionViewItem & option,
                const QModelIndex & index) const override;

    QSize sizeHint (const QStyleOptionViewItem & option,
                    const QModelIndex & index) const override;
};

#endif /* SKINS_UI_SKINSELECTOR_H */
