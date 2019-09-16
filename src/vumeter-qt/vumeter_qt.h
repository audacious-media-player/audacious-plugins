/*
 * Copyright (c) 2017-2019 Marc Sanchez Fauste.
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

#ifndef __VUMETER_QT_H
#define __VUMETER_QT_H

#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/visualizer.h>
#include <QElapsedTimer>

class VUMeterQt : public VisPlugin
{

public:
    static const int max_channels = 20;
    static const int db_range = 96;
    static const char about[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;
    static const char * const prefs_defaults[];

    static constexpr PluginInfo info = {
        N_("VU Meter"),
        PACKAGE,
        about,
        & prefs,
        PluginQtOnly
    };

    constexpr VUMeterQt () : VisPlugin (info, Visualizer::MultiPCM) {}

    bool init ();

    void * get_qt_widget ();

    void clear ();
    void render_multi_pcm (const float * pcm, int channels);
    static void toggle_display_legend();

private:
    QElapsedTimer last_peak_times[max_channels]; // Time elapsed since peak was set
    QElapsedTimer render_timer;

    float get_db_on_range(float db);

};

#endif