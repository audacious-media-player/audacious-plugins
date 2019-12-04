/*
 * html-delegate.h
 * Copyright 2018 John Lindgren and René J.V. Bertin
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
#ifndef HTMLDELEGATE_H
#define HTMLDELEGATE_H

#include <QStyledItemDelegate>

// Allow rich text in QTreeView entries
class HtmlDelegate : public QStyledItemDelegate
{
protected:
    void paint (QPainter * painter, const QStyleOptionViewItem & option,
                const QModelIndex & index) const override;
    QSize sizeHint (const QStyleOptionViewItem & option,
                    const QModelIndex & index) const override;
};

#endif // HTMLDELEGATE_H
