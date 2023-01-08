#ifndef AUDACIOUS_PLUGINS_INTEGRATOR_H
#define AUDACIOUS_PLUGINS_INTEGRATOR_H
/*
 * RC-circuit style integrator.
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
#include "utils.h"
#include <cmath>
#include <stdexcept>

class Integrator
{
protected:
    double history_multiply;
    double input_multiply;

public:
    inline Integrator() : history_multiply(0), input_multiply(1) {}
    [[maybe_unused]] inline explicit Integrator(double samples)
        : history_multiply(calculate_history_multiplier_from_samples(samples)),
          input_multiply(1.0 - history_multiply)
    {
    }

    [[maybe_unused]] inline Integrator(double time, int sample_rate)
        : Integrator(time * sample_rate)
    {
    }

    inline void integrate(double & history, double input) const
    {
        history = history_multiply * history + input_multiply * input;
    }

    [[nodiscard]] double history_multiplication() const
    {
        return history_multiply;
    }

    [[nodiscard]] double input_multiplication() const { return input_multiply; }

    [[nodiscard]] double estimate_integration_samples() const
    {
        return calculate_samples_from_history_multiplier(history_multiply);
    }

    static inline double
    calculate_history_multiplier_from_samples(double samples)
    {
        double absolute = fabs(samples);
        return absolute < 1e-2 ? 0 : exp(-1.0 / absolute);
    }

    static inline double
    calculate_samples_from_history_multiplier(double history_multiplier)
    {
        return history_multiplier > std::numeric_limits<double>::epsilon()
                   ? -1.0 / log(history_multiplier)
                   : 0;
    }

    /**
     * Determines how many samples it takes until the threshold is reach when a
     * step response is provided to the integrators.
     *
     * @param squares_integrator The integrator
     * @param maximum_samples After how many samples to bail out.
     * @return The sample count needed to reach teh desired value.
     */
    static int
    get_samples_until_threshold_step(const Integrator & squares_integrator,
                                     const Integrator & smoother_integrator,
                                     double threshold, int maximum_samples)
    {
        double integrated = 0;
        double smooth_intermediate = 0;
        double smooth_integrated = 0;
        for (int i = 0; i < maximum_samples; i++)
        {
            squares_integrator.integrate(integrated, 1);
            smoother_integrator.integrate(smooth_intermediate, integrated);
            smoother_integrator.integrate(smooth_integrated,
                                          smooth_intermediate);
            if (smooth_integrated >= threshold)
            {
                return i;
            }
        }
        return maximum_samples;
    }
};

class ScaledIntegrator
{
    double scale_ = 1.0;
    double integrated_ = 0;
    Integrator integrator_;

public:
    explicit ScaledIntegrator(const Integrator & s) : integrator_(s) {}
    ScaledIntegrator() = default;

    inline void integrate(double input)
    {
        integrator_.integrate(integrated_, input);
    }

    ScaledIntegrator & operator=(const Integrator & source)
    {
        integrator_ = source;
        return *this;
    }

    void set_value(double new_value) { integrated_ = new_value; }

    void set_scale(double new_scale)
    {
        if (new_scale >= 0 && new_scale <= 1e6)
        {
            scale_ = new_scale;
        }
    }

    [[nodiscard]] double integrated() const { return scale_ * integrated_; }
    [[nodiscard]] double scale() const { return scale_; }
    [[nodiscard]] inline const Integrator & integrator() const
    {
        return integrator_;
    }
};

class DoubleIntegrator
{
    double intermediate_ = 0;
    double integrated_ = 0;
    Integrator integrator_;

public:
    explicit DoubleIntegrator(const Integrator & s) : integrator_(s) {}
    DoubleIntegrator() = default;

    inline double integrate(double input)
    {
        integrator_.integrate(intermediate_, input);
        integrator_.integrate(integrated_, intermediate_);
        return integrated_;
    }

    DoubleIntegrator & operator=(const Integrator & source)
    {
        integrator_ = source;
        return *this;
    }

    void set_value(double new_value)
    {
        integrated_ = intermediate_ = new_value;
    }

    [[nodiscard]] double integrated() const { return integrated_; }
    [[nodiscard]] inline const Integrator & integrator() const
    {
        return integrator_;
    }
};

class MultiIntegrator
{

public:
    struct Entry
    {
        ScaledIntegrator integrator_;
        int look_ahead_;
        int reduced_look_ahead_;
    };

    MultiIntegrator(size_t number_of_entries)
        : entry_(allocate_if_valid<Entry>(number_of_entries, 20)),
          number_of_integrators_(number_of_entries)
    {
        reset_lookahead();
    }

    [[nodiscard]] const Integrator & integrator(size_t i) const
    {
        if (i < (size_t)number_of_integrators_)
        {
            return entry_[i].integrator_.integrator();
        }
        throw std::out_of_range(
            "MultiIntegrator::integrator(i): index out of bounds");
    }

    const Entry & get_entry(size_t i)
    {
        return entry_[i % number_of_integrators_];
    }

