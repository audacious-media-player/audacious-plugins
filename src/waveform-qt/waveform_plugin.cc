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

#include <libintl.h> /* must come before libaudcore/i18n.h to prevent macro-poisoning */

#include <libaudcore/drct.h>
#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

#include "settings.h"
#include "track_state.h"
#include "waveform_widget.h"

static void on_playback_ready(void *, void *)
{
    String fn = aud_drct_get_filename();
    if (fn)
        load_track_async((const char *)fn);
}

static const PreferencesWidget waveform_widgets[] = {
    WidgetCustomQt(waveform_get_settings_widget)};

static const PluginPreferences waveform_prefs = {{waveform_widgets}};

class QtWaveform : public VisPlugin
{
public:
    static constexpr PluginInfo info = {N_("Waveform"), PACKAGE, nullptr,
                                        &waveform_prefs, PluginQtOnly};

    constexpr QtWaveform() : VisPlugin(info, Visualizer::Freq) {}

    bool init() override;
    void cleanup() override;
    void * get_qt_widget() override;
    void clear() override;
    void render_freq(const float * freq) override;
};

EXPORT QtWaveform aud_plugin_instance;

bool QtWaveform::init()
{
    hook_associate("playback ready", (HookFunction)on_playback_ready, nullptr);
    return true;
}

void QtWaveform::cleanup()
{
    hook_dissociate("playback ready", (HookFunction)on_playback_ready);
    track_state_cleanup();
}

void QtWaveform::render_freq(const float * freq) {}

void QtWaveform::clear()
{
    WaveformWidget * w = waveform_get_active_widget();
    if (w)
        w->update();
}

void * QtWaveform::get_qt_widget() { return waveform_get_container_widget(); }
