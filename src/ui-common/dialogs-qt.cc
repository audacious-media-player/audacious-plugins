/*
 * dialogs-qt.cc
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

#include "dialogs-qt.h"

#include <libaudcore/i18n.h>
#include <libaudqt/libaudqt.h>

void DialogWindows::create_progress ()
{
    if (! m_progress)
    {
        m_progress = new QMessageBox (m_parent);
        m_progress->setIcon (QMessageBox::Information);
        m_progress->setWindowTitle (_("Working ..."));
        m_progress->setWindowModality (Qt::WindowModal);
    }
}

void DialogWindows::show_error (const char * message)
{
    audqt::simple_message (_("Error"), message, QMessageBox::Critical);
}

void DialogWindows::show_info (const char * message)
{
    audqt::simple_message (_("Information"), message, QMessageBox::Information);
}

void DialogWindows::show_progress (const char * message)
{
    create_progress ();
    m_progress->setText (message);
    m_progress->show ();
}

void DialogWindows::show_progress_2 (const char * message)
{
    create_progress ();
    m_progress->setInformativeText (message);
    m_progress->show ();
}

void DialogWindows::hide_progress ()
{
    if (m_progress)
        m_progress->hide ();
}
