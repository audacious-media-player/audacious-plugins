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

#ifndef AUDACIOUS_MPT_MPTWRAP_H
#define AUDACIOUS_MPT_MPTWRAP_H

#include <libaudcore/i18n.h>
#include <libaudcore/preferences.h>
#include <libaudcore/vfs.h>

#include <libopenmpt/libopenmpt.h>
#include <libopenmpt/libopenmpt_stream_callbacks_file.h>

class MPTWrap
{
public:
    static constexpr int interp_none = 1;
    static constexpr int interp_linear = 2;
    static constexpr int interp_cubic = 4;
    static constexpr int interp_windowed = 8;

    static constexpr int default_interpolator = interp_windowed;
    static constexpr int default_stereo_separation = 70;

    static constexpr ComboItem interpolators[] =
    {
        {N_("None"),          interp_none},
        {N_("Linear"),        interp_linear},
        {N_("Cubic"),         interp_cubic},
        {N_("Windowed sinc"), interp_windowed}
    };

    static bool is_valid_interpolator(int);
    void set_interpolator(int);

    static bool is_valid_stereo_separation(int);
    void set_stereo_separation(int);

    bool open(VFSFile &);
    int64_t read(float *, int64_t);
    void seek(int pos);

    static constexpr int rate() { return 48000; }
    static constexpr int channels() { return 2; }

    int duration() const { return m_duration; }
    const String & title() const { return m_title; }
    const String & format() const { return m_format; }

private:
    static size_t stream_read(void *, void *, size_t);
    static int stream_seek(void *, int64_t, int);
    static int64_t stream_tell(void *);
    static VFSFile *VFS(void *instance) { return reinterpret_cast<VFSFile *>(instance); }

    static constexpr openmpt_stream_callbacks callbacks = { stream_read, stream_seek, stream_tell };

    SmartPtr<openmpt_module, openmpt_module_destroy> mod;

    int m_duration = 0;
    String m_title;
    String m_format;
};

#endif
