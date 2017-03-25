/*
 * search_bar.cc
 * Copyright 2016 John Lindgren
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

#include "search_bar.h"
#include "playlist.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include <libaudcore/i18n.h>
#include <libaudqt/libaudqt.h>

static QPushButton * makeButton (const char * icon, QWidget * parent)
{
    auto button = new QPushButton (QIcon::fromTheme (icon), QString (), parent);
    button->setFlat (true);
    button->setFocusPolicy (Qt::NoFocus);
    return button;
}

SearchBar::SearchBar (QWidget * parent, PlaylistWidget * playlistWidget) :
    QWidget (parent),
    m_playlistWidget (playlistWidget),
    m_entry (new QLineEdit (this))
{
    m_entry->setClearButtonEnabled (true);
    m_entry->setPlaceholderText (_("Search playlist"));

    auto upButton = makeButton ("go-up", this);
    auto downButton = makeButton ("go-down", this);
    auto closeButton = makeButton ("window-close", this);

    auto layout = audqt::make_hbox (this);
    layout->setContentsMargins (audqt::margins.TwoPt);

    layout->addWidget (m_entry);
    layout->addWidget (upButton);
    layout->addWidget (downButton);
    layout->addWidget (closeButton);

    setFocusProxy (m_entry);

    connect (m_entry, & QLineEdit::textChanged, [this] (const QString & text) {
        m_playlistWidget->setFilter (text.toUtf8 ());
    });

    connect (upButton, & QPushButton::clicked, [this] (bool) {
        m_playlistWidget->moveFocus (-1);
    });

    connect (downButton, & QPushButton::clicked, [this] (bool) {
        m_playlistWidget->moveFocus (1);
    });

    connect (closeButton, & QPushButton::clicked, [this] (bool) {
        m_entry->clear ();
        m_playlistWidget->setFocus ();
        hide ();
    });
}

void SearchBar::keyPressEvent (QKeyEvent * event)
{
    if (event->modifiers () == Qt::NoModifier)
    {
        switch (event->key ())
        {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            QApplication::sendEvent (m_playlistWidget, event);
            return;

        case Qt::Key_Escape:
            m_entry->clear ();
            m_playlistWidget->setFocus ();
            hide ();
            return;
        }
    }

    QWidget::keyPressEvent (event);
}
