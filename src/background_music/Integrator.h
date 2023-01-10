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
#include <limits>
#include <stdexcept>
#include <type_traits>

template<typename S>
class Integrator
{
    static_assert(std::is_floating_point_v<S>);

    S history_multiplier_;
    S input_multiplier_;

public:
    /**
     * Below this fraction of a sample the input multiplier cannot be
     * distinguished from 1.0 and the history multiplier would become de-normal.
     */
    static S minimum_samples()
    {
        static S min = -1.0 / (std::log(std::numeric_limits<S>::epsilon()));
        return min;
    }

    /**
     * Above this number of samples, the history multiplier cannot be
     * distinguished from 1.0, which leads to unstable results.
     */
    static S maximum_samples()
    {
        static S max = -1.0 / std::log(1.0 - std::numeric_limits<S>::epsilon());
        return max;
    }

    /**
     * Returns the history multiplier for integration over the provided number
     * of samples, that can be a fraction. If the fraction is smaller than
     * {@code minimum_samples}, zero is returned. If the number is bigger than
     * {@code maximum_samples} the history multiplier for {@code
     * maximum_samples} is returned.
     * @param samples The number of samples
     * @return the history multiplier that lies between [0 .. 1>.
     */
    static inline S calculate_history_multiplier_from_samples(S samples)
    {
        S absolute = fabs(samples);
        return absolute < minimum_samples()   ? 0
               : absolute > maximum_samples() ? exp(-1 / maximum_samples())
                                            : exp(-1.0 / absolute);
    }

    /**
     * Calculates the history multiplier from a given input multiplier and vice
     * versa.
     * @param multiplier The multiplier
     * @return The other multiplier
     */
    static inline S calculate_other_multiplier(S multiplier)
    {
        return 1 - multiplier;
    }

    /**
     * Calculates the number of samples for the provided history multiplier. The
     * result is always positive or zero, but not always a whole number. If the
     * history multiplier is not valid, std::invalid_argument is thrown.
     * The choice was made not to return -1 on error, as that easily leads to
     * segfaults.
     */
    static inline S
    calculate_samples_from_history_multiplier(S history_multiplier)
    {
        double absolute = fabs(history_multiplier);
        if (absolute < std::numeric_limits<S>::epsilon()())
        {
            return 0;
        }
        else if (absolute <= (1 - std::numeric_limits<S>::epsilon()()))
        {
            return 1.0 / log(absolute);
        }
        throw std::invalid_argument(
            "IntegratorBase::calculate_samples_from_history_multiplier("
            "multiplier): multiplier out of range.");
    }

    /**
     * By default, the integrator does not integrate and just passes-through the
     * input.
     */
    inline Integrator() : history_multiplier_(0), input_multiplier_(1) {}

    /**
     * Creates an integrator over the provided number of samples.
     * @param samples The number of samples
     */
    [[maybe_unused]] inline explicit Integrator(S samples)
        : history_multiplier_(
              calculate_history_multiplier_from_samples(samples)),
          input_multiplier_(1.0 - history_multiplier_)
    {
    }

    /**
     * Creates an integrator of the the provided number of samples that is
     * calculated from the time and sample rate.
     * @param time The number of time units
     * @param sample_rate The sample-rate samples per unit of time
     */
    [[maybe_unused]] inline Integrator(S time, S sample_rate)
        : Integrator(time * sample_rate)
    {
    }
    template<typename T1, typename T2>
    [[maybe_unused]] inline Integrator(T1 time, T2 sample_rate)
        : Integrator(static_cast<S>(time) * static_cast<S>(sample_rate))
    {
        // Replace with requires in on C++20 adoption
        static_assert(std::is_arithmetic_v<T1>);
        static_assert(std::is_arithmetic_v<T2>);
    }

    /**
     * Integrates value input using history as integrated value.
     * @param history The historic integrated value
     * @param input The new input
     */
    inline void integrate(S & history, S input) const
    {
        history = history_multiplier_ * history + input_multiplier_ * input;
    }

    /**
     * Getters
     */
    [[nodiscard]] S history_multiplier() const { return history_multiplier_; }

    [[nodiscard]] S input_multiplier() const { return input_multiplier_; }

    /**
     * Returns the number of samples that is integrated over. This is calculated
     * from the constants and not totally accurate.
     */
    [[nodiscard]] S estimate_integration_samples() const
    {
        return -1 / log(history_multiplier_);
    }
};

template<typename S>
class ScaledIntegrator
{
    S scale_ = 1.0;
    S integrated_ = 0;
    Integrator<S> integrator_;

public:
    explicit ScaledIntegrator(const Integrator<S> & s) : integrator_(s) {}
    ScaledIntegrator() = default;

