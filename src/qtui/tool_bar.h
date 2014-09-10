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
    void (* callback) ();
    void (* toggled) (bool on);

    QObject * item;

    bool sep;

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

constexpr ToolBarItem ToolBarAction (const char * icon_name, const char * name, const char * tooltip_text,
                                     void (* callback) (), void ** set_ptr = nullptr)
    { return { icon_name, name, tooltip_text, nullptr, callback, nullptr, nullptr, false, set_ptr }; }

constexpr ToolBarItem ToolBarAction (const char * icon_name, const char * name, const char * tooltip_text,
                                     void (* toggled) (bool), void ** set_ptr = nullptr)
    { return { icon_name, name, tooltip_text, nullptr, nullptr, toggled, nullptr, false, set_ptr }; }

constexpr ToolBarItem ToolBarConstructor (QWidget * (*construct) (void))
    { return { nullptr, nullptr, nullptr, construct }; }

constexpr ToolBarItem ToolBarCustom (QWidget * item)
    { return { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, item }; }

constexpr ToolBarItem ToolBarSeparator ()
    { return { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, true }; }

#endif
