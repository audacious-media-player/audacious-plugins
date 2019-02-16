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

#include <cctype>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

#include "mptwrap.h"

static Index<ComboItem> items;

static bool force_apply = false;

static const char *SETTING_STEREO_SEPARATION = "stereo_separation";
static const char *SETTING_INTERPOLATOR      = "interpolator";

class MPTPlugin : public InputPlugin
{
public:
    static const char about[];
    static const char *const exts[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info =
    {
        N_("OpenMPT (Module Player)"),
        PACKAGE,
        about,
        &prefs,
    };

    static constexpr auto iinfo = InputInfo(0)
        /* For continuity, be at a lower priority than modplug for the time being */
        .with_priority(1)
        .with_exts(exts);

    constexpr MPTPlugin() : InputPlugin(info, iinfo) { }

    bool init()
    {
        std::ostringstream default_stereo_separation;
        std::ostringstream default_interpolator;

        default_stereo_separation << MPTWrap::default_stereo_separation();
        default_interpolator << MPTWrap::default_interpolator();

        const char *const defaults[] =
        {
            SETTING_STEREO_SEPARATION, default_stereo_separation.str().c_str(),
            SETTING_INTERPOLATOR, default_interpolator.str().c_str(),
            nullptr,
        };

        aud_config_set_defaults(PACKAGE, defaults);

        return true;
    }

    bool is_our_file(const char *filename, VFSFile &file)
    {
        try
        {
            MPTWrap mpt(file);
            return true;
        }
        catch(const MPTWrap::InvalidFile &)
        {
            return false;
        }
    }

    bool read_tag(const char *filename, VFSFile &file, Tuple &tuple, Index<char> *)
    {
        try
        {
            MPTWrap mpt(file);
            tuple.set_filename(filename);
            tuple.set_format(mpt.format(), mpt.channels(), mpt.rate(), 0);

            tuple.set_int(Tuple::Length, mpt.duration());
            tuple.set_str(Tuple::Title, mpt.title());

            return true;
        }
        catch(const MPTWrap::InvalidFile &)
        {
            return false;
        }
    }

    bool play(const char *filename, VFSFile &file)
    {
        force_apply = true;

        try
        {
            MPTWrap mpt(file);

            open_audio(FMT_FLOAT, mpt.rate(), mpt.channels());

            while (!check_stop())
            {
                unsigned char buffer[65536];
                std::int64_t n;
                int seek_value = check_seek();

                if (seek_value >= 0)
                    mpt.seek(seek_value);

                if (force_apply)
                {
                    mpt.set_interpolator(aud_get_int(PACKAGE, SETTING_INTERPOLATOR));
                    mpt.set_stereo_separation(aud_get_int(PACKAGE, SETTING_STEREO_SEPARATION));
                    force_apply = false;
                }

                n = mpt.read(buffer, sizeof buffer);
                if (n == 0)
                    break;

                write_audio(buffer, n);
            }

            return true;
        }
        catch(const MPTWrap::InvalidFile &)
        {
            return false;
        }
    }
};

const char MPTPlugin::about[] = N_("Module player based on libopenmpt\n\nWritten by: Chris Spiegel <cspiegel@gmail.com>");

const char *const MPTPlugin::exts[] =
{
    "669", "amf", "ams", "dbm", "digi", "dmf", "dsm", "far", "gdm", "ice", "imf",
    "it", "j2b", "m15", "mdl", "med", "mmcmp", "mms", "mo3", "mod", "mptm", "mt2",
    "mtm", "nst", "okt", "plm", "ppm", "psm", "pt36", "ptm", "s3m", "sfx", "sfx2",
    "st26", "stk", "stm", "ult", "umx", "wow", "xm", "xpk",
    nullptr
};

static ArrayRef<ComboItem> interpolator_fill()
{
    items.clear();

    for(MPTWrap::Interpolator &interpolator : MPTWrap::get_interpolators())
        items.append(interpolator.name, interpolator.value);

    return { items.begin(), items.len() };
}

static void values_changed()
{
    force_apply = true;
}

const PreferencesWidget MPTPlugin::widgets[] =
{
    WidgetSpin(
            N_("Stereo separation:"),
            WidgetInt(PACKAGE, SETTING_STEREO_SEPARATION, values_changed),
            { 0.0, 100.0, 1.0, N_("%") }
    ),

    WidgetCombo(
            N_("Interpolation:"),
            WidgetInt(PACKAGE, SETTING_INTERPOLATOR, values_changed),
            { nullptr, interpolator_fill }
    )
};

const PluginPreferences MPTPlugin::prefs = {{ widgets }};

EXPORT MPTPlugin aud_plugin_instance;
