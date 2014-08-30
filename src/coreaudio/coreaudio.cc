/*
 * CoreAudio Output Plugin for Audacious
 * Copyright 2014 William Pitcock
 *
 * Based on:
 * SDL Output Plugin for Audacious
 * Copyright 2010 John Lindgren
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
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

#include "coreaudio.h"

namespace coreaudio {

#define VOLUME_RANGE 40 /* decibels */

#define error(...) do { \
    aud_ui_show_error (str_printf ("CoreAudio error: " __VA_ARGS__)); \
} while (0)

static const char * const sdl_defaults[] = {
 "vol_left", "100",
 "vol_right", "100",
 nullptr};

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static volatile int vol_left, vol_right;

static int chan, rate;

static unsigned char * buffer;
static int buffer_size, buffer_data_start, buffer_data_len, buffer_bytes_per_channel;

static int64_t frames_written;
static char prebuffer_flag, paused_flag;

static int block_delay;
static struct timeval block_time;

static AudioComponent output_comp;
static AudioComponentInstance output_instance;

static OSStatus callback (void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData);

struct AudioUnitFormatDescriptionMap {
    int aud_format;
    unsigned int mBitsPerChannel;
    unsigned int mFormatFlags;
};
static struct AudioUnitFormatDescriptionMap AUFormatMap[] = {
    {FMT_S16_LE, 16, kAudioFormatFlagIsSignedInteger},
    {FMT_S16_BE, 16, kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian},
    {FMT_U16_LE, 16, 0},
    {FMT_U16_BE, 16, kAudioFormatFlagIsBigEndian},
    {FMT_S24_LE, 24, kAudioFormatFlagIsSignedInteger},
    {FMT_S24_BE, 24, kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian},
    {FMT_U24_LE, 24, 0},
    {FMT_U24_BE, 24, kAudioFormatFlagIsBigEndian},
    {FMT_S32_LE, 32, kAudioFormatFlagIsSignedInteger},
    {FMT_S32_BE, 32, kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian},
    {FMT_U32_LE, 32, 0},
    {FMT_U32_BE, 32, kAudioFormatFlagIsBigEndian},
    {FMT_FLOAT, 32, kAudioFormatFlagIsFloat},
};

bool init (void)
{
    aud_config_set_defaults ("coreaudio", sdl_defaults);

    vol_left = aud_get_int ("coreaudio", "vol_left");
    vol_right = aud_get_int ("coreaudio", "vol_right");

    /* open the default audio device */
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    desc.componentManufacturer = 0;

    output_comp = AudioComponentFindNext (NULL, & desc);
    if (! output_comp)
    {
        error ("Failed to open default audio device.\n");
        return 0;
    }

    if (AudioComponentInstanceNew (output_comp, & output_instance))
    {
        error ("Failed to open default audio device.\n");
        return 0;
    }

    AURenderCallbackStruct input;
    input.inputProc = callback;
    input.inputProcRefCon = NULL;

    if (AudioUnitSetProperty (output_instance, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, & input, sizeof input))
    {
        error ("Unable to attach an IOProc to the selected audio unit.\n");
        return 0;
    }

    return 1;
}

void cleanup (void)
{
}

void get_volume (int * left, int * right)
{
    * left = vol_left;
    * right = vol_right;
}

void set_volume (int left, int right)
{
    vol_left = left;
    vol_right = right;

    aud_set_int ("coreaudio", "vol_left", left);
    aud_set_int ("coreaudio", "vol_right", right);
}

#ifdef XXX_NOTYET
static void apply_mono_volume (unsigned char * data, int len)
{
    int vol = aud::max (vol_left, vol_right);
    int factor = (vol == 0) ? 0 : powf (10, (float) VOLUME_RANGE * (vol - 100)
     / 100 / 20) * 65536;

    int16_t * i = (int16_t *) data;
    int16_t * end = (int16_t *) (data + len);

    while (i < end)
    {
        * i = ((int) * i * factor) >> 16;
        i ++;
    }
}

