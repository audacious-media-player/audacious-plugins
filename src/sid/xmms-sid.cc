/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   Main source file

   Programmed and designed by Matti 'ccr' Hamalainen <ccr@tnsp.org>
   (C) Copyright 1999-2009 Tecnic Software productions (TNSP)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

#include "xs_config.h"
#include "xs_sidplay2.h"

class SIDPlugin : public InputPlugin
{
public:
    static const char *const exts[];

    static constexpr PluginInfo info = {
        N_("SID Player"),
        PACKAGE,
        nullptr,
        & sid_prefs
    };

    constexpr SIDPlugin() : InputPlugin(info, InputInfo(FlagSubtunes)
        .with_priority(5) /* medium priority (slow to initialize) */
        .with_exts(exts)) {}

    bool init()
    {
        xs_init_configuration();
        return true;
    }

    void cleanup();

    bool is_our_file(const char *filename, VFSFile &file);
    bool read_tag(const char *filename, VFSFile &file, Tuple &tuple, Index<char> *image);
    bool play(const char *filename, VFSFile &file);

private:
    bool delayed_init();

    bool m_initialized = false;
    bool m_init_failed = false;
};

EXPORT SIDPlugin aud_plugin_instance;

static void xs_get_song_tuple_info(Tuple &pResult, const xs_tuneinfo_t &info, int subTune);

static pthread_mutex_t s_init_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Initialization functions
 */
bool SIDPlugin::delayed_init()
{
    pthread_mutex_lock(&s_init_mutex);

    if (!m_initialized && !m_init_failed)
    {
        /* Try to initialize emulator engine */
        m_initialized = xs_sidplayfp_init();
        if (!m_initialized)
            m_init_failed = true;
    }

    pthread_mutex_unlock(&s_init_mutex);
    return m_initialized;
}


/*
 * Shut down XMMS-SID
 */
void SIDPlugin::cleanup()
{
    if (m_initialized)
    {
        xs_sidplayfp_close();
        m_initialized = false;
    }

    m_init_failed = false;
}


/*
 * Check whether this is a SID file
 */
bool SIDPlugin::is_our_file(const char *filename, VFSFile &file)
{
    char buf[4];
    if (file.fread (buf, 1, 4) != 4)
        return false;

    // does not require xs_sidplayfp_init()
    return xs_sidplayfp_probe(buf, 4);
}


/*
 * Start playing the given file
 */
bool SIDPlugin::play(const char *filename, VFSFile &file)
{
    if (!delayed_init())
        return false;

    /* Load file */
    Index<char> buf = file.read_all ();
    if (!xs_sidplayfp_probe(buf.begin(), buf.len()))
        return false;

    /* Get tune information */
    xs_tuneinfo_t info;
    if (!xs_sidplayfp_getinfo(info, buf.begin(), buf.len()))
        return false;

    /* Initialize the tune */
    if (!xs_sidplayfp_load(buf.begin(), buf.len()))
        return false;

    /* Set general status information */
    int subTune = -1;
    uri_parse(filename, nullptr, nullptr, nullptr, &subTune);

    if (subTune < 1 || subTune > info.nsubTunes)
        subTune = info.startTune;

    /* Check minimum playtime */
    int tmpLength = info.subTunes[subTune - 1].tuneLength;
    if (xs_cfg.playMinTimeEnable && (tmpLength >= 0)) {
        if (tmpLength < xs_cfg.playMinTime)
            tmpLength = xs_cfg.playMinTime;
    }

    /* Initialize song */
    if (!xs_sidplayfp_initsong(subTune)) {
        AUDERR("Couldn't initialize SID-tune '%s' (sub-tune #%i)!\n",
            filename, subTune);
        return false;
    }

    /* Open the audio output */
    open_audio(FMT_S16_NE, xs_cfg.audioFrequency, xs_cfg.audioChannels);

    /* Allocate audio buffer */
    int audioBufSize = xs_cfg.audioFrequency * xs_cfg.audioChannels * 2;
    if (audioBufSize < 512)
        audioBufSize = 512;

    char *audioBuffer = new char[audioBufSize];
    int64_t bytes_played = 0;

    while (! check_stop ())
    {
        if (check_seek () >= 0)
            AUDWARN ("Seeking is not implemented, ignoring.\n");

        int bufRemaining = xs_sidplayfp_fillbuffer(audioBuffer, audioBufSize);

        write_audio (audioBuffer, bufRemaining);
        bytes_played += bufRemaining;

        /* Check if we have played enough */
        int time_played = aud::rescale<int64_t> (bytes_played,
         xs_cfg.audioFrequency * xs_cfg.audioChannels * 2, 1000);

        if (xs_cfg.playMaxTimeEnable) {
            if (xs_cfg.playMaxTimeUnknown) {
                if (tmpLength < 0 &&
                    time_played >= xs_cfg.playMaxTime * 1000)
                    break;
            } else {
                if (time_played >= xs_cfg.playMaxTime * 1000)
                    break;
            }
        }

        if (tmpLength >= 0) {
            if (time_played >= tmpLength * 1000)
                break;
        }
    }

    delete[] audioBuffer;

    return true;
}


/*
 * Return song information Tuple
 */
static void xs_get_song_tuple_info(Tuple &tuple, const xs_tuneinfo_t &info, int subTune)
{
    tuple.set_str (Tuple::Title, info.sidName);
    tuple.set_str (Tuple::Artist, info.sidComposer);
    tuple.set_str (Tuple::Copyright, info.sidCopyright);
    tuple.set_str (Tuple::Codec, info.sidFormat);

    /* Get sub-tune information, if available */
    if (subTune < 0 || info.startTune > info.nsubTunes)
        subTune = info.startTune;

    if (subTune > 0 && subTune <= info.nsubTunes) {
        int tmpInt = info.subTunes[subTune - 1].tuneLength;
        tuple.set_int (Tuple::Length, (tmpInt < 0) ? -1 : tmpInt * 1000);
    } else
        subTune = 1;

    tuple.set_int (Tuple::NumSubtunes, info.nsubTunes);
    tuple.set_int (Tuple::Subtune, subTune);
    tuple.set_int (Tuple::Track, subTune);
}


static void xs_fill_subtunes(Tuple &tuple, const xs_tuneinfo_t &info)
{
    Index<short> subtunes;

    for (int count = 0; count < info.nsubTunes; count++) {
        if (count + 1 == info.startTune || !xs_cfg.subAutoMinOnly ||
            info.subTunes[count].tuneLength < 0 ||
            info.subTunes[count].tuneLength >= xs_cfg.subAutoMinTime)
            subtunes.append (count + 1);
    }

    tuple.set_subtunes (subtunes.len (), subtunes.begin ());
}

bool SIDPlugin::read_tag(const char *filename, VFSFile &file, Tuple &tuple, Index<char> *image)
{
    if (!delayed_init())
        return false;

    Index<char> buf = file.read_all ();
    if (!xs_sidplayfp_probe(buf.begin(), buf.len()))
        return false;

    /* Get information from URL */
    int tune = tuple.get_int (Tuple::Subtune);

    /* Get tune information from emulation engine */
    xs_tuneinfo_t info;
    if (!xs_sidplayfp_getinfo(info, buf.begin(), buf.len()))
        return false;

    xs_get_song_tuple_info(tuple, info, tune);

    if (xs_cfg.subAutoEnable && info.nsubTunes > 1 && tune < 0)
        xs_fill_subtunes(tuple, info);

    return true;
}

/*
 * Plugin header
 */
const char *const SIDPlugin::exts[] = { "sid", "psid", nullptr };
