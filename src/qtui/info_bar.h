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

class InfoVis;
struct PixelSizes;

class InfoBar : public QWidget
{
public:
    InfoBar (QWidget * parent = nullptr);

    void resizeEvent (QResizeEvent *);
    void paintEvent (QPaintEvent *);

private:
    void update_title ();
    void update_album_art ();
    void next_song ();
    void do_fade ();

    void playback_ready_cb ();
    void playback_stop_cb ();
    void update_vis ();

    const HookReceiver<InfoBar>
     hook1 {"tuple change", this, & InfoBar::update_title},
     hook2 {"playback ready", this, & InfoBar::playback_ready_cb},
     hook3 {"playback stop", this, & InfoBar::playback_stop_cb},
     hook4 {"qtui toggle infoarea_vis", this, & InfoBar::update_vis};

    const Timer<InfoBar>
     fade_timer {TimerRate::Hz30, this, & InfoBar::do_fade};

    InfoVis * m_vis;
    const PixelSizes & ps;

    struct SongData {
        QPixmap art;
        QString orig_title;
        QStaticText title, artist, album;
        int alpha;
    };

    enum {Prev = 0, Cur = 1}; /* index into SongData array */

    SongData sd[2];
    bool m_stopped;
};

#endif
