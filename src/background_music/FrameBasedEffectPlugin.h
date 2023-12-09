#ifndef AUDACIOUS_PLUGINS_BGM_FRAMEBASEDEFFECTPLUGIN_H
#define AUDACIOUS_PLUGINS_BGM_FRAMEBASEDEFFECTPLUGIN_H
/*
 * audacious-plugins/FrameBasedEffectPlugin.h
 *
 * Added by michel on 2023-09-23
 * Copyright (C) 2015-2023 Michel Fleur.
 * Source https://github.com/emmef/audacious-plugins
 * Email audacious-plugins@emmef.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <libaudcore/plugin.h>
#include <libaudcore/ringbuf.h>

#include "Detection.h"

template<class D>
class FrameBasedEffectPlugin : public EffectPlugin
{
    static_assert(std::is_base_of_v<Detection, D>);

public:
    constexpr FrameBasedEffectPlugin(const PluginInfo info, int order)
        : EffectPlugin(info, order, true)
    {
    }
    virtual ~FrameBasedEffectPlugin() = default;

    bool init() final;
    void cleanup() final;

    void start(int & channels, int & rate) final;
    Index<float> & process(Index<float> & data) final;
    bool flush(bool force) final;
    Index<float> & finish(Index<float> & data, bool end_of_playlist) final;
    int adjust_delay(int delay) final;
    bool offer_frame_return_if_output();

    virtual bool after_init() { return true; }
    virtual void before_cleanup() {}

    Index<float> frame_in;
    RingBuf<float> read_ahead_buffer;
    Index<float> frame_out;
    Index<float> output;
    int processed_frames = 0;
    int current_channels = 0, current_rate = 0, channel_last_read = 0;
    D * detection = nullptr;
};

template<class D>
bool FrameBasedEffectPlugin<D>::init()
{
    if (!detection)
    {
        detection = new D;
    }
    return after_init();
}

template<class D>
void FrameBasedEffectPlugin<D>::cleanup()
{
    before_cleanup();
    read_ahead_buffer.destroy();
    output.clear();
    frame_in.clear();
    frame_out.clear();
    if (detection)
    {
        delete detection;
        detection = nullptr;
    }
}

template<class D>
void FrameBasedEffectPlugin<D>::start(int & channels, int & rate)
{
    current_channels = channels;
    current_rate = rate;
    channel_last_read = 0;
    processed_frames = 0;

    detection->start(channels, rate);
    // Set the initial values for the integrators to the center value
    // (naturally squared as we integrate squares)

    // As data is added, then fetched, we need 1 extra frame in the buffer.
    int alloc_size = current_channels * (detection->read_ahead() + 1);
    if (read_ahead_buffer.size() < alloc_size)
    {
        read_ahead_buffer.alloc(alloc_size);
    }

    frame_in.resize(current_channels);
    frame_out.resize(current_channels);

    flush(false);
}

template<class D>
Index<float> & FrameBasedEffectPlugin<D>::process(Index<float> & data)
{
    detection->update_config();

    int output_samples = 0;
    output.resize(0);

    // It is assumed data always contains a multiple of channels, but we don't
    // care.
    for (float sample : data)
    {
        frame_in[channel_last_read++] = sample;
        if (channel_last_read == current_channels)
        {
            // Processing happens per frame. Because of read-ahead there is not
            // always output available yet.
            if (offer_frame_return_if_output())
            {
                output.move_from(frame_out, 0, output_samples, current_channels,
                                 true, false);
                output_samples += current_channels;
            }
            channel_last_read = 0;
        }
    }

    return output;
}

template<class D>
bool FrameBasedEffectPlugin<D>::flush(bool force)
{
    read_ahead_buffer.discard();
    return true;
}

template<class D>
Index<float> & FrameBasedEffectPlugin<D>::finish(Index<float> & data,
                                                 bool end_of_playlist)
{
    return process(data);
}

template<class D>
int FrameBasedEffectPlugin<D>::adjust_delay(int delay)
{
    auto result = aud::rescale<int64_t>(
        read_ahead_buffer.len() / current_channels - 1, current_rate, 1000);
    result += delay;
    return (int)result;
}

template<class D>
bool FrameBasedEffectPlugin<D>::offer_frame_return_if_output()
{
    /**
     * Add samples to the delay buffer so that the detection can be
     * applied ion a predictive fashion.
     */
    read_ahead_buffer.copy_in(frame_in.begin(), current_channels);

    detection->detect(frame_in);

    if (processed_frames >= detection->read_ahead())
    {
        read_ahead_buffer.move_out(frame_out.begin(), current_channels);
        detection->apply_detect(frame_out);
        return true;
    }
    else
    {
        processed_frames++;
    }

    return false;
}

#endif // AUDACIOUS_PLUGINS_BGM_FRAMEBASEDEFFECTPLUGIN_H
