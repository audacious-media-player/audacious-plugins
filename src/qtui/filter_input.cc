/*
 * playlist.cc
 * Copyright 2014 Daniel (dmilith) Dettlaff
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

#include "filter_input.h"


FilterInput::FilterInput ()
{
    QTextEdit ("");
    setFocusPolicy (Qt::NoFocus); /* by default we want no focus here */
    setFixedHeight (26);
    setFixedWidth (120);
    setTabChangesFocus (true);
    setUndoRedoEnabled (true);
}

void FilterInput::keyPressEvent (QKeyEvent *e)
{
    if (e->key() == Qt::Key_Enter or e->key() == Qt::Key_Return)
    {
        e->ignore ();
        qDebug () << "Enter in filter input";
        setFocusPolicy (Qt::NoFocus);
        focusNextChild ();
    } else
        QTextEdit::keyPressEvent (e);
}