    [[nodiscard]] double scale(size_t i) const
    {
        if (i < (size_t)number_of_integrators_)
        {
            return entry_[i].integrator_.scale();
        }
        throw std::out_of_range(
            "MultiIntegrator::scale(i): index out of bounds");
    }

    [[nodiscard]] const Integrator & smoothing() const
    {
        return smoothing_.integrator();
    }

    [[nodiscard]] size_t number_of_integrators() const
    {
        return number_of_integrators_;
    }

    [[nodiscard]] double integrated() const { return smoothing_.integrated(); }

    void integrate(double input)
    {
        last_read_ = buffer_.get_and_set(input);

        smoothing_.integrate(on_integration());
    }

    virtual double on_integration()
    {
        double max_integrated = do_integrate_from_buffer(0);
        for (size_t i = 1; i < number_of_integrators_; i++)
        {
            max_integrated =
                std::max(max_integrated, do_integrate_from_buffer(i));
        }
        return max_integrated;
    }

    void set_value(double new_value)
    {
        entry_[0].integrator_.set_value(new_value);
        smoothing_.set_value(new_value);
        if (number_of_integrators_ > 1)
        {
            for (size_t n = 1; n < number_of_integrators_; n++)
            {
                entry_[n].integrator_.set_value(new_value);
            }
        }
        last_read_ = new_value;
        buffer_.fill_with(new_value);
        on_set_value(new_value);
    }

    void set_multipliers(size_t i, const Integrator & multipliers)
    {
        if (i < (size_t)number_of_integrators_)
        {
            entry_[i].integrator_ = multipliers;
            return;
        }
        throw std::out_of_range(
            "MultiIntegrator::set_multipliers: index out of bounds");
    }

    void set_scale(size_t i, double scale)
    {
        if (i < (size_t)number_of_integrators_)
        {
            entry_[i].integrator_.set_scale(scale);
            return;
        }
        throw std::out_of_range(
            "MultiIntegrator::set_scale: index out of bounds");
    }

    void set_smoothing(const Integrator & new_smoothing)
    {
        smoothing_ = new_smoothing;
    }

    void reset_lookahead()
    {
        for (size_t i = 0; i < number_of_integrators_; i++)
        {
            entry_[i].look_ahead_ = 0;
            entry_[i].reduced_look_ahead_ = 0;
        }
    }

    int
    prepare_lookahead(size_t integrator_with_maximum_lookahead,
                      double fill_value,
                      int max_samples_to_try = std::numeric_limits<int>::max())
    {
        if (integrator_with_maximum_lookahead < number_of_integrators_)
        {
            double threshold = 1 - 1 / M_E;
            threshold *= threshold;
            max_look_ahead_ = Integrator::get_samples_until_threshold_step(
                entry_[integrator_with_maximum_lookahead]
                    .integrator_.integrator(),
                smoothing_.integrator(), threshold, max_samples_to_try);
            entry_[integrator_with_maximum_lookahead].look_ahead_ =
                max_look_ahead_;
            for (size_t n = 0; n < number_of_integrators_; n++)
            {
                if (n != integrator_with_maximum_lookahead)
                {
                    entry_[n].look_ahead_ =
                        Integrator::get_samples_until_threshold_step(
                            entry_[n].integrator_.integrator(),
                            smoothing_.integrator(), threshold,
                            max_look_ahead_ + 1);
                }
            }
            for (size_t n = 0; n < number_of_integrators_; n++)
            {
                entry_[n].reduced_look_ahead_ =
                    std::max(0, max_look_ahead_ - entry_[n].look_ahead_);
            }
            buffer_.set_size(max_look_ahead_, fill_value, false);
            return max_look_ahead_;
        }
        throw std::out_of_range(
            "MultiIntegrator::calculate_lookahead: index out of bounds");
    }

    ~MultiIntegrator() = default;

protected:
    double do_integrate_from_buffer(size_t i)
    {
        entry_[i].integrator_.integrate(get_buffered_value_to_integrate(i));
        return entry_[i].integrator_.integrated();
    }

    [[maybe_unused]] double integrate_value(size_t i, double value)
    {
        Entry & entry = entry_[i];
        entry.integrator_.integrate(value);
        return entry.integrator_.integrated();
    }

    [[maybe_unused]] double get_buffered_value_to_integrate(size_t i)
    {
        auto rla = entry_[i].reduced_look_ahead_;
        return rla == max_look_ahead_
                   ? last_read_
                   : buffer_.peek_back(rla);
    }

    virtual void on_set_value(double value) {}

    [[maybe_unused]] int max_look_ahead() const { return max_look_ahead_; }

private:
    Entry * entry_;
    size_t number_of_integrators_;
    DoubleIntegrator smoothing_;
    int max_look_ahead_ = 0;
    CircularBuffer<double> buffer_;
    double last_read_;
};

#endif // AUDACIOUS_PLUGINS_INTEGRATOR_H
