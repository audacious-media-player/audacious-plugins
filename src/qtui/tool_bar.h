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

#ifndef TOOL_BAR_H
#define TOOL_BAR_H

#include <QToolBar>

struct ToolBarItem
{
    const char * icon_name;
    const char * name;
    const char * tooltip_text;

    QWidget * (* construct) (void);
    std::function <void ()> callback;

    QObject * item;

    bool sep;
    bool checkable;

    void ** set_ptr;
};

class ToolBar : public QToolBar
{
public:
    ToolBar (QWidget * parent, ToolBarItem * items, size_t count);
    ~ToolBar ();

private:
    ToolBarItem * m_items;
    size_t m_count;
};

#endif
