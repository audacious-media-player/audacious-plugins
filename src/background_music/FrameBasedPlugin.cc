/*
 * Frame-based Audacious EffectPlugin
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

#include "FrameBasedPlugin.h"
#include "utils.h"
#include <cstdio>

FrameBasedPlugin::FrameBasedPlugin(const PluginInfo info, int order)
    : EffectPlugin(info, order, true)
{
}

Index<float> & FrameBasedPlugin::process(Index<float> & data)
{
    return process_buffer(data, false);
}


bool FrameBasedPlugin::init()
{
    print_debug("%s: init()\n", name())
    enabled = false;
    current_channels = 0;
    current_rate = 0;
    return on_init();
}

void FrameBasedPlugin::cleanup()
{
    print_debug("%s: cleanup()\n", name())
    bool en = enabled;
    enabled = false;
    frame_in.clear();
    frame_out.clear();
    current_channels = 0;
    current_rate = 0;
    channel_last_read = 0;
    on_cleanup(en);
}

bool FrameBasedPlugin::flush (bool force) {
    print_debug("%s: flush(%i)\n", name(), (int)force)
    return on_flush(force);
}

Index<float> & FrameBasedPlugin::finish(Index<float> & data,
                                        bool end_of_playlist)
{
    print_debug("%s: finish()\n", name())
    Index<float> & index = process_buffer(data, end_of_playlist);
    after_finished(end_of_playlist);
    return index;
}

void FrameBasedPlugin::start(int & channels, int & rate)
{
    print_debug("%s: start(channels=%i, rate=%i)\n", name(), channels, rate)
    if (channels <= 0 || rate <= 0)
    {
        fprintf(stderr,
                "%s: disabled because of invalid parameters: channels=%i, "
                "rate=%i.\n",
                name(), channels, rate);
        enabled = false;
        return;
    }
    update_config(false);
    int old_channels = current_channels;
    int old_rate = current_rate;
    current_channels = channels;
    current_rate = rate;

    frame_in.resize(current_channels);
    frame_out.resize(current_channels);

    if (current_channels != old_channels || current_rate != old_rate) {
        on_start(old_channels, old_rate);
    }

    enabled = true;
    channel_last_read = 0;
}

int FrameBasedPlugin::adjust_delay(int delay)
{
    if (enabled)
    {
        auto result =
            aud::rescale<int64_t>(latency(), current_rate, 1000) + delay;
        return (int)result;
    }
    return delay;
}

// New methods

Index<float> & FrameBasedPlugin::process_buffer(Index<float> & input,
                                                bool end_of_playlist)
{
    output.resize(0);
    if (!enabled)
    {
        disabled_frame_count++;
        if (is_multiple_of(10, disabled_frame_count, rate()))
        {
            print_debug("%s: disabled for %lf seconds\n", name(),
                   1.0 * disabled_frame_count / rate())
        }
        // We bluntly copy input to output
        output.move_from(input, 0, 0, input.len(), true, false);
        return output;
    }

    update_config(true);
    int output_samples = 0;

    // It is assumed data always contains a multiple of channels, but we don't
    // care.
    for (float sample : input)
    {
        frame_in[channel_last_read++] = sample;
        if (channel_last_read == current_channels)
        {
            // Processing happens per frame. Because of read-ahead there is not
            // always output available yet.
            if (offer_frame_return_if_output(frame_in, frame_out))
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
