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
#include <QAbstractButton>

static QMessageBox * create_message_box (QMessageBox::Icon icon,
 const QString & title, const QString & message, QWidget * parent)
{
    auto msgbox = new QMessageBox (icon, title, message, QMessageBox::Close, parent);
    msgbox->setAttribute (Qt::WA_DeleteOnClose);
    msgbox->setTextInteractionFlags (Qt::TextSelectableByMouse);
    msgbox->button (QMessageBox::Close)->setText (audqt::translate_str (N_("_Close")));
    return msgbox;
}

static void add_message (QMessageBox * msgbox, QString message)
{
    QString old = msgbox->text ();
    if (old.count (QChar::LineFeed) >= 9)
        message = _("\n(Further messages have been hidden.)");
    if (! old.contains (message))
        msgbox->setText (old + QChar::LineFeed + message);
}

void DialogWindows::create_progress ()
{
    if (! m_progress)
    {
        m_progress = new QMessageBox (m_parent);
        m_progress->setAttribute (Qt::WA_DeleteOnClose);
        m_progress->setIcon (QMessageBox::Information);
        m_progress->setWindowTitle (_("Working ..."));
        m_progress->setWindowModality (Qt::WindowModal);
    }
}

void DialogWindows::show_error (const char * message)
{
    if (m_error)
        add_message (m_error, message);
    else
        m_error = create_message_box (QMessageBox::Critical, _("Error"), message, m_parent);

    m_error->show ();
}

void DialogWindows::show_info (const char * message)
{
    if (m_info)
        add_message (m_info, message);
    else
        m_info = create_message_box (QMessageBox::Information, _("Information"), message, m_parent);

    m_info->show ();
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
