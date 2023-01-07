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

#include <cmath>
#include <cstdint>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/ringbuf.h>
#include <libaudcore/runtime.h>

/* Response times for (integration) */
#define SHORT_INTEGRATION 0.2
#define LONG_INTEGRATION 3.2

static const char * const CONFIG_SECTION_BACKGROUND_MUSIC = "background_music";
static const char * const CONFIG_VALUE_CENTER = "center";
static const char * const CONFIG_VALUE_DYNAMIC_RANGE = "dynamic_range";
static const char * const CONFIG_VALUE_MAXIMUM_AMPLIFICATION =
    "maximum_amplification";
/*
 * The standard "center" volume is set to 0.25 while the dynamic range is set to
 * zero, meaning all songs will be equally loud according to the detected
 * loudness, that is.
 */
static const char * const background_music_defaults[] = {
    CONFIG_VALUE_CENTER,
    "0.5",
    CONFIG_VALUE_DYNAMIC_RANGE,
    "0.0",
    CONFIG_VALUE_MAXIMUM_AMPLIFICATION,
    "30",
    nullptr};

static const PreferencesWidget background_music_widgets[] = {
    WidgetLabel(N_("<b>Background music</b>")),
    WidgetSpin(
        N_("Center volume:"),
        WidgetFloat(CONFIG_SECTION_BACKGROUND_MUSIC, CONFIG_VALUE_CENTER),
        {0.1, 0.3, 0.1}), // Limited until limiter feature arrives
    WidgetSpin(N_("Dynamic range:"),
               WidgetFloat(CONFIG_SECTION_BACKGROUND_MUSIC,
                           CONFIG_VALUE_DYNAMIC_RANGE),
               {0.0, 1.0, 0.1}),
    WidgetSpin(N_("Maximum amplification:"),
               WidgetFloat(CONFIG_SECTION_BACKGROUND_MUSIC,
                           CONFIG_VALUE_MAXIMUM_AMPLIFICATION),
               {1.0, 100.0, 10.0})};

static const PluginPreferences background_music_preferences = {
    {background_music_widgets}};

static const char background_music_about[] =
    N_("Background music (equal loudness) Plugin for Audacious\n"
       "Copyright 2023 Michel Fleur");

class BackgroundMusic : public EffectPlugin
{
public:
    static constexpr PluginInfo info = {N_("Background music (equal loudness)"),
                                        PACKAGE, background_music_about,
                                        &background_music_preferences};

    constexpr BackgroundMusic() : EffectPlugin(info, 0, true) {}

    bool init() override;
    void cleanup() override;

    void start(int & channels, int & rate) override;
    Index<float> & process(Index<float> & data) override;
    bool flush(bool force) override;
    Index<float> & finish(Index<float> & data, bool end_of_playlist) override;
    int adjust_delay(int delay) override;
};

EXPORT BackgroundMusic aud_plugin_instance;

class Integrator
{
    double history_multiply;
    double input_multiply;

public:
    Integrator() : history_multiply(0), input_multiply(1) {}
    explicit Integrator(double samples)
        : history_multiply(calculate_history_multiplier_from_samples(samples)),
          input_multiply(1.0 - history_multiply)
    {
    }
    Integrator(double time, int sample_rate) : Integrator(time * sample_rate) {}

    inline void integrate(double & history, double input) const
    {
        history = history_multiply * history + input_multiply * input;
    }

    static inline double
    calculate_history_multiplier_from_samples(double samples)
    {
        return fabs(samples) < 1e-6 ? 0 : exp(-1.0 / fabs(samples));
    }
};

static Integrator short_integration;
static Integrator long_integration;
static Index<float> frame_in;
static RingBuf<float> read_ahead_buffer;
static Index<float> frame_out;
static Index<float> output;
static int read_ahead;
static int processed_frames;
static int current_channels, current_rate, channel_last_read;
static double long_integrated, short_integrated;

