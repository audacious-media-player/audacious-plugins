#ifndef AUDACIOUS_PLUGINS_UTILS_H
#define AUDACIOUS_PLUGINS_UTILS_H
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
#include <limits>
#include <type_traits>


#ifdef BACKGROUND_MUSIC_PRINT_DEBUG_MESSAGES
#define print_debug(...) printf(__VA_ARGS__);
static constexpr bool enabled_print_debug = true;
#else
#define print_debug(...)
static constexpr bool enabled_print_debug = false;
#endif


template<typename T, typename V>
static inline constexpr T narrow_clamp(const V & v)
{
    auto value = v + static_cast<T>(0);
    using W = decltype(value);

    if constexpr (std::numeric_limits<W>::lowest() <
                  std::numeric_limits<T>::lowest())
    {
        if (value < static_cast<W>(std::numeric_limits<T>::lowest()))
        {
            return std::numeric_limits<T>::lowest();
        }
    }
    if constexpr (std::numeric_limits<W>::max() > std::numeric_limits<T>::max())
    {
        if (value > static_cast<W>(std::numeric_limits<T>::max()))
        {
            return std::numeric_limits<T>::max();
        }
    }
    return static_cast<T>(value);
}

template<typename M, typename N, typename D>
static inline bool is_multiple_of(M multiple_of, N numerator, D denominator)
{
    static_assert(std::is_integral_v<M>);
    static_assert(std::is_integral_v<N>);
    static_assert(std::is_integral_v<D>);
    using W = decltype(numerator + denominator + multiple_of);

    return (numerator != 0) && (denominator != 0) && (multiple_of != 0) &&
           ((static_cast<W>(numerator) / static_cast<W>(denominator)) %
            static_cast<W>(multiple_of)) == 0;
}


template <typename T>
class CircularBuffer {
    static_assert(std::is_trivial_v<T>);
    static constexpr size_t MINIMUM_CAPACITY = 16;
    size_t capacity_;
    T *value_;
    size_t size_;
    size_t ptr_;

    static size_t check_size(size_t size) {
        if (size > 0 && size < std::numeric_limits<size_t>::max() / sizeof(T)) {
            return std::max(MINIMUM_CAPACITY, size);
        }
        throw std::invalid_argument("CircularBuffer::check_size() failed: size zero or too big");
    }

    void reallocate(size_t allocate_size) {
        T * new_value = new T[allocate_size];
        if (value_) {
            delete[] value_;
        }
        value_ = new_value;
        capacity_ = allocate_size;
    }

public:

    CircularBuffer(size_t size) : capacity_(check_size(size)), value_(new T[size]), size_(size), ptr_(0) {
    }

    CircularBuffer() : CircularBuffer(MINIMUM_CAPACITY) {
        size_ = 1;
    }

    [[nodiscard]] size_t size() const { return size_; }

    void set_size(size_t size, const T& fill_value, bool shrink = false) {
        size_t new_capacity_needed = std::max((size_t)16, size);
        if (new_capacity_needed >= capacity_ || shrink) {
            reallocate(new_capacity_needed);
        }
        size_ = std::max((size_t)1, size);
        ptr_ = 0;
        fill_with(fill_value);
    }

    /**
     * Return the value size() ago and set a new value.
     */
    T get_and_set(const T &value) {
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
    inline T peek_back(size_t back_in_time) {
        return value_[(ptr_ + size_ - back_in_time % size_ - 1) % size_];
    }

    void fill_with(const T& value) {
        for (size_t i = 0; i < capacity_; i++) {
            value_[i] = value;
        }
    }

    ~CircularBuffer() {
        if (value_) {
            delete[] value_;
            value_ = nullptr;
        }
        capacity_ = 0;
    }
};


#endif // AUDACIOUS_PLUGINS_UTILS_H
