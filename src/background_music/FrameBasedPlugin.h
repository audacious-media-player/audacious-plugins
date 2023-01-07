#ifndef AUDACIOUS_PLUGINS_FRAMEBASEDPLUGIN_H
#define AUDACIOUS_PLUGINS_FRAMEBASEDPLUGIN_H
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

#include <libaudcore/plugin.h>
#include <stdexcept>

class FrameBasedPlugin : public EffectPlugin
{
public:

    bool init() final;
    void cleanup() final;

    void start(int & channels, int & rate) final;
    Index<float> & process(Index<float> & data) final;
    Index<float> & finish(Index<float> & data, bool end_of_playlist) final;
    int adjust_delay(int delay) final;
    bool flush (bool force) final;

    [[nodiscard]] virtual const char * name() const = 0;

protected:
    FrameBasedPlugin(PluginInfo info, int order);
    [[nodiscard]] virtual bool
    offer_frame_return_if_output(const Index<float> & in,
                                 Index<float> & out) = 0;
    virtual void update_config(bool is_processing) = 0;
    [[nodiscard]] virtual int latency() const = 0;
    virtual bool on_init() = 0;
    virtual void on_start(int previous_channels, int previous_rate) = 0;
    virtual void on_cleanup(bool was_enabled) = 0;
    virtual bool on_flush (bool force) = 0;
    virtual bool after_finished(bool end_of_playlist) = 0;
    virtual Index<float> & process_buffer(Index<float> & input,
                                          bool end_of_playlist);

    [[nodiscard]] int rate() const { return current_rate; }
    [[nodiscard]] int channels() const { return current_channels; }

private:
    Index<float> output;
    Index<float> frame_in;
    Index<float> frame_out;
    int current_channels = 0;
    int current_rate = 0;
    int channel_last_read = 0;
    bool enabled = false;
    uint16_t disabled_frame_count = 0;
};

#endif // AUDACIOUS_PLUGINS_FRAMEBASEDPLUGIN_H