static bool offer_frame_return_if_output(double center, double dynamic_range,
                                         double maximum_amplification)
{
    /**
     * Add samples to the delay buffer so that the detection can be
     * applied ion a predictive fashion.
     */
    read_ahead_buffer.copy_in(frame_in.begin(), current_channels);

    /*
     * Detection is based on a kind-of RMS value. The sum of the squared sample
     * values in each frame is integrated over two time periods. The square root
     * of those integrated values is a pretty nice indication of loudness
     * for a first implementation.
     * The detection does nothing fancy and just uses a weighted sum of the
     * square roots of both integrations. In the future it will use multiple
     * frequency bands, actual RMS, ear-loudness curves and actual perceived
     * weighting of RMS windows.
     */
    double square_sum = 0.0;
    for (float sample : frame_in)
    {
        square_sum += (sample * sample);
    }
    long_integration.integrate(long_integrated, square_sum);
    short_integration.integrate(short_integrated, square_sum);
    double detection =
        (sqrt(long_integrated) + 0.5 * sqrt(short_integrated)) / 1.5;
    double ratio = detection / center;
    double amplify =
        aud::clamp(pow(ratio, dynamic_range - 1), 0.0, maximum_amplification);

    if (processed_frames >= read_ahead)
    {
        read_ahead_buffer.move_out(frame_out.begin(), current_channels);
        for (float & sample : frame_out)
        {
            sample *= (float)amplify;
        }
        return true;
    }
    else
    {
        processed_frames++;
    }

    return false;
}

/**
 * Determines how fast the step response for the square root of the integrator
 * reaches 1 - 1/e, the usual characteristic "RC" value for an integrator.
 * @param integrator The integrator
 * @param maximum_samples After how many samples to bail out.
 * @return The sample count needed to reach teh desired value.
 */
static int
get_root_integrate_square_rc_time_samples(const Integrator & integrator,
                                          int maximum_samples)
{
    double integrated = 0;
    double rc_value = 1.0 - 1.0 / M_E;
    double sqr_value = rc_value * rc_value;
    for (int i = 0; i < maximum_samples; i++)
    {
        integrator.integrate(integrated, 1);
        if (integrated >= sqr_value)
        {
            return i;
        }
    }
    return maximum_samples;
}

bool BackgroundMusic::init()
{
    aud_config_set_defaults(CONFIG_SECTION_BACKGROUND_MUSIC,
                            background_music_defaults);
    return true;
}

void BackgroundMusic::cleanup()
{
    read_ahead_buffer.destroy();
    output.clear();
    frame_in.clear();
    frame_out.clear();
}

void BackgroundMusic::start(int & channels, int & rate)
{
    current_channels = channels;
    current_rate = rate;
    channel_last_read = 0;
    processed_frames = 0;

    // Set the initial values for the integrators to the center value
    // (naturally squared as we integrate squares)
    double center =
        aud_get_double(CONFIG_SECTION_BACKGROUND_MUSIC, CONFIG_VALUE_CENTER);
    long_integrated = center * center;
    short_integrated = long_integrated;
    // Configure the integrators
    short_integration = {SHORT_INTEGRATION, current_rate};
    long_integration = {LONG_INTEGRATION, current_rate};
    // Determine how much we must read ahead for the short integration period to
    // track signals quickly enough.
    read_ahead = get_root_integrate_square_rc_time_samples(
        short_integration, SHORT_INTEGRATION * current_rate * 3);

    // As data is added, then fetched, we need 1 extra frame in the buffer.
    int alloc_size = current_channels * (read_ahead + 1);
    if (read_ahead_buffer.size() < alloc_size)
    {
        read_ahead_buffer.alloc(alloc_size);
    }

    frame_in.resize(current_channels);
    frame_out.resize(current_channels);

    flush(false);
}

Index<float> & BackgroundMusic::process(Index<float> & data)
{
    double center =
        aud_get_double(CONFIG_SECTION_BACKGROUND_MUSIC, CONFIG_VALUE_CENTER);
    double range = aud_get_double(CONFIG_SECTION_BACKGROUND_MUSIC,
                                  CONFIG_VALUE_DYNAMIC_RANGE);
    double maximum_amplification = aud_get_double(
        CONFIG_SECTION_BACKGROUND_MUSIC, CONFIG_VALUE_MAXIMUM_AMPLIFICATION);

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
            if (offer_frame_return_if_output(center, range, maximum_amplification))
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

bool BackgroundMusic::flush(bool force)
{
    read_ahead_buffer.discard();
    return true;
}

Index<float> & BackgroundMusic::finish(Index<float> & data,
                                       bool end_of_playlist)
{
    return process(data);
}

int BackgroundMusic::adjust_delay(int delay)
{
    auto result = aud::rescale<int64_t>(
        read_ahead_buffer.len() / current_channels - 1, current_rate, 1000);
    result += delay;
    return (int)result;
}
