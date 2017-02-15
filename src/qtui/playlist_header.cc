/*
 * playlist_header.cc
 * Copyright 2017 John Lindgren
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

#include "playlist_header.h"
#include "playlist_columns.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>

static void toggle_column (int col, bool on)
{
    int pos = -1;

    for (int i = 0; i < pl_num_cols; i ++)
    {
        if (pl_cols[i] == col)
        {
            pos = i;
            break;
        }
    }

    if (on)
    {
        if (pos >= 0)
            return;

        pl_cols[pl_num_cols ++] = col;
    }
    else
    {
        if (pos < 0)
            return;

        pl_num_cols --;
        for (int i = pos; i < pl_num_cols; i ++)
            pl_cols[i] = pl_cols[i + 1];
    }

    hook_call ("qtui update playlist columns", nullptr);

    pl_col_save ();
}

void PlaylistHeader::contextMenuEvent (QContextMenuEvent * event)
{
    auto menu = new QMenu (this);
    QAction * actions[PL_COLS];

    for (int c = 0; c < PL_COLS; c ++)
    {
        actions[c] = new QAction (_(pl_col_names[c]), menu);
        actions[c]->setCheckable (true);

        QObject::connect (actions[c], & QAction::toggled, [c] (bool on) {
            toggle_column (c, on);
        });

        menu->addAction (actions[c]);
    }

    for (int i = 0; i < pl_num_cols; i ++)
        actions[pl_cols[i]]->setChecked (true);

    menu->popup (event->globalPos ());
}
