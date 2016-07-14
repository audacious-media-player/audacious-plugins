/*
 * Sndio Output Plugin for Audacious
 * Copyright 2014 John Lindgren
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

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <sndio.h>

class SndioPlugin : public OutputPlugin
{
public:
    static const char * const defaults[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;

    static constexpr PluginInfo info = {
        N_("Sndio Output"),
        PACKAGE,
        nullptr,
        & prefs
    };

    constexpr SndioPlugin () : OutputPlugin (info, 5) {}

    bool init ();

    StereoVolume get_volume ();
    void set_volume (StereoVolume v);

    bool open_audio (int format, int rate, int channels, String & error);
    void close_audio ();

    void period_wait ();
    int write_audio (const void * data, int size);
    void drain ();

    int get_delay ();

    void flush ();

    // not implemented
    void pause (bool pause) {}

private:
    bool poll_locked ();

    static void volume_cb (void *, unsigned int vol);
    static void move_cb (void * arg, int delta);

    sio_hdl * m_handle = nullptr;

    int m_rate = 0, m_channels = 0;
    int m_bytes_per_frame = 0;

    int m_frames_buffered = 0;
    timeval m_last_write_time = timeval ();
    int m_flush_count = 0;

    pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t m_cond = PTHREAD_COND_INITIALIZER;
};

EXPORT SndioPlugin aud_plugin_instance;

const char * const SndioPlugin::defaults[] = {
    "save_volume", "FALSE",
    "volume", "100",
    nullptr
};

const PreferencesWidget SndioPlugin::widgets[] = {
    WidgetEntry (N_("Device (blank for default):"),
        WidgetString ("sndio", "device")),
    WidgetCheck (N_("Save and restore volume:"),
        WidgetBool ("sndio", "save_volume"))
};

const PluginPreferences SndioPlugin::prefs = {{widgets}};

bool SndioPlugin::init ()
{
    aud_config_set_defaults ("sndio", defaults);
    return true;
}

void SndioPlugin::volume_cb (void *, unsigned int vol)
{
    aud_set_int ("sndio", "volume", aud::rescale ((int) vol, SIO_MAXVOL, 100));
}

void SndioPlugin::set_volume (StereoVolume v)
{
    int vol = aud::max (v.left, v.right);
    aud_set_int ("sndio", "volume", vol);

    pthread_mutex_lock (& m_mutex);

    if (m_handle)
        sio_setvol (m_handle, aud::rescale (vol, 100, SIO_MAXVOL));

    pthread_mutex_unlock (& m_mutex);
}

StereoVolume SndioPlugin::get_volume ()
{
    int vol = aud_get_int ("sndio", "volume");
    return {vol, vol};
}

void SndioPlugin::move_cb (void * arg, int delta)
{
    auto me = (SndioPlugin *) arg;

    me->m_frames_buffered -= delta;
    gettimeofday (& me->m_last_write_time, nullptr);

    pthread_cond_broadcast (& me->m_cond);
}

struct FormatData {
    int format;
    int bits, bytes;
    bool sign, le;
};

static const FormatData format_table[] = {
    {FMT_S8, 8, 1, true, false},
    {FMT_U8, 8, 1, false, false},
    {FMT_S16_LE, 16, 2, true, true},
    {FMT_S16_BE, 16, 2, true, false},
    {FMT_U16_LE, 16, 2, false, true},
    {FMT_U16_BE, 16, 2, false, false},
    {FMT_S24_LE, 24, 4, true, true},
    {FMT_S24_BE, 24, 4, true, false},
    {FMT_U24_LE, 24, 4, false, true},
    {FMT_U24_BE, 24, 4, false, false},
    {FMT_S32_LE, 32, 4, true, true},
    {FMT_S32_BE, 32, 4, true, false},
    {FMT_U32_LE, 32, 4, false, true},
    {FMT_U32_BE, 32, 4, false, false},
};

bool SndioPlugin::open_audio (int format, int rate, int channels, String & error)
{
    const FormatData * fdata = nullptr;

    for (const FormatData & f : format_table)
    {
        if (f.format == format)
            fdata = & f;
    }

    if (! fdata)
    {
        error = String (str_printf (_("Sndio error: Unsupported audio format (%d)"), format));
        return false;
    }

    String device = aud_get_str ("sndio", "device");
    const char * device2 = device[0] ? (const char *) device : SIO_DEVANY;

    m_handle = sio_open (device2, SIO_PLAY, true);

    if (! m_handle)
    {
        error = String (_("Sndio error: sio_open() failed"));
        return false;
    }

    m_rate = rate;
    m_channels = channels;
    m_bytes_per_frame = FMT_SIZEOF (format) * channels;

    m_frames_buffered = 0;
    m_last_write_time = timeval ();
    m_flush_count = 0;

    int buffer_ms = aud_get_int (nullptr, "output_buffer_size");

    sio_par par;
    sio_initpar (& par);

    par.bits = fdata->bits;
    par.bps = fdata->bytes;
    par.sig = fdata->sign;
    par.le = fdata->le;
    par.msb = false;
    par.pchan = channels;
    par.rate = rate;
    par.bufsz = aud::rescale (buffer_ms, 1000, rate);
    par.xrun = SIO_IGNORE;

    if (! sio_setpar (m_handle, & par))
    {
        error = String (_("Sndio error: sio_setpar() failed"));
        goto fail;
    }

    if (aud_get_bool ("sndio", "save_volume"))
        set_volume (get_volume ());

    sio_onvol (m_handle, volume_cb, nullptr);
    sio_onmove (m_handle, move_cb, this);

    if (! sio_start (m_handle))
    {
        error = String (_("Sndio error: sio_start() failed"));
        goto fail;
    }

    return true;

fail:
    sio_close (m_handle);
    m_handle = nullptr;
    return false;
}

void SndioPlugin::close_audio ()
{
    sio_close (m_handle);
    m_handle = nullptr;
}

bool SndioPlugin::poll_locked ()
{
    bool success = false;
    pollfd * fds = nullptr;
    int nfds, old_flush_count;

    nfds = sio_nfds (m_handle);
    if (nfds < 1)
        goto fail;

    fds = new pollfd[nfds];

    nfds = sio_pollfd (m_handle, fds, POLLOUT);
    if (nfds < 1)
        goto fail;

    old_flush_count = m_flush_count;

    pthread_mutex_unlock (& m_mutex);

    if (poll (fds, nfds, -1) < 0)
        AUDERR ("poll() failed: %s\n", strerror (errno));
    else
        success = true;

    pthread_mutex_lock (& m_mutex);

    if (success && m_flush_count == old_flush_count)
        sio_revents (m_handle, fds);

fail:
    delete[] fds;
    return success;
}

void SndioPlugin::period_wait ()
{
    pthread_mutex_lock (& m_mutex);

    if (sio_eof (m_handle) || ! poll_locked ())
        pthread_cond_wait (& m_cond, & m_mutex);

    pthread_mutex_unlock (& m_mutex);
}

int SndioPlugin::write_audio (const void * data, int size)
{
    pthread_mutex_lock (& m_mutex);

    int len = sio_write (m_handle, data, size);
    m_frames_buffered += len / m_bytes_per_frame;

    pthread_mutex_unlock (& m_mutex);
    return len;
}

void SndioPlugin::drain ()
{
    pthread_mutex_lock (& m_mutex);

    int d = aud::rescale (m_frames_buffered, m_rate, 1000);
    timespec delay = {d / 1000, d % 1000 * 1000000};

    pthread_mutex_unlock (& m_mutex);
    nanosleep (& delay, nullptr);
    pthread_mutex_lock (& m_mutex);

    poll_locked ();

    pthread_mutex_unlock (& m_mutex);
}

int SndioPlugin::get_delay ()
{
    auto timediff = [] (const timeval & a, const timeval & b) -> int64_t
        { return 1000 * (int64_t) (b.tv_sec - a.tv_sec) + (b.tv_usec - a.tv_usec) / 1000; };

    pthread_mutex_lock (& m_mutex);

    int delay = aud::rescale (m_frames_buffered, m_rate, 1000);

    if (m_last_write_time.tv_sec)
    {
        timeval now;
        gettimeofday (& now, nullptr);
        delay = aud::max (delay - timediff (m_last_write_time, now), (int64_t) 0);
    }

    pthread_mutex_unlock (& m_mutex);
    return delay;
}

void SndioPlugin::flush ()
{
    pthread_mutex_lock (& m_mutex);

    sio_stop (m_handle);

    m_frames_buffered = 0;
    m_last_write_time = timeval ();
    m_flush_count ++;

    if (! sio_start (m_handle))
        AUDERR ("sio_start() failed\n");

    pthread_cond_broadcast (& m_cond);
    pthread_mutex_unlock (& m_mutex);
}
