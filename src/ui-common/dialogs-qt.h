/*
 * dialogs-qt.h
 * Copyright 2014 John Lindgren and Michał Lipski
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

#ifndef DIALOG_WINDOWS_H
#define DIALOG_WINDOWS_H

#include <libaudcore/hook.h>

class QMessageBox;
class QWidget;

class DialogWindows
{
public:
    DialogWindows (QWidget * parent) :
        m_parent (parent) {}

private:
    QWidget * m_parent;
    QMessageBox * m_progress = nullptr;

    void create_progress ();
    void show_error (const char * message);
    void show_info (const char * message);
    void show_progress (const char * message);
    void show_progress_2 (const char * message);
    void hide_progress ();

    const HookReceiver<DialogWindows, const char *>
     show_hook1 {"ui show progress", this, & DialogWindows::show_progress},
     show_hook2 {"ui show progress 2", this, & DialogWindows::show_progress_2},
     show_hook3 {"ui show error", this, & DialogWindows::show_error},
     show_hook4 {"ui show info", this, & DialogWindows::show_info};
    const HookReceiver<DialogWindows>
     hide_hook {"ui hide progress", this, & DialogWindows::hide_progress};
};

#endif