static void apply_stereo_volume (unsigned char * data, int len)
{
    int factor_left = (vol_left == 0) ? 0 : powf (10, (float) VOLUME_RANGE *
     (vol_left - 100) / 100 / 20) * 65536;
    int factor_right = (vol_right == 0) ? 0 : powf (10, (float) VOLUME_RANGE *
     (vol_right - 100) / 100 / 20) * 65536;

    int16_t * i = (int16_t *) data;
    int16_t * end = (int16_t *) (data + len);

    while (i < end)
    {
        * i = ((int) * i * factor_left) >> 16;
        i ++;
        * i = ((int) * i * factor_right) >> 16;
        i ++;
    }
}
#endif

static OSStatus callback (void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
{
    pthread_mutex_lock (& mutex);

    int len = ioData->mBuffers[0].mDataByteSize;
    unsigned char * buf = (unsigned char *) ioData->mBuffers[0].mData;
    ioData->mBuffers[0].mNumberChannels = chan;

    int copy = aud::min (len, buffer_data_len);
    int part = buffer_size - buffer_data_start;

    if (copy <= part)
    {
        memcpy (buf, buffer + buffer_data_start, copy);
        buffer_data_start += copy;
    }
    else
    {
        memcpy (buf, buffer + buffer_data_start, part);
        memcpy (buf + part, buffer, copy - part);
        buffer_data_start = copy - part;
    }

    buffer_data_len -= copy;

#ifdef XXX_NOTYET
    if (chan == 2)
        apply_stereo_volume (buf, copy);
    else
        apply_mono_volume (buf, copy);
#endif

    if (copy < len)
        memset (buf + copy, 0, len - copy);

    /* At this moment, we know that there is a delay of (at least) the block of
     * data just written.  We save the block size and the current time for
     * estimating the delay later on. */
    block_delay = copy / (buffer_bytes_per_channel * chan) * 1000 / rate;
    gettimeofday (& block_time, nullptr);

    pthread_cond_broadcast (& cond);
    pthread_mutex_unlock (& mutex);

    return 0;
}

bool open_audio (int format, int rate_, int chan_)
{
    struct AudioUnitFormatDescriptionMap * m = nullptr;

    for (struct AudioUnitFormatDescriptionMap it : AUFormatMap)
    {
        if (it.aud_format == format)
        {
            m = & it;
            break;
        }
    }

    if (! m)
    {
        error ("The requested audio format %d is unsupported.\n", format);
        return 0;
    }

    AUDDBG ("Opening audio for %d channels, %d Hz.\n", chan, rate);

    chan = chan_;
    rate = rate_;

    buffer_bytes_per_channel = (m->mBitsPerChannel / 8);
    buffer_size = buffer_bytes_per_channel * chan * (aud_get_int (nullptr, "output_buffer_size") * rate / 1000);
    buffer = new unsigned char[buffer_size];
    buffer_data_start = 0;
    buffer_data_len = 0;

    frames_written = 0;
    prebuffer_flag = 1;
    paused_flag = 0;

    if (AudioUnitInitialize (output_instance))
    {
        error ("Unable to initialize audio unit instance\n");
        return 0;
    }

    AudioStreamBasicDescription streamFormat;
    streamFormat.mSampleRate = rate;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = m->mFormatFlags;
    streamFormat.mFramesPerPacket = 1;
    streamFormat.mChannelsPerFrame = chan;
    streamFormat.mBitsPerChannel = m->mBitsPerChannel;
    streamFormat.mBytesPerPacket = chan * buffer_bytes_per_channel;
    streamFormat.mBytesPerFrame = chan * buffer_bytes_per_channel;

    AUDDBG("Stream format:\n");
    AUDDBG(" Channels: %d\n", streamFormat.mChannelsPerFrame);
    AUDDBG(" Sample rate: %f\n", streamFormat.mSampleRate);
    AUDDBG(" Bits per channel: %d\n", streamFormat.mBitsPerChannel);
    AUDDBG(" Bytes per frame: %d\n", streamFormat.mBytesPerFrame);

    if (AudioUnitSetProperty (output_instance, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &streamFormat, sizeof(streamFormat)))
    {
        error ("Failed to set audio unit input property.\n");
        return 0;
    }

    if (AudioOutputUnitStart (output_instance))
    {
        error ("Unable to start audio unit.\n");
        return 0;
    }

    return 1;
}

void close_audio (void)
{
    AUDDBG ("Closing audio.\n");

    AudioOutputUnitStop (output_instance);

    delete[] buffer;
    buffer = nullptr;
}

int buffer_free (void)
{
    pthread_mutex_lock (& mutex);
    int space = buffer_size - buffer_data_len;
    pthread_mutex_unlock (& mutex);
    return space;
}

static void check_started (void)
{
    if (! prebuffer_flag)
        return;

    AUDDBG ("Starting playback.\n");
    prebuffer_flag = 0;
    block_delay = 0;
}

void period_wait (void)
{
    pthread_mutex_lock (& mutex);

    while (buffer_data_len == buffer_size)
    {
        if (! paused_flag)
            check_started ();

        pthread_cond_wait (& cond, & mutex);
    }

    pthread_mutex_unlock (& mutex);
}

void write_audio (void * data, int len)
{
    pthread_mutex_lock (& mutex);

    assert (len <= buffer_size - buffer_data_len);

    int start = (buffer_data_start + buffer_data_len) % buffer_size;

    if (len <= buffer_size - start)
        memcpy (buffer + start, data, len);
    else
    {
        int part = buffer_size - start;
        memcpy (buffer + start, data, part);
        memcpy (buffer, (char *) data + part, len - part);
    }

    buffer_data_len += len;
    frames_written += len / (buffer_bytes_per_channel * chan);

    pthread_mutex_unlock (& mutex);
}

void drain (void)
{
    AUDDBG ("Draining.\n");
    pthread_mutex_lock (& mutex);

    check_started ();

    while (buffer_data_len)
        pthread_cond_wait (& cond, & mutex);

    pthread_mutex_unlock (& mutex);
}

int output_time (void)
{
    pthread_mutex_lock (& mutex);

    int out = (int64_t) (frames_written - buffer_data_len / (buffer_bytes_per_channel * chan))
     * 1000 / rate;

    /* Estimate the additional delay of the last block written. */
    if (! prebuffer_flag && ! paused_flag && block_delay)
    {
        struct timeval cur;
        gettimeofday (& cur, nullptr);

        int elapsed = 1000 * (cur.tv_sec - block_time.tv_sec) + (cur.tv_usec -
         block_time.tv_usec) / 1000;

        if (elapsed < block_delay)
            out -= block_delay - elapsed;
    }

    pthread_mutex_unlock (& mutex);
    return out;
}

void pause (bool pause)
{
    AUDDBG ("%sause.\n", pause ? "P" : "Unp");
    pthread_mutex_lock (& mutex);

    paused_flag = pause;

    if (pause)
        AudioOutputUnitStop (output_instance);
    else
    {
        if (AudioOutputUnitStart (output_instance))
        {
            error ("Unable to restart audio unit after pausing.\n");
            close_audio ();
        }
    }

    pthread_cond_broadcast (& cond); /* wake up period wait */
    pthread_mutex_unlock (& mutex);
}

void flush (int time)
{
    AUDDBG ("Seek requested; discarding buffer.\n");
    pthread_mutex_lock (& mutex);

    buffer_data_start = 0;
    buffer_data_len = 0;

    frames_written = (int64_t) time * rate / 1000;
    prebuffer_flag = 1;

    pthread_cond_broadcast (& cond); /* wake up period wait */
    pthread_mutex_unlock (& mutex);
}

}
