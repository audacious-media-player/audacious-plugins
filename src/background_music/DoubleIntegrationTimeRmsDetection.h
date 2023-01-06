#ifndef AUDACIOUS_PLUGINS_DOUBLE_INTEGRATION_TIME_RMS_DETECTION_H
#define AUDACIOUS_PLUGINS_DOUBLE_INTEGRATION_TIME_RMS_DETECTION_H
/*
 * audacious-plugins/DoubleIntegrationTimeRmsDetection.h
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

#include <cmath>
#include "Detection.h"
#include "Integrator.h"
#include "basic_config.h"
#include "utils.h"

class DoubleIntegrationTimeRmsDetection : public Detection
{
    static constexpr float SHORT_INTEGRATION = 0.2;
    static constexpr float LONG_INTEGRATION = 3.2;
    static constexpr double FUDGE_FACTOR = 2.5;

    Integrator<double> short_integration;
    Integrator<double> long_integration;
    double long_integrated = 0, short_integrated = 0;
    double center = 0.1;
    double maximum_amplification = 1;
    float amplify = 0;
    int read_ahead_ = 0;

public:
    [[nodiscard]] int read_ahead() const override { return read_ahead_; }
    /**
     * Determines how fast the step response for the square root of the
     * integrator reaches 1 - 1/e, the usual characteristic "RC" value for an
     * integrator.
     * @param integrator The integrator
     * @param maximum_samples After how many samples to bail out.
     * @return The sample count needed to reach teh desired value.
     */
    static int get_root_integrate_square_rc_time_samples(
        const Integrator<double> & integrator, int maximum_samples)
    {
        double integrated = 0;
        double rc_value = 1.0 - 1.0 / M_E;
        double sqr_value = rc_value * rc_value;
        for (int i = 0; i < maximum_samples; i++)
        {
            integrated = integrator.get_integrated(integrated, 1);
            if (integrated >= sqr_value)
            {
                return i;
            }
        }

        return maximum_samples;
    }

    constexpr DoubleIntegrationTimeRmsDetection() = default;

    void start(int channels, int rate) override
    {
        center = decibel_level_to_value(conf_target_level.get_value());
        long_integrated = center * center;
        short_integrated = long_integrated;
        // Configure the integrators
        short_integration = {SHORT_INTEGRATION, rate};
        long_integration = {LONG_INTEGRATION, rate};
        // Determine how much we must read ahead for the short integration
        // period to track signals quickly enough.
        read_ahead_ = get_root_integrate_square_rc_time_samples(
            short_integration, static_cast<int>(SHORT_INTEGRATION * 3.0 * static_cast<float >(rate)));
    }

    void update_config() override {
        center = decibel_level_to_value(conf_target_level.get_value());
        maximum_amplification =
            decibel_level_to_value(conf_maximum_amplification.get_value());
    }

    void detect(const Index<float> & frame_in) override
    {
        /*
         * Detection is based on a kind-of RMS value. The sum of the squared
         * sample values in each frame is integrated over two time periods. The
         * square root of those integrated values is a pretty nice indication of
         * loudness for a first implementation. The detection does nothing fancy
         * and just uses a weighted sum of the square roots of both
         * integrations. In the future it will use multiple frequency bands,
         * actual RMS, ear-loudness curves and actual perceived weighting of RMS
         * windows.
         */
        double square_sum = 0.0;
        for (float sample : frame_in)
        {
            square_sum += (sample * sample);
        }
        long_integration.integrate(long_integrated, square_sum);
        short_integration.integrate(short_integrated, square_sum);
        double detection = FUDGE_FACTOR *
            (sqrt(long_integrated) + 0.5 * sqrt(short_integrated)) / 1.5;
        double ratio = center / aud::max(detection, 1e-6);
        amplify = static_cast<float>(aud::clamp(ratio, 0.0, maximum_amplification));
    }

    void apply_detect(Index<float> & frame_out) override
    {
        for (float & sample : frame_out)
        {
            sample *= amplify;
        }
    }
};

#endif // AUDACIOUS_PLUGINS_DOUBLE_INTEGRATION_TIME_RMS_DETECTION_H
