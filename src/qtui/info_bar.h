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

#include <QStaticText>
#include <QWidget>

#include <libaudcore/hook.h>
#include <libaudcore/visualizer.h>

#ifndef INFO_BAR_H
#define INFO_BAR_H

class InfoBar : public QWidget, Visualizer {
public:
    InfoBar (QWidget * parent = nullptr);
    ~InfoBar ();

    static constexpr int Spacing = 8;
    static constexpr int IconSize = 64;
    static constexpr int Height = IconSize + 2 * Spacing;

    static constexpr int VisBands = 12;
    static constexpr int VisWidth = 8 * VisBands + Spacing - 2;
    static constexpr int VisCenter = IconSize * 5 / 8 + Spacing;
    static constexpr int VisDelay = 2;
    static constexpr int VisFalloff = 2;

    QSize minimumSizeHint () const;
    void paintEvent (QPaintEvent *);

private:
    void update_cb ();
    void update_metadata_cb ();

    void render_freq (const float * freq);
    void clear ();

    const HookReceiver<InfoBar>
     hook1 {"tuple change", this, & InfoBar::update_metadata_cb},
     hook2 {"playback ready", this, & InfoBar::update_cb},
     hook3 {"playback stop", this, & InfoBar::update_cb};

    QLinearGradient m_gradient;
    QColor m_colors[VisBands], m_shadow[VisBands];

    QPixmap m_art;
    QStaticText m_title, m_artist, m_album;

    char m_bars[VisBands] {};
    char m_delay[VisBands] {};
};

#endif
