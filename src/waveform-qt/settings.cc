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

#include "settings.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

#include <libaudcore/i18n.h>

#include "preload.h"

WaveformSettings::WaveformSettings(QWidget * parent) : QWidget(parent)
{
    auto * layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);

    m_button = new QPushButton(_("Preload Playlist Waveforms"), this);
    m_label = new QLabel(this);
    m_label->setMinimumWidth(90);

    layout->addWidget(m_button);
    layout->addWidget(m_label);
    layout->addStretch(1);

    QObject::connect(m_button, &QPushButton::clicked, this,
                     []() { run_playlist_preload(); });

    m_timer = new QTimer(this);
    QObject::connect(m_timer, &QTimer::timeout, this, [this]() { refresh(); });
    m_timer->start(150);

    refresh();
}

void WaveformSettings::refresh()
{
    bool running = g_preload_running.load();
    int done = g_preload_done.load();
    int total = g_preload_total.load();

    m_button->setEnabled(!running);

    if (running)
        m_label->setText(QString("%1/%2").arg(done).arg(total));
    else if (total > 0)
        m_label->setText(QString(_("Done (%1/%2)")).arg(done).arg(total));
    else
        m_label->setText(QString());
}

void * waveform_get_settings_widget() { return new WaveformSettings(); }
