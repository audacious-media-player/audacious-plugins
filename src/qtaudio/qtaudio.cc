/*
 * QtMultimedia Audio Output Plugin for Audacious
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
#include <libaudcore/mainloop.h>

#include "qtaudio.h"

namespace qtaudio {

#define VOLUME_RANGE 40 /* decibels */

#define error(...) do { \
    aud_ui_show_error (str_printf ("QtAudio error: " __VA_ARGS__)); \
} while (0)

static const char * const sdl_defaults[] = {
 "vol_left", "100",
 "vol_right", "100",
 nullptr};

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static volatile int vol_left, vol_right;

static int chan, rate;

static int buffer_size, buffer_bytes_per_channel;

static int64_t frames_written;
static char prebuffer_flag, paused_flag;

static int block_delay;
static struct timeval block_time;

static QAudioOutput * output_instance = nullptr;
static QIODevice * buffer_instance = nullptr;

struct FormatDescriptionMap {
    int aud_format;

    unsigned int sample_size;
    enum QAudioFormat::SampleType sample_type;
    enum QAudioFormat::Endian endian;    
};
static struct FormatDescriptionMap FormatMap[] = {
    {FMT_S16_LE, 16, QAudioFormat::SignedInt, QAudioFormat::LittleEndian},
    {FMT_S16_BE, 16, QAudioFormat::SignedInt, QAudioFormat::BigEndian},
    {FMT_U16_LE, 16, QAudioFormat::UnSignedInt, QAudioFormat::LittleEndian},
    {FMT_U16_BE, 16, QAudioFormat::UnSignedInt, QAudioFormat::BigEndian},
#ifdef XXX_NOTYET
    {FMT_S24_LE, 24, QAudioFormat::SignedInt, QAudioFormat::LittleEndian},
    {FMT_S24_BE, 24, QAudioFormat::SignedInt, QAudioFormat::BigEndian},
    {FMT_U24_LE, 24, QAudioFormat::UnSignedInt, QAudioFormat::LittleEndian},
    {FMT_U24_BE, 24, QAudioFormat::UnSignedInt, QAudioFormat::BigEndian},
#endif
    {FMT_S32_LE, 32, QAudioFormat::SignedInt, QAudioFormat::LittleEndian},
    {FMT_S32_BE, 32, QAudioFormat::SignedInt, QAudioFormat::BigEndian},
    {FMT_U32_LE, 32, QAudioFormat::UnSignedInt, QAudioFormat::LittleEndian},
    {FMT_U32_BE, 32, QAudioFormat::UnSignedInt, QAudioFormat::BigEndian},
    {FMT_FLOAT, 32, QAudioFormat::Float, QAudioFormat::LittleEndian},
};

bool init (void)
{
    aud_config_set_defaults ("coreaudio", sdl_defaults);

    vol_left = aud_get_int ("coreaudio", "vol_left");
    vol_right = aud_get_int ("coreaudio", "vol_right");

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
    int vol_max;

    vol_left = left;
    vol_right = right;

    vol_max = aud::max (vol_left, vol_right);

    aud_set_int ("coreaudio", "vol_left", left);
    aud_set_int ("coreaudio", "vol_right", right);

    if (output_instance)
    {
        float factor = (vol_max == 0) ? 0.0 : powf (10, (float) VOLUME_RANGE * (vol_max - 100) / 100 / 20);

        output_instance->setVolume (factor);
    }
}

bool open_audio (int format, int rate_, int chan_)
{
    struct FormatDescriptionMap * m = nullptr;

    for (struct FormatDescriptionMap it : FormatMap)
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

    AUDDBG ("Opening audio for %d channels, %d Hz.\n", chan_, rate_);

    chan = chan_;
    rate = rate_;

    buffer_bytes_per_channel = (m->sample_size / 8) + ((m->sample_size % 16) / 8);
    buffer_size = buffer_bytes_per_channel * chan * (aud_get_int (nullptr, "output_buffer_size") * rate / 1000);

    frames_written = 0;
    prebuffer_flag = 1;
    paused_flag = 0;

    QAudioFormat fmt;
    fmt.setSampleRate (rate);
    fmt.setChannelCount (chan);
    fmt.setSampleSize (m->sample_size);
    fmt.setCodec ("audio/pcm");
    fmt.setByteOrder (m->endian);
    fmt.setSampleType (m->sample_type);

    QAudioDeviceInfo info (QAudioDeviceInfo::defaultOutputDevice ());
    if (! info.isFormatSupported (fmt))
    {
        error ("Format not supported by backend.\n");
        return 0;
    }

    output_instance = new QAudioOutput (fmt, nullptr);
    output_instance->setBufferSize (buffer_size);
    buffer_instance = output_instance->start ();

    set_volume (vol_left, vol_right);

    return 1;
}

void close_audio (void)
{
    AUDDBG ("Closing audio.\n");

    output_instance->stop ();

    delete output_instance;
    output_instance = nullptr;
}

int buffer_free (void)
{
    pthread_mutex_lock (& mutex);

    int space = output_instance->bytesFree ();

    pthread_mutex_unlock (& mutex);
    return space;
}

void write_audio (void * data, int len)
{
    pthread_mutex_lock (& mutex);

    buffer_instance->write ((char *) data, len);

    frames_written += len / (buffer_bytes_per_channel * chan);

    if (len)
        pthread_cond_broadcast (& cond);

    pthread_mutex_unlock (& mutex);
}

void drain (void)
{
    AUDDBG ("Draining.\n");
    pthread_mutex_lock (& mutex);

    while (output_instance->bytesFree () < output_instance->bufferSize ())
        pthread_cond_wait (& cond, & mutex);

    pthread_mutex_unlock (& mutex);
}

int output_time (void)
{
    pthread_mutex_lock (& mutex);

    int out = (int64_t) (frames_written - (output_instance->bufferSize () - output_instance->bytesFree ()) / (buffer_bytes_per_channel * chan))
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
        output_instance->suspend ();
    else
        output_instance->resume ();

    pthread_cond_broadcast (& cond); /* wake up period wait */
    pthread_mutex_unlock (& mutex);
}

void flush (int time)
{
    AUDDBG ("Seek requested; discarding buffer.\n");
    pthread_mutex_lock (& mutex);

    frames_written = (int64_t) time * rate / 1000;
    prebuffer_flag = 1;

    output_instance->reset ();
    buffer_instance = output_instance->start ();

    pthread_cond_broadcast (& cond); /* wake up period wait */
    pthread_mutex_unlock (& mutex);
}

}
