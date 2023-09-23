#ifndef AUDACIOUS_PLUGINS_CIRCULARBUFFER_H
#define AUDACIOUS_PLUGINS_CIRCULARBUFFER_H
/*
 * audacious-plugins/CircularBuffer.h
 *
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

template<typename T>
class CircularBuffer
{
    static_assert(std::is_trivial_v<T>);
    static constexpr size_t MINIMUM_CAPACITY = 16;
    size_t capacity_;
    T * value_;
    size_t size_;
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
        T * new_value = new T[allocate_size];
        if (value_)
        {
            delete[] value_;
        }
        value_ = new_value;
        capacity_ = allocate_size;
    }

public:
    CircularBuffer(size_t size)
        : capacity_(check_size(size)), value_(new T[size]), size_(size), ptr_(0)
    {
    }

    CircularBuffer() : CircularBuffer(MINIMUM_CAPACITY) { size_ = 1; }

    [[nodiscard]] size_t size() const { return size_; }

    void set_size(size_t size, const T & fill_value, bool shrink = false)
    {
        size_t new_capacity_needed = std::max((size_t)16, size);
        if (new_capacity_needed >= capacity_ || shrink)
        {
            reallocate(new_capacity_needed);
        }
        size_ = std::max((size_t)1, size);
        ptr_ = 0;
        fill_with(fill_value);
    }

    /**
     * Return the value size() ago and set a new value.
     */
    T get_and_set(const T & value)
    {
        T result = value_[ptr_];
        value_[ptr_] = value;
        ptr_++;
        ptr_ %= size_;
        return result;
    }

    /**
     * Returns an earlier written value, where a parameter of zero returns the
     * last value provided in get_and_set.
     */
    inline T peek_back(size_t back_in_time)
    {
        return value_[(ptr_ + size_ - back_in_time % size_ - 1) % size_];
    }

    void fill_with(const T & value)
    {
        for (size_t i = 0; i < capacity_; i++)
        {
            value_[i] = value;
        }
    }

    ~CircularBuffer()
    {
        if (value_)
        {
            delete[] value_;
            value_ = nullptr;
        }
        capacity_ = 0;
    }
};

#endif // AUDACIOUS_PLUGINS_CIRCULARBUFFER_H
