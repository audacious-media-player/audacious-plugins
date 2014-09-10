/*
 * tool_bar.h
 * Copyright 2014 William Pitcock
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

#include <QtWidgets>
#include "tool_bar.h"

#include <libaudcore/runtime.h>
#include <libaudqt/libaudqt.h>

ToolBar::ToolBar (QWidget * parent, ToolBarItem * items, size_t count)
    : QToolBar (parent), m_items (items), m_count (count)
{
    setContextMenuPolicy (Qt::PreventContextMenu);
    setMovable (false);
    setIconSize (QSize (22, 22));

    for (int i = 0; i < m_count; i++)
    {
        /* if item is preexisting, then that means it is a widget. */
        if (m_items[i].item)
            addWidget ((QWidget *) m_items[i].item);
        else if (m_items[i].sep)
            addSeparator ();
        else if (m_items[i].construct)
        {
            m_items[i].item = m_items[i].construct ();
            if (m_items[i].item)
                addWidget ((QWidget *) m_items[i].item);
        }
        else if (m_items[i].icon_name)
        {
            QAction * a = new QAction (QIcon::fromTheme (m_items[i].icon_name), audqt::translate_str (m_items[i].name), nullptr);

            if (m_items[i].tooltip_text)
                a->setToolTip (audqt::translate_str (m_items[i].tooltip_text));

            if (m_items[i].callback)
                connect (a, &QAction::triggered, m_items[i].callback);

            if (m_items[i].toggled)
            {
                a->setCheckable (true);
                connect (a, &QAction::toggled, m_items[i].toggled);
            }

            addAction (a);

            m_items[i].item = a;
        }

        if (m_items[i].set_ptr)
            * m_items[i].set_ptr = m_items[i].item;
    }
}

ToolBar::~ToolBar ()
{
    for (int i = 0; i < m_count; i++)
    {
        if (m_items[i].construct)
            delete (QWidget *) m_items[i].item;
        else if (m_items[i].icon_name)
            delete (QAction *) m_items[i].item;
    }
}
