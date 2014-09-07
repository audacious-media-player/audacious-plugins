/*
 * dialog_windows.cc
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

#include "dialog_windows.h"

#include <QMessageBox>

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>

DialogWindows::DialogWindows (QWidget * parent) :
    m_parent (parent)
{
    hook_associate ("ui show progress", show_progress, this);
    hook_associate ("ui show progress 2", show_progress_2, this);
    hook_associate ("ui hide progress", hide_progress, this);
    hook_associate ("ui show error", show_error, this);
}

DialogWindows::~DialogWindows ()
{
    hook_dissociate_full ("ui show progress", show_progress, this);
    hook_dissociate_full ("ui show progress 2", show_progress_2, this);
    hook_dissociate_full ("ui hide progress", hide_progress, this);
    hook_dissociate_full ("ui show error", show_error, this);
}

void DialogWindows::create_progress ()
{
    if (! m_progress)
    {
        m_progress = new QMessageBox (m_parent);
        m_progress->setIcon (QMessageBox::Information);
        m_progress->setText (_("Working ..."));
        m_progress->setStandardButtons (QMessageBox::NoButton);
        m_progress->setWindowModality (Qt::WindowModal);
    }
}

void DialogWindows::create_error (const char * message)
{
    if (! m_error)
    {
        m_error = new QMessageBox (m_parent);
        m_error->setIcon (QMessageBox::Warning);
        m_error->setWindowModality (Qt::WindowModal);
    }

    m_error->setText (message);
    m_error->show ();
}

void DialogWindows::show_progress (void * message, void * data)
{
    auto dw = (DialogWindows *) data;

    dw->create_progress ();
    dw->m_progress->setInformativeText ((const char *) message);
    dw->m_progress->show ();
}

void DialogWindows::show_progress_2 (void * message, void * data)
{
    auto dw = (DialogWindows *) data;

    dw->create_progress ();
    dw->m_progress->setText ((const char *) message);
    dw->m_progress->show ();
}

void DialogWindows::hide_progress (void *, void * data)
{
    auto dw = (DialogWindows *) data;

    if (dw->m_progress)
        dw->m_progress->hide ();
}

void DialogWindows::show_error (void * message, void * data)
{
    auto dw = (DialogWindows *) data;

    dw->create_error ((const char *) message);
}
