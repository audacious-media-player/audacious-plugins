/*
 * Copyright (c) 2026 0000xFFFF.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __WAVEFORM_CONTROLS_H__
#define __WAVEFORM_CONTROLS_H__

#include <QWidget>

class QPushButton;
class QLabel;
class QTimer;

/* Shown on the plugin's preferences page: a button to preload the
 * whole playlist's waveforms, and a label that polls preload progress
 * while it runs. */
class WaveformSettings : public QWidget
{
public:
    explicit WaveformSettings(QWidget * parent = nullptr);

private:
    void refresh();

    QPushButton * m_button;
    QLabel * m_label;
    QTimer * m_timer;
};

/* Factory matching the signature WidgetCustomQt() expects in
 * libaudcore/preferences.h. */
void * waveform_get_settings_widget();

#endif
