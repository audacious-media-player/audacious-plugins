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

#include <math.h>
#include <pthread.h>
#include <sys/time.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

#include <QAudioOutput>

#define VOLUME_RANGE 40 /* decibels */

class QtAudio : public OutputPlugin
{
public:
    static const char about[];
    static const char * const defaults[];

    static constexpr PluginInfo info = {
        N_("QtMultimedia Output"),
        PACKAGE,
        about
    };

    constexpr QtAudio () : OutputPlugin (info, 1) {}

    bool init ();

    StereoVolume get_volume ();
    void set_volume (StereoVolume v);

    bool open_audio (int aud_format, int rate, int chan, String & error);
    void close_audio ();

    void period_wait ();
    int write_audio (const void * data, int size);
    void drain ();

    int get_delay ();

    void pause (bool pause);
    void flush ();
};

EXPORT QtAudio aud_plugin_instance;

const char QtAudio::about[] =
 N_("QtMultimedia Audio Output Plugin for Audacious\n"
    "Copyright 2014 William Pitcock\n\n"
    "Based on SDL Output Plugin for Audacious\n"
    "Copyright 2010 John Lindgren");

const char * const QtAudio::defaults[] = {
 "vol_left", "100",
 "vol_right", "100",
 nullptr};

static const timespec fifty_ms = {0, 50000000};

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static int bytes_per_sec;
static bool paused;
static int last_buffered, delay_estimate;
static timeval last_system_time;

static QAudioOutput * output_instance = nullptr;
static QIODevice * buffer_instance = nullptr;

struct FormatDescriptionMap {
    int aud_format;

    unsigned int sample_size;
    enum QAudioFormat::SampleType sample_type;
    enum QAudioFormat::Endian endian;    
};

static constexpr FormatDescriptionMap FormatMap[] = {
    {FMT_S16_LE, 16, QAudioFormat::SignedInt, QAudioFormat::LittleEndian},
    {FMT_S16_BE, 16, QAudioFormat::SignedInt, QAudioFormat::BigEndian},
    {FMT_U16_LE, 16, QAudioFormat::UnSignedInt, QAudioFormat::LittleEndian},
    {FMT_U16_BE, 16, QAudioFormat::UnSignedInt, QAudioFormat::BigEndian},
    {FMT_S32_LE, 32, QAudioFormat::SignedInt, QAudioFormat::LittleEndian},
    {FMT_S32_BE, 32, QAudioFormat::SignedInt, QAudioFormat::BigEndian},
    {FMT_U32_LE, 32, QAudioFormat::UnSignedInt, QAudioFormat::LittleEndian},
    {FMT_U32_BE, 32, QAudioFormat::UnSignedInt, QAudioFormat::BigEndian},
    {FMT_FLOAT, 32, QAudioFormat::Float, QAudioFormat::LittleEndian},
};

bool QtAudio::init ()
{
    aud_config_set_defaults ("qtaudio", defaults);
    return true;
}

StereoVolume QtAudio::get_volume ()
{
    return {aud_get_int ("qtaudio", "vol_left"), aud_get_int ("qtaudio", "vol_right")};
}

void QtAudio::set_volume (StereoVolume v)
{
    int vol_max = aud::max (v.left, v.right);

    aud_set_int ("qtaudio", "vol_left", v.left);
    aud_set_int ("qtaudio", "vol_right", v.right);

    if (output_instance)
    {
        float factor = (vol_max == 0) ? 0.0 : powf (10, (float) VOLUME_RANGE * (vol_max - 100) / 100 / 20);

        output_instance->setVolume (factor);
    }
}

bool QtAudio::open_audio (int format, int rate, int chan, String & error)
{
    const FormatDescriptionMap * m = nullptr;

    for (auto & it : FormatMap)
    {
        if (it.aud_format == format)
        {
            m = & it;
            break;
        }
    }

    if (! m)
    {
        error = String (str_printf ("QtAudio error: The requested audio format "
         "%d is unsupported.", format));
        return false;
    }

    AUDDBG ("Opening audio for %d channels, %d Hz.\n", chan, rate);

    bytes_per_sec = FMT_SIZEOF (format) * chan * rate;

    int buffer_ms = aud_get_int (nullptr, "output_buffer_size");
    int buffer_size = FMT_SIZEOF (format) * chan * aud::rescale (buffer_ms, 1000, rate);

    paused = false;
    last_buffered = 0;
    delay_estimate = 0;

    gettimeofday (& last_system_time, nullptr);

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
        error = String ("QtAudio error: Format not supported by backend.");
        return false;
    }

    output_instance = new QAudioOutput (fmt, nullptr);
    output_instance->setBufferSize (buffer_size);
    buffer_instance = output_instance->start ();

    set_volume (get_volume ());

    return true;
}

void QtAudio::close_audio ()
{
    AUDDBG ("Closing audio.\n");

    output_instance->stop ();

    delete output_instance;
    output_instance = nullptr;
}

void QtAudio::period_wait ()
{
    pthread_mutex_lock (& mutex);

    while (output_instance->bytesFree () < output_instance->periodSize ())
        pthread_cond_timedwait (& cond, & mutex, & fifty_ms);

    pthread_mutex_unlock (& mutex);
}

int QtAudio::write_audio (const void * data, int len)
{
    pthread_mutex_lock (& mutex);

    len = aud::min (len, output_instance->bytesFree ());
    buffer_instance->write ((const char *) data, len);
    last_buffered += len;

    pthread_mutex_unlock (& mutex);
    return len;
}

void QtAudio::drain ()
{
    AUDDBG ("Draining.\n");
    pthread_mutex_lock (& mutex);

    while (output_instance->bytesFree () < output_instance->bufferSize ())
        pthread_cond_timedwait (& cond, & mutex, & fifty_ms);

    pthread_mutex_unlock (& mutex);
}

int QtAudio::get_delay ()
{
    auto timediff = [] (const timeval & a, const timeval & b) -> int64_t
        { return 1000 * (int64_t) (b.tv_sec - a.tv_sec) + (b.tv_usec - a.tv_usec) / 1000; };

    pthread_mutex_lock (& mutex);

    int buffered = output_instance->bufferSize () - output_instance->bytesFree ();
    int delay = aud::rescale (buffered, bytes_per_sec, 1000);

    timeval now;
    gettimeofday (& now, nullptr);

    if (buffered == last_buffered && ! paused)
        delay += aud::max (delay_estimate - timediff (last_system_time, now), (int64_t) 0);
    else
    {
        delay_estimate = aud::rescale (last_buffered - buffered, bytes_per_sec, 1000);
        delay += delay_estimate;
        last_buffered = buffered;
        last_system_time = now;
    }

    pthread_mutex_unlock (& mutex);
    return delay;
}

void QtAudio::pause (bool pause)
{
    AUDDBG ("%sause.\n", pause ? "P" : "Unp");
    pthread_mutex_lock (& mutex);

    if (pause)
        output_instance->suspend ();
    else
        output_instance->resume ();

    paused = pause;

    pthread_cond_broadcast (& cond); /* wake up period wait */
    pthread_mutex_unlock (& mutex);
}

void QtAudio::flush ()
{
    AUDDBG ("Seek requested; discarding buffer.\n");
    pthread_mutex_lock (& mutex);

    last_buffered = 0;
    delay_estimate = 0;

    gettimeofday (& last_system_time, nullptr);

    output_instance->reset ();
    buffer_instance = output_instance->start ();

    pthread_cond_broadcast (& cond); /* wake up period wait */
    pthread_mutex_unlock (& mutex);
}
