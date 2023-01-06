#ifndef AUDACIOUS_PLUGINS_CONFIGVALUE_H
#define AUDACIOUS_PLUGINS_CONFIGVALUE_H
/*
 * audacious-plugins/ConfigValue.h
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
#include <algorithm>
#include <cstddef>
#include <libaudcore/runtime.h>
#include <limits>
#include <stdexcept>
#include <type_traits>

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

    [[maybe_unused]] constexpr explicit ConfigValue(const char * section_name)
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

    constexpr ConfigValue withName(const char * with_name) const
    {
        return ConfigValue(section, with_name, variable, default_string,
                           value_default, value_min, value_max);
    }
    constexpr ConfigValue withVariable(const char * with_variable) const
    {
        return ConfigValue(section, name, with_variable, default_string,
                           value_default, value_min, value_max);
    }
    constexpr ConfigValue withDefault(T value,
                                      const char * with_default_string) const
    {
        return ConfigValue(section, name, variable, with_default_string, value,
                           value_min, value_max);
    }
    constexpr ConfigValue withMinimum(T with_value_min) const
    {
        return ConfigValue(section, name, variable, default_string,
                           value_default, with_value_min, value_max);
    }
    constexpr ConfigValue withMaximum(T with_value_max) const
    {
        return ConfigValue(section, name, variable, default_string,
                           value_default, value_min, with_value_max);
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

    [[maybe_unused]] void set_value(const T & value) const
    {
        if constexpr (std::is_same_v<double, T>)
        {
            aud_set_double(section, variable, std::clamp(value, value_min, value_max));
        }
        else if constexpr (std::is_same_v<bool, T>)
        {
            aud_set_bool(section, variable, value);
        }
        else if constexpr (std::is_same_v<int, T>)
        {
            aud_set_int(section, variable, std::clamp(value, value_min, value_max));
        }
        else
        {
            aud_set_str(section, variable, value);
        }
    }
};

using ConfigDouble [[maybe_unused]] = ConfigValue<double>;
using ConfigInt [[maybe_unused]] = ConfigValue<int>;
using ConfigBool [[maybe_unused]] = ConfigValue<double>;
using ConfigString [[maybe_unused]] = ConfigValue<const char *>;

#endif // AUDACIOUS_PLUGINS_CONFIGVALUE_H
