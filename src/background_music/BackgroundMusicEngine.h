#ifndef AUDACIOUS_PLUGINS_BACKGROUND_MUSIC_PLUGIN_H
#define AUDACIOUS_PLUGINS_BACKGROUND_MUSIC_PLUGIN_H
/*
 * Background music (equal loudness) engine.
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

#include <libaudcore/i18n.h>
#include <libaudcore/preferences.h>
#include <libaudcore/ringbuf.h>

#include "FrameBasedPlugin.h"
#include "Integrator.h"
#include "utils.h"

class BackgroundMusicEngine : public FrameBasedPlugin
{
    double target_level = 0.3;
    double range = 0;
    double maximum_amplification = 10;
    int processed_frames = 0;
    Index<float> frame_in;
    Index<float> frame_out;
    Integrator short_integration;
    Integrator long_integration;
    RingBuf<float> read_ahead_buffer;
    Index<float> output;
    int read_ahead = 0;
    double long_integrated = 0;
    double short_integrated = 0;

public:
    explicit BackgroundMusicEngine(int order);

    [[nodiscard]] bool
    offer_frame_return_if_output(const Index<float> & in,
                                 Index<float> & out) override;
    bool on_flush(bool force) override;
    [[nodiscard]] const char * name() const override;
    void update_config(bool is_processing) override;
    [[nodiscard]] int latency() const override;
    bool on_init() override;
    void on_start(int channels, int rate) override;
    void on_cleanup(bool was_enabled) override;
    bool after_finished(bool end_of_playlist) override;;
};

#endif // AUDACIOUS_PLUGINS_BACKGROUND_MUSIC_PLUGIN_H