    inline void integrate(S input)
    {
        integrator_.integrate(integrated_, input);
    }

    ScaledIntegrator<S> & operator=(const Integrator<S> & source)
    {
        integrator_ = source;
        return *this;
    }

    void set_value(S new_value) { integrated_ = new_value; }

    void set_scale(S new_scale)
    {
        if (new_scale >= 0 && new_scale <= 1e6)
        {
            scale_ = new_scale;
        }
    }

    [[nodiscard]] S integrated() const { return scale_ * integrated_; }
    [[nodiscard]] S scale() const { return scale_; }
    [[nodiscard]] inline const Integrator<S> & integrator() const
    {
        return integrator_;
    }
};

template<typename S>
class DoubleIntegrator
{
    double intermediate_ = 0;
    double integrated_ = 0;
    Integrator<S> integrator_;

public:
    explicit DoubleIntegrator(const Integrator<S> & s) : integrator_(s) {}
    DoubleIntegrator() = default;

    inline double integrate(double input)
    {
        integrator_.integrate(intermediate_, input);
        integrator_.integrate(integrated_, intermediate_);
        return integrated_;
    }

    DoubleIntegrator<S> & operator=(const Integrator<S> & source)
    {
        integrator_ = source;
        return *this;
    }

    void set_value(double new_value)
    {
        integrated_ = intermediate_ = new_value;
    }

    [[nodiscard]] double integrated() const { return integrated_; }
    [[nodiscard]] inline const Integrator<S> & integrator() const
    {
        return integrator_;
    }
};

template<typename S>
class MultiIntegrator
{

public:
    struct Entry
    {
        ScaledIntegrator<S> integrator_;
        int look_ahead_;
        int reduced_look_ahead_;
    };

    MultiIntegrator(size_t number_of_entries)
        : entry_(allocate_if_valid<Entry>(number_of_entries, 20)),
          number_of_integrators_(number_of_entries)
    {
        reset_lookahead();
    }

    [[nodiscard]] const Integrator<S> & integrator(size_t i) const
    {
        if (i < (size_t)number_of_integrators_)
        {
            return entry_[i].integrator_.integrator();
        }
        throw std::out_of_range(
            "MultiIntegratorBase::integrator(i): index out of bounds");
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
            "MultiIntegratorBase::scale(i): index out of bounds");
    }

    [[nodiscard]] const Integrator<S> & smoothing() const
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

    void set_multipliers(size_t i, const Integrator<S> & multipliers)
    {
        if (i < (size_t)number_of_integrators_)
        {
            entry_[i].integrator_ = multipliers;
            return;
        }
        throw std::out_of_range(
            "MultiIntegratorBase::set_multipliers: index out of bounds");
    }

    void set_scale(size_t i, double scale)
    {
        if (i < (size_t)number_of_integrators_)
        {
            entry_[i].integrator_.set_scale(scale);
            return;
        }
        throw std::out_of_range(
            "MultiIntegratorBase::set_scale: index out of bounds");
    }

    void set_smoothing(const Integrator<S> & new_smoothing)
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
            max_look_ahead_ = get_samples_until_threshold_step(
                entry_[integrator_with_maximum_lookahead]
                    .integrator_.integrator(),
                smoothing_.integrator(), threshold, max_samples_to_try);
            entry_[integrator_with_maximum_lookahead].look_ahead_ =
                max_look_ahead_;
            for (size_t n = 0; n < number_of_integrators_; n++)
            {
                if (n != integrator_with_maximum_lookahead)
                {
                    entry_[n].look_ahead_ = get_samples_until_threshold_step(
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
            "MultiIntegratorBase::calculate_lookahead: index out of bounds");
    }

    ~MultiIntegrator() = default;

    /**
     * Determines how many samples it takes until the threshold is reach when a
     * step response is provided to the integrators.
     *
     * @param squares_integrator The integrator
     * @param maximum_samples After how many samples to bail out.
     * @return The sample count needed to reach teh desired value.
     */
    static int
    get_samples_until_threshold_step(const Integrator<S> & squares_integrator,
                                     const Integrator<S> & smoother_integrator,
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
        return rla == max_look_ahead_ ? last_read_ : buffer_.peek_back(rla);
    }

    virtual void on_set_value(double value) {}

    [[maybe_unused]] int max_look_ahead() const { return max_look_ahead_; }

private:
    Entry * entry_;
    size_t number_of_integrators_;
    DoubleIntegrator<S> smoothing_;
    int max_look_ahead_ = 0;
    CircularBuffer<double> buffer_;
    double last_read_;
};

#endif // AUDACIOUS_PLUGINS_INTEGRATOR_H
