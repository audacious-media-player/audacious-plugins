/*
 * html-delegate.cc
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

#include "html-delegate.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QPainter>

#include <libaudqt/libaudqt.h>

static void init_text_document (QTextDocument & doc, const QStyleOptionViewItem & option)
{
    doc.setHtml (option.text);
    doc.setDocumentMargin (audqt::sizes.TwoPt);
    doc.setDefaultFont (option.font);
}

void HtmlDelegate::paint (QPainter * painter, const QStyleOptionViewItem & option_,
                          const QModelIndex & index) const
{
    QStyleOptionViewItem option = option_;
    initStyleOption (& option, index);

    QTextDocument doc;
    init_text_document (doc, option);

    QStyle * style = option.widget ? option.widget->style () : qApp->style ();
    QAbstractTextDocumentLayout::PaintContext ctx;

    // Painting item without text
    option.text = QString ();
    style->drawControl (QStyle::CE_ItemViewItem, & option, painter);

    // Color group logic imitating qcommonstyle.cpp
    QPalette::ColorGroup cg =
        ! (option.state & QStyle::State_Enabled) ? QPalette::Disabled :
        ! (option.state & QStyle::State_Active) ? QPalette::Inactive : QPalette::Normal;

    // Highlighting text if item is selected
    if (option.state & QStyle::State_Selected)
        ctx.palette.setColor (QPalette::Text, option.palette.color (cg, QPalette::HighlightedText));
    else
        ctx.palette.setColor (QPalette::Text, option.palette.color (cg, QPalette::Text));

    QRect textRect = style->subElementRect (QStyle::SE_ItemViewItemText, & option);
    painter->save ();
    painter->translate (textRect.topLeft ());
    painter->setClipRect (textRect.translated (-textRect.topLeft ()));
    doc.documentLayout ()->draw (painter, ctx);
    painter->restore ();
}

QSize HtmlDelegate::sizeHint (const QStyleOptionViewItem & option_,
                              const QModelIndex & index) const
{
    QStyleOptionViewItem option = option_;
    initStyleOption (& option, index);

    QTextDocument doc;
    init_text_document (doc, option);

    return QSize (audqt::sizes.OneInch, doc.size ().height ());
}

