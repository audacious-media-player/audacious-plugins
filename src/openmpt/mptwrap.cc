/*-
 * Copyright (c) 2015-2019 Chris Spiegel
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <algorithm>
#include <iterator>

#define WANT_VFS_STDIO_COMPAT
#include <libopenmpt/libopenmpt_version.h>
#include "mptwrap.h"

constexpr ComboItem MPTWrap::interpolators[];
constexpr openmpt_stream_callbacks MPTWrap::callbacks;

static String to_aud_str(const char * str)
{
    String aud_str(str);
    openmpt_free_string(str);
    return aud_str;
}

bool MPTWrap::open(VFSFile &file)
{
#if OPENMPT_API_VERSION_MAJOR <= 0 && OPENMPT_API_VERSION_MINOR < 3
    auto m = openmpt_module_create(callbacks, &file, openmpt_log_func_silent,
     nullptr, nullptr);
#else
    auto m = openmpt_module_create2(callbacks, &file, openmpt_log_func_silent,
     nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
#endif

    if (m == nullptr)
        return false;

    mod.capture(m);

    openmpt_module_select_subsong(mod.get(), -1);

    m_duration = openmpt_module_get_duration_seconds(mod.get()) * 1000;
    m_title = to_aud_str(openmpt_module_get_metadata(mod.get(), "title"));
    m_format = to_aud_str(openmpt_module_get_metadata(mod.get(), "type_long"));

    return true;
}

size_t MPTWrap::stream_read(void *instance, void *buf, size_t n)
{
    return VFS(instance)->fread(buf, 1, n);
}

int MPTWrap::stream_seek(void *instance, int64_t offset, int whence)
{
    VFSSeekType w = to_vfs_seek_type(whence);
    return VFS(instance)->fseek(offset, w) >= 0 ? 0 : -1;
}

int64_t MPTWrap::stream_tell(void *instance)
{
    return VFS(instance)->ftell();
}

bool MPTWrap::is_valid_interpolator(int interpolator_value)
{
    return std::any_of(std::begin(interpolators), std::end(interpolators),
     [interpolator_value](const ComboItem &ci) { return ci.num == interpolator_value; });
}

void MPTWrap::set_interpolator(int interpolator_value)
{
    if (is_valid_interpolator(interpolator_value))
        openmpt_module_set_render_param(mod.get(),
         OPENMPT_MODULE_RENDER_INTERPOLATIONFILTER_LENGTH, interpolator_value);
}

bool MPTWrap::is_valid_stereo_separation(int separation)
{
    return separation >= 0 && separation <= 100;
}

void MPTWrap::set_stereo_separation(int separation)
{
    if (is_valid_stereo_separation(separation))
        openmpt_module_set_render_param(mod.get(),
         OPENMPT_MODULE_RENDER_STEREOSEPARATION_PERCENT, separation);
}

int64_t MPTWrap::read(float *buf, int64_t bufcnt)
{
    auto n = openmpt_module_read_interleaved_float_stereo(mod.get(), rate(),
     bufcnt / channels(), buf);

    return n * channels();
}

void MPTWrap::seek(int pos)
{
    openmpt_module_set_position_seconds(mod.get(), pos / 1000.0);
}
