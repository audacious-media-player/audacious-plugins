/*
 * tool_bar.cc
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

#include "tool_bar.h"

#include <QAction>
#include <QIcon>

#include <libaudcore/runtime.h>
#include <libaudqt/libaudqt.h>

ToolBar::ToolBar(QWidget * parent, ArrayRef<ToolBarItem> items)
    : QToolBar(parent)
{
    setContextMenuPolicy(Qt::PreventContextMenu);
    setMovable(false);
    setObjectName("MainToolBar");

#ifdef _WIN32
    // avoid double border with the tab bar below
    setStyleSheet("QToolBar { border: 0px; }");
#endif

    for (const ToolBarItem & item : items)
    {
        QAction * a = nullptr;

        if (item.widget)
            a = addWidget(item.widget);
        else if (item.sep)
            a = addSeparator();
        else if (item.icon_name)
        {
            a = new QAction(QIcon::fromTheme(item.icon_name),
                            audqt::translate_str(item.name), this);

            if (item.tooltip_text)
                a->setToolTip(audqt::translate_str(item.tooltip_text));

            if (item.callback)
                connect(a, &QAction::triggered, item.callback);

            if (item.toggled)
            {
                a->setCheckable(true);
                connect(a, &QAction::toggled, item.toggled);
            }

            addAction(a);
        }

        if (item.action_ptr)
            *item.action_ptr = a;
    }
}
