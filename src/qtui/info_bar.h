/*
 * info_bar.h
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

#ifndef INFO_BAR_H
#define INFO_BAR_H

#include <QStaticText>
#include <QWidget>

#include <libaudcore/hook.h>
#include <libaudcore/visualizer.h>

class InfoVis;

class InfoBar : public QWidget
{
public:
    InfoBar (QWidget * parent = nullptr);

    void resizeEvent (QResizeEvent *);
    void paintEvent (QPaintEvent *);

private:
    void update_metadata_cb ();
    void update_cb ();

    const HookReceiver<InfoBar>
     hook1 {"tuple change", this, & InfoBar::update_metadata_cb},
     hook2 {"playback ready", this, & InfoBar::update_cb},
     hook3 {"playback stop", this, & InfoBar::update_cb};

    QPixmap m_art;
    QStaticText m_title, m_artist, m_album;
    InfoVis * m_vis;
};

#endif
