#ifndef AUDACIOUS_PLUGINS_DELAY_H
#define AUDACIOUS_PLUGINS_DELAY_H
/*
 * Delay.h
 *
 * Implements a multi-tap delay.
 * Added by michel on 2023-09-24
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

#include "utils.h"

/**
 * Delays samples by a size set at creation or via \c set_delay. Using the delay
 * is done simply by using \c get_oldest_and_set, that returns the value \c
 * get_delay() samples ago and overwrites it with the newest one provided. It is
 * also possible to get more recently added values using \c peek_back(), as long
 * as the specified delay there remains smaller than \c get_delay().
 *
 * \brief Implements a "multi-tap" delay.
 * \tparam T The trivial type of samples to delay.
 */
template<typename T>
class Delay
{
    static_assert(std::is_trivial_v<T>);
    static constexpr size_t MINIMUM_CAPACITY = 16;
    size_t capacity_;
    T * values_;
    size_t delay_;
    size_t ptr_;

    static size_t check_size(size_t size)
    {
        if (size > 0 && size < std::numeric_limits<size_t>::max() / sizeof(T))
        {
            return std::max(MINIMUM_CAPACITY, size);
        }
        throw std::invalid_argument(
            "CircularBuffer::check_size() failed: size zero or too big");
    }

    void reallocate(size_t allocate_size)
    {
        T * new_values = new T[allocate_size];
        if (values_)
        {
            delete[] values_;
        }
        values_ = new_values;
        capacity_ = allocate_size;
        delay_ = std::min(delay_, capacity_);
    }

public:
    /**
     * \brief Creates a delay with a delay of delay.
     * \param delay The initial delay.
     */
    Delay(size_t delay)
        : capacity_(check_size(delay)), values_(new T[delay]), delay_(delay),
          ptr_(0)
    {
    }

    /**
     * \brief Creates a delay with the minimum allowed delay.
     */
    Delay() : Delay(MINIMUM_CAPACITY) { delay_ = 1; }

    /**
     * \brief Gets the current delay set.
     * \return The number of values delayed.
     */
    [[nodiscard]] size_t get_delay() const { return delay_; }

    /**
     * \brief Sets the delay.
     * \param delay The new delay.
     * \param fill_value The value to fill all samples with initially.
     * \param shrink Indicates if the internal capacity should reallocate if the
     * required size is smaller.
     */
    void set_delay(size_t delay, const T & fill_value, bool shrink = false)
    {
        size_t new_capacity_needed = std::max((size_t)MINIMUM_CAPACITY, delay);
        if (new_capacity_needed >= capacity_ || shrink)
        {
            reallocate(new_capacity_needed);
        }
        ptr_ = 0;
        fill_with(fill_value);
    }

    /**
     * \brief Gets the oldest delayed value and overwrites it with the newest.
     * \param newest_value The new value to be written.
     * \return The value written get_delay() samples ago.
     */
    T get_oldest_and_set(const T & newest_value)
    {
        T result = values_[ptr_];
        values_[ptr_] = newest_value;
        ptr_++;
        ptr_ %= delay_;
        return result;
    }

    /**
     * \brief Returns an earlier written value.
     * \param back_in_time The earlier sample, where zero is the most recently
     * set value by get_oldest_and_set() and the maximum is get_delay() - 1.
     */
    inline T peek_back(size_t back_in_time)
    {
        return values_[(ptr_ + delay_ - back_in_time % delay_ - 1) % delay_];
    }

    /**
     * \brief Set all values with the same value.
     * \param value fill-value
     */
    void fill_with(const T & value)
    {
        for (size_t i = 0; i < capacity_; i++)
        {
            values_[i] = value;
        }
    }

    ~Delay()
    {
        if (values_)
        {
            delete[] values_;
            values_ = nullptr;
        }
        capacity_ = 0;
    }
};

#endif // AUDACIOUS_PLUGINS_DELAY_H
