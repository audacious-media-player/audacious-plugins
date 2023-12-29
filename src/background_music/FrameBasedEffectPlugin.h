#ifndef AUDACIOUS_PLUGINS_BGM_FRAMEBASEDEFFECTPLUGIN_H
#define AUDACIOUS_PLUGINS_BGM_FRAMEBASEDEFFECTPLUGIN_H
/*
 * Background music (equal loudness) Plugin for Audacious
 * Copyright 2023 Michel Fleur
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
#include "DoubleIntegrationTimeRmsDetection.h"
#include <libaudcore/plugin.h>
#include <libaudcore/ringbuf.h>

class FrameBasedEffectPlugin : public EffectPlugin
{
    Index<float> frame_in;
    RingBuf<float> read_ahead_buffer;
    Index<float> frame_out;
    Index<float> output;
    int processed_frames = 0;
    int current_channels = 0, current_rate = 0, channel_last_read = 0;
    DoubleIntegrationTimeRmsDetection detection;

public:
    FrameBasedEffectPlugin(const PluginInfo & info, int order)
        : EffectPlugin(info, order, true)
    {
    }

    virtual ~FrameBasedEffectPlugin() = default;

    bool init() final
    {
        detection.init();
        return true;
    }

    void cleanup() final
    {
        read_ahead_buffer.destroy();
        output.clear();
        frame_in.clear();
        frame_out.clear();
    }

    void start(int & channels, int & rate) final
    {
        current_channels = channels;
        current_rate = rate;
        channel_last_read = 0;
        processed_frames = 0;

        detection.start(channels, rate);
        // Set the initial values for the integrators to the center value
        // (naturally squared as we integrate squares)

        // As data is added, then fetched, we need 1 extra frame in the buffer.
        const int alloc_size = current_channels * (detection.read_ahead() + 1);
        if (read_ahead_buffer.size() < alloc_size)
        {
            read_ahead_buffer.alloc(alloc_size);
        }

        frame_in.resize(current_channels);
        frame_out.resize(current_channels);

        flush(false);
    }

    Index<float> & process(Index<float> & data) final
    {
        detection.update_config();

        int output_samples = 0;
        output.resize(0);

        // It is assumed data always contains a multiple of channels, but we
        // don't care.
        for (const float sample : data)
        {
            frame_in[channel_last_read++] = sample;
            if (channel_last_read == current_channels)
            {
                // Processing happens per frame. Because of read-ahead there is
                // not always output available yet.
                if (offer_frame_return_if_output())
                {
                    output.move_from(frame_out, 0, output_samples,
                                     current_channels, true, false);
                    output_samples += current_channels;
                }
                channel_last_read = 0;
            }
        }

        return output;
    }

    bool flush(bool force) final
    {
        read_ahead_buffer.discard();
        return true;
    }

    Index<float> & finish(Index<float> & data, bool end_of_playlist) final
    {
        return process(data);
    }

    int adjust_delay(int delay) final
    {
        auto result = aud::rescale<int64_t>(
            read_ahead_buffer.len() / current_channels - 1, current_rate, 1000);
        result += delay;
        return static_cast<int>(result);
    }

    bool offer_frame_return_if_output()
    {
        /**
         * Add samples to the delay buffer so that the detection can be
         * applied ion a predictive fashion.
         */
        read_ahead_buffer.copy_in(frame_in.begin(), current_channels);

        detection.detect(frame_in);

        if (processed_frames >= detection.read_ahead())
        {
            read_ahead_buffer.move_out(frame_out.begin(), current_channels);
            detection.apply_detect(frame_out);
            return true;
        }
        else
        {
            processed_frames++;
        }

        return false;
    }
};

#endif // AUDACIOUS_PLUGINS_BGM_FRAMEBASEDEFFECTPLUGIN_H
