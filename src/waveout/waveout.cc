/*
 * Win32 waveOut Plugin for Audacious
 * Copyright 2016 John Lindgren
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

#include <assert.h>
#include <pthread.h>

#include <windows.h>

// missing from MinGW header
#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT 3
#endif

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

#define NUM_BLOCKS 4

class WaveOut : public OutputPlugin
{
public:
    static const char about[];

    static constexpr PluginInfo info = {
        N_("Win32 waveOut"),
        PACKAGE,
        about
    };

    constexpr WaveOut () : OutputPlugin (info, 5) {}

    StereoVolume get_volume ();
    void set_volume (StereoVolume v);

    bool open_audio (int aud_format, int rate, int chans, String & error);
    void close_audio ();

    void period_wait ();
    int write_audio (const void * data, int size);
    void drain ();

    int get_delay ();

    void pause (bool pause);
    void flush ();
};

EXPORT WaveOut aud_plugin_instance;

const char WaveOut::about[] =
 N_("Win32 waveOut Plugin for Audacious\n"
    "Copyright 2016 John Lindgren");

// lock order is state, then buffer
static pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t buffer_cond = PTHREAD_COND_INITIALIZER;

static int waveout_format;
static int waveout_chan, waveout_rate;
static int block_size;

// guarded by state_mutex
static HWAVEOUT device;
static bool prebuffer_flag, paused_flag;
static int64_t total_frames;

// guarded by buffer_mutex
static Index<WAVEHDR> headers;
static Index<Index<char>> blocks;
static int next_block, blocks_free;

StereoVolume WaveOut::get_volume ()
{
    DWORD vol = 0;
    waveOutGetVolume ((HWAVEOUT) WAVE_MAPPER, & vol);
    return {aud::rescale (int (vol & 0xffff), 0xffff, 100),
            aud::rescale (int (vol >> 16), 0xffff, 100)};
}

void WaveOut::set_volume (StereoVolume v)
{
    int left = aud::rescale (v.left, 100, 0xffff);
    int right = aud::rescale (v.right, 100, 0xffff);
    waveOutSetVolume ((HWAVEOUT) WAVE_MAPPER, left | (right << 16));
}

static void CALLBACK callback (HWAVEOUT, UINT uMsg, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
    if (uMsg != WOM_DONE)
        return;

    pthread_mutex_lock (& buffer_mutex);

    assert (blocks_free < NUM_BLOCKS);
    int old_block = (next_block + blocks_free) % NUM_BLOCKS;
    blocks[old_block].resize (0);
    blocks_free ++;

    pthread_cond_broadcast (& buffer_cond);
    pthread_mutex_unlock (& buffer_mutex);
}

bool WaveOut::open_audio (int format, int rate, int chan, String & error)
{
    if (format != FMT_S16_NE && format != FMT_S32_NE && format != FMT_FLOAT)
    {
        error = String ("waveOut error: Unsupported audio format.  Supported "
                        "bit depths are 16, 32, and floating point.");
        return false;
    }

    AUDDBG ("Opening audio for format %d, %d channels, %d Hz.\n", format, chan, rate);

    waveout_format = format;
    waveout_chan = chan;
    waveout_rate = rate;

    WAVEFORMATEX desc;
    desc.wFormatTag = (format == FMT_FLOAT) ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
    desc.nChannels = chan;
    desc.nSamplesPerSec = rate;
    desc.nAvgBytesPerSec = FMT_SIZEOF (format) * chan * rate;
    desc.nBlockAlign = FMT_SIZEOF (format) * chan;
    desc.wBitsPerSample = 8 * FMT_SIZEOF (format);
    desc.cbSize = 0;

    MMRESULT res = waveOutOpen (& device, WAVE_MAPPER, & desc,
     (DWORD_PTR) callback, 0, CALLBACK_FUNCTION);

    if (res != MMSYSERR_NOERROR)
    {
        error = String (str_printf ("Error opening device (%d)!", (int) res));
        return false;
    }

    int block_ms = aud_get_int (nullptr, "output_buffer_size") / NUM_BLOCKS;
    block_size = FMT_SIZEOF (format) * chan * aud::rescale (block_ms, 1000, rate);

    headers.insert (0, NUM_BLOCKS);
    blocks.insert (0, NUM_BLOCKS);

    next_block = 0;
    blocks_free = NUM_BLOCKS;

    waveOutPause (device);
    prebuffer_flag = true;
    paused_flag = false;
    total_frames = 0;

    return true;
}

void WaveOut::close_audio ()
{
    AUDDBG ("Closing audio.\n");
    flush ();
    waveOutClose (device);

    headers.clear ();
    blocks.clear ();
}

void WaveOut::period_wait ()
{
    pthread_mutex_lock (& buffer_mutex);

    while (! blocks_free)
        pthread_cond_wait (& buffer_cond, & buffer_mutex);

    pthread_mutex_unlock (& buffer_mutex);
}

// assumes state_mutex locked
static void write_block (int b)
{
    WAVEHDR & header = headers[b];
    Index<char> & block = blocks[b];

    header.lpData = block.begin ();
    header.dwBufferLength = block.len ();

    waveOutPrepareHeader (device, & header, sizeof header);
    waveOutWrite (device, & header, sizeof header);
}

// assumes state_mutex locked
static void check_started ()
{
    if (prebuffer_flag)
    {
        AUDDBG ("Prebuffering complete.\n");
        if (! paused_flag)
            waveOutRestart (device);

        prebuffer_flag = false;
    }
}

int WaveOut::write_audio (const void * data, int len)
{
    int written = 0;
    pthread_mutex_lock (& state_mutex);
    pthread_mutex_lock (& buffer_mutex);

    while (len > 0 && blocks_free > 0)
    {
        int block_to_write = -1;

        Index<char> & block = blocks[next_block];
        int copy = aud::min (len, block_size - block.len ());
        block.insert ((const char *) data, -1, copy);

        if (block.len () == block_size)
        {
            block_to_write = next_block;
            next_block = (next_block + 1) % NUM_BLOCKS;
            blocks_free --;
        }

        written += copy;
        data = (const char *) data + copy;
        len -= copy;

        if (block_to_write >= 0)
        {
            pthread_mutex_unlock (& buffer_mutex);
            write_block (block_to_write);
            pthread_mutex_lock (& buffer_mutex);
        }
    }

    bool filled = ! blocks_free;
    pthread_mutex_unlock (& buffer_mutex);

    if (filled)
        check_started ();

    total_frames += written / (FMT_SIZEOF (waveout_format) * waveout_chan);

    pthread_mutex_unlock (& state_mutex);
    return written;
}

void WaveOut::drain ()
{
    AUDDBG ("Draining.\n");
    pthread_mutex_lock (& state_mutex);
    pthread_mutex_lock (& buffer_mutex);

    int block_to_write = -1;

    // write last partial block, if any
    if (blocks_free > 0 && blocks[next_block].len () > 0)
    {
        block_to_write = next_block;
        next_block = (next_block + 1) % NUM_BLOCKS;
        blocks_free --;
    }

    pthread_mutex_unlock (& buffer_mutex);

    if (block_to_write >= 0)
        write_block (block_to_write);

    check_started ();

    pthread_mutex_unlock (& state_mutex);
    pthread_mutex_lock (& buffer_mutex);

    while (blocks_free < NUM_BLOCKS)
        pthread_cond_wait (& buffer_cond, & buffer_mutex);

    pthread_mutex_unlock (& buffer_mutex);
}

int WaveOut::get_delay ()
{
    int delay = 0;
    pthread_mutex_lock (& state_mutex);

    MMTIME t;
    t.wType = TIME_SAMPLES;

    if (waveOutGetPosition (device, & t, sizeof t) == MMSYSERR_NOERROR && t.wType == TIME_SAMPLES)
    {
        // frame_diff is intentionally truncated to a signed 32-bit word to
        // account for overflow in the DWORD frame counter returned by Windows
        int32_t frame_diff = total_frames - t.u.sample;
        delay = aud::rescale ((int) frame_diff, waveout_rate, 1000);
    }

    pthread_mutex_unlock (& state_mutex);
    return delay;
}

void WaveOut::pause (bool pause)
{
    AUDDBG (pause ? "Pausing.\n" : "Unpausing.\n");
    pthread_mutex_lock (& state_mutex);

    if (! prebuffer_flag)
    {
        if (pause)
            waveOutPause (device);
        else
            waveOutRestart (device);
    }

    paused_flag = pause;

    pthread_mutex_unlock (& state_mutex);
}

void WaveOut::flush ()
{
    AUDDBG ("Flushing buffers.\n");
    pthread_mutex_lock (& state_mutex);

    waveOutReset (device);

    pthread_mutex_lock (& buffer_mutex);

    for (auto & header : headers)
        waveOutUnprepareHeader (device, & header, sizeof header);
    for (auto & block : blocks)
        block.resize (0);

    next_block = 0;
    blocks_free = NUM_BLOCKS;

    pthread_cond_broadcast (& buffer_cond);
    pthread_mutex_unlock (& buffer_mutex);

    waveOutPause (device);
    prebuffer_flag = true;
    total_frames = 0;

    pthread_mutex_unlock (& state_mutex);
}
