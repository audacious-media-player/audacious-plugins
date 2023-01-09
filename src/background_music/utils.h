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
#include <libaudcore/runtime.h>
#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>
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

template<typename T>
static constexpr size_t get_max_array_size()
{
    const size_t max = std::numeric_limits<size_t>::max();
    const size_t size = sizeof(T);
    return size > 0 ? max / size : max;
}

template<typename T>
static constexpr size_t verified_above_min_below_max(size_t elements,
                                                     size_t minimum = 1)
{
    if (elements > minimum && elements <= get_max_array_size<T>())
    {
        return elements;
    }
    throw std::invalid_argument(
        "verified_above_min_below_max(elements,minimum): number of elements "
        "below minimum or larger than maximum array size for type.");
}

template<typename T>
static T * allocate_if_valid(size_t elements, size_t limit)
{
    size_t valid_elements = verified_above_min_below_max<T>(elements, 1);
    if (limit == 0 || valid_elements <= limit)
    {
        return new T[elements];
    }
    throw std::invalid_argument("allocate_if_valid(elements,limit): number of "
                                "elements larger than supplied limit");
}

template<typename T>
struct ConfigValue
{
    static_assert(std::is_trivial_v<T>);
    const char * const section;
    const char * const name;
    const char * const variable;
    const char * const default_string;
    const T value_default;
    const T value_min;
    const T value_max;

    constexpr ConfigValue(const char * section_name)
        : section(section_name), name("no name"), variable("no_variable"),
          default_string("0"), value_default(0), value_min(0.0), value_max(1.0)
    {
    }
    constexpr ConfigValue(const char * section_, const char * name_,
                          const char * variable_, const char * default_string_,
                          T value_default_, T value_min_, T value_max_)
        : section(section_), name(name_), variable(variable_),
          default_string(default_string_), value_default(value_default_),
          value_min(value_min_), value_max(value_max_)
    {
    }

    constexpr ConfigValue withName(const char * name) const
    {
        return ConfigValue(section, name, variable, default_string,
                           value_default, value_min, value_max);
    }
    constexpr ConfigValue withVariable(const char * variable) const
    {
        return ConfigValue(section, name, variable, default_string,
                           value_default, value_min, value_max);
    }
    constexpr ConfigValue withDefault(T value,
                                      const char * default_string) const
    {
        return ConfigValue(section, name, variable, default_string, value,
                           value_min, value_max);
    }
    constexpr ConfigValue withMinimum(T value_min) const
    {
        return ConfigValue(section, name, variable, default_string,
                           value_default, value_min, value_max);
    }
    constexpr ConfigValue withMaximum(T value_max) const
    {
        return ConfigValue(section, name, variable, default_string,
                           value_default, value_min, value_max);
    }
    T get_value() const
    {
        if constexpr (std::is_same_v<double, T>)
        {
            return std::clamp(aud_get_double(section, variable), value_min,
                              value_max);
        }
        else if constexpr (std::is_same_v<bool, T>)
        {
            return aud_get_bool(section, variable);
        }
        else if constexpr (std::is_same_v<int, T>)
        {
            return std::clamp(aud_get_int(section, variable), value_min,
                              value_max);
        }
        else
        {
            return aud_get_str(section, variable);
        }
    }
};

using ConfigDouble [[maybe_unused]] = ConfigValue<double>;
using ConfigInt [[maybe_unused]] = ConfigValue<int>;
using ConfigBool [[maybe_unused]] = ConfigValue<double>;
using ConfigString [[maybe_unused]] = ConfigValue<const char *>;

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

#endif // AUDACIOUS_PLUGINS_UTILS_H
