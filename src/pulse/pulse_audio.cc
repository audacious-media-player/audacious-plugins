/***
  This file is part of xmms-pulse.

  Updates for Audacious are:
  Copyright 2016 John Lindgren

  xmms-pulse is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  xmms-pulse is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with xmms-pulse; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
  USA.
***/

#include <condition_variable>
#include <mutex>

#include <pulse/pulseaudio.h>

#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/i18n.h>

using scoped_lock = std::unique_lock<std::mutex>;

class PulseOutput : public OutputPlugin
{
public:
    static const char about[];

    static constexpr PluginInfo info = {
        N_("PulseAudio Output"),
        PACKAGE,
        about
    };

    constexpr PulseOutput () : OutputPlugin (info, 8) {}

    bool init ();

    StereoVolume get_volume ();
    void set_volume (StereoVolume v);

    bool open_audio (int fmt, int rate, int nch, String & error);
    void close_audio ();

    void period_wait ();
    int write_audio (const void * ptr, int length);
    void drain ();

    int get_delay ();

    void pause (bool pause);
    void flush ();
};

EXPORT PulseOutput aud_plugin_instance;

static std::mutex pulse_mutex;
static std::condition_variable pulse_cond;

static pa_context * context = nullptr;
static pa_stream * stream = nullptr;
static pa_mainloop * mainloop = nullptr;

static bool connected, flushed, polling;

static pa_cvolume volume;

/* Check whether the connection is still alive. */
static bool alive ()
{
    return pa_context_get_state (context) == PA_CONTEXT_READY &&
     pa_stream_get_state (stream) == PA_STREAM_READY;
}

/* Cooperative polling method.  Only one thread calls the actual poll function,
 * and dispatches the events received.  Any other threads simply wait for the
 * first thread to finish. */
static void poll_events (scoped_lock & lock)
{
    if (polling)
        pulse_cond.wait (lock);
    else
    {
        pa_mainloop_prepare (mainloop, -1);

        polling = true;
        lock.unlock ();

        pa_mainloop_poll (mainloop);

        lock.lock ();
        polling = false;

        pa_mainloop_dispatch (mainloop);

        pulse_cond.notify_all ();
    }
}

/* Wait for an asynchronous operation to complete.  Return immediately if the
 * connection dies. */
static bool finish (pa_operation * op, scoped_lock & lock)
{
    pa_operation_state_t state;
    while ((state = pa_operation_get_state (op)) != PA_OPERATION_DONE && alive ())
        poll_events (lock);

    pa_operation_unref (op);
    return (state == PA_OPERATION_DONE);
}

#define REPORT(function) do { \
    AUDERR ("%s() failed: %s\n", function, pa_strerror (pa_context_errno (context))); \
} while (0)

#define CHECK(function, ...) do { \
    auto op = function (__VA_ARGS__, & success); \
    if (! op || ! finish (op, lock) || ! success) \
        REPORT (#function); \
} while (0)

static void info_cb (pa_context *, const pa_sink_input_info * i, int, void * userdata)
{
    if (! i)
        return;

    volume = i->volume;

    if (userdata)
        * (int *) userdata = 1;
}

static void subscribe_cb (pa_context * c, pa_subscription_event_type t, uint32_t index, void *)
{
    pa_operation * o;

    if (! stream || index != pa_stream_get_index (stream) ||
        (t != (PA_SUBSCRIPTION_EVENT_SINK_INPUT | PA_SUBSCRIPTION_EVENT_CHANGE) &&
         t != (PA_SUBSCRIPTION_EVENT_SINK_INPUT | PA_SUBSCRIPTION_EVENT_NEW)))
        return;

    if (! (o = pa_context_get_sink_input_info (c, index, info_cb, nullptr)))
    {
        REPORT ("pa_context_get_sink_input_info");
        return;
    }

    pa_operation_unref (o);
}

static void stream_success_cb (pa_stream *, int success, void * userdata)
{
    if (userdata)
        * (int * ) userdata = success;
}

static void context_success_cb (pa_context *, int success, void * userdata)
{
    if (userdata)
        * (int * ) userdata = success;
}

StereoVolume PulseOutput::get_volume ()
{
    scoped_lock lock (pulse_mutex);

    if (! connected)
        return {0, 0};

    if (! polling)
    {
        /* read any pending events to get the lastest volume */
        while (pa_mainloop_iterate (mainloop, 0, nullptr) > 0)
            continue;
    }

    StereoVolume v;
    if (volume.channels == 2)
    {
        v.left = aud::rescale<int> (volume.values[0], PA_VOLUME_NORM, 100);
        v.right = aud::rescale<int> (volume.values[1], PA_VOLUME_NORM, 100);
    }
    else
        v.left = v.right = aud::rescale<int> (pa_cvolume_avg (& volume), PA_VOLUME_NORM, 100);

    return v;
}

void PulseOutput::set_volume (StereoVolume v)
{
    scoped_lock lock (pulse_mutex);

    if (! connected)
        return;

    if (volume.channels != 1)
    {
        volume.values[0] = aud::rescale<int> (v.left, 100, PA_VOLUME_NORM);
        volume.values[1] = aud::rescale<int> (v.right, 100, PA_VOLUME_NORM);
        volume.channels = 2;
    }
    else
    {
        volume.values[0] = aud::rescale<int> (aud::max (v.left, v.right), 100, PA_VOLUME_NORM);
        volume.channels = 1;
    }

    int success = 0;
    CHECK (pa_context_set_sink_input_volume, context,
     pa_stream_get_index (stream), & volume, context_success_cb);
}

void PulseOutput::pause (bool pause)
{
    scoped_lock lock (pulse_mutex);

    int success = 0;
    CHECK (pa_stream_cork, stream, pause, stream_success_cb);
}

int PulseOutput::get_delay ()
{
    scoped_lock lock (pulse_mutex);

    pa_usec_t usec;
    int neg;

    if (pa_stream_get_latency (stream, & usec, & neg) == PA_OK)
        return usec / 1000;
    else
        return 0;
}

void PulseOutput::drain ()
{
    scoped_lock lock (pulse_mutex);

    int success = 0;
    CHECK (pa_stream_drain, stream, stream_success_cb);
}

void PulseOutput::flush ()
{
    scoped_lock lock (pulse_mutex);

    int success = 0;
    CHECK (pa_stream_flush, stream, stream_success_cb);

    /* wake up period_wait() */
    flushed = true;
    if (polling)
        pa_mainloop_wakeup (mainloop);
}

void PulseOutput::period_wait ()
{
    scoped_lock lock (pulse_mutex);

    int success = 0;
    CHECK (pa_stream_trigger, stream, stream_success_cb);

    /* if the connection dies, wait until flush() is called */
    while ((! pa_stream_writable_size (stream) || ! alive ()) && ! flushed)
        poll_events (lock);
}

int PulseOutput::write_audio (const void * ptr, int length)
{
    scoped_lock lock (pulse_mutex);
    int ret = 0;

    length = aud::min ((size_t) length, pa_stream_writable_size (stream));

    if (pa_stream_write (stream, ptr, length, nullptr, 0, PA_SEEK_RELATIVE) < 0)
        REPORT ("pa_stream_write");
    else
        ret = length;

    flushed = false;
    return ret;
}

static void close_audio_locked (scoped_lock & lock)
{
    /* wait for any parallel tasks (e.g. set_volume()) to complete */
    while (polling)
        pulse_cond.wait (lock);

    connected = false;

    if (stream)
    {
        pa_stream_disconnect (stream);
        pa_stream_unref (stream);
        stream = nullptr;
    }

    if (context)
    {
        pa_context_disconnect (context);
        pa_context_unref (context);
        context = nullptr;
    }

    if (mainloop)
    {
        pa_mainloop_free (mainloop);
        mainloop = nullptr;
    }
}

void PulseOutput::close_audio ()
{
    scoped_lock lock (pulse_mutex);
    close_audio_locked (lock);
}

static pa_sample_format_t to_pulse_format (int aformat)
{
    switch (aformat)
    {
        case FMT_U8:      return PA_SAMPLE_U8;
        case FMT_S16_LE:  return PA_SAMPLE_S16LE;
        case FMT_S16_BE:  return PA_SAMPLE_S16BE;
#ifdef PA_SAMPLE_S24_32LE
        case FMT_S24_LE:  return PA_SAMPLE_S24_32LE;
        case FMT_S24_BE:  return PA_SAMPLE_S24_32BE;
#endif
#ifdef PA_SAMPLE_S32LE
        case FMT_S32_LE:  return PA_SAMPLE_S32LE;
        case FMT_S32_BE:  return PA_SAMPLE_S32BE;
#endif
        case FMT_FLOAT:   return PA_SAMPLE_FLOAT32NE;
        default:          return PA_SAMPLE_INVALID;
    }
}

static bool set_sample_spec (pa_sample_spec & ss, int fmt, int rate, int nch)
{
    ss.format = to_pulse_format (fmt);
    if (ss.format == PA_SAMPLE_INVALID)
        return false;

    ss.rate = rate;
    ss.channels = nch;

    return pa_sample_spec_valid (& ss);
}

static void set_buffer_attr (pa_buffer_attr & buffer, const pa_sample_spec & ss)
{
    int buffer_ms = aud_get_int (nullptr, "output_buffer_size");
    size_t buffer_size = pa_usec_to_bytes ((pa_usec_t) 1000 * buffer_ms, & ss);

    buffer.maxlength = (uint32_t) -1;
    buffer.tlength = buffer_size;
    buffer.prebuf = (uint32_t) -1;
    buffer.minreq = (uint32_t) -1;
    buffer.fragsize = buffer_size;
}

static bool create_context (scoped_lock & lock)
{
    if (! (mainloop = pa_mainloop_new ()))
    {
        AUDERR ("Failed to allocate main loop\n");
        return false;
    }

    if (! (context = pa_context_new (pa_mainloop_get_api (mainloop), "Audacious")))
    {
        AUDERR ("Failed to allocate context\n");
        return false;
    }

    if (pa_context_connect (context, nullptr, (pa_context_flags_t) 0, nullptr) < 0)
    {
        REPORT ("pa_context_connect");
        return false;
    }

    /* Wait until the context is ready */
    pa_context_state_t cstate;
    while ((cstate = pa_context_get_state (context)) != PA_CONTEXT_READY)
    {
        if (cstate == PA_CONTEXT_TERMINATED || cstate == PA_CONTEXT_FAILED)
        {
            REPORT ("pa_context_connect");
            return false;
        }

        poll_events (lock);
    }

    return true;
}

static bool create_stream (scoped_lock & lock, const pa_sample_spec & ss)
{
    if (! (stream = pa_stream_new (context, "Audacious", & ss, nullptr)))
    {
        REPORT ("pa_stream_new");
        return false;
    }

    /* Connect stream with sink and default volume */
    pa_buffer_attr buffer;
    set_buffer_attr (buffer, ss);

    auto flags = pa_stream_flags_t (PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE);
    if (pa_stream_connect_playback (stream, nullptr, & buffer, flags, nullptr, nullptr) < 0)
    {
        REPORT ("pa_stream_connect_playback");
        return false;
    }

    /* Wait until the stream is ready */
    pa_stream_state_t sstate;
    while ((sstate = pa_stream_get_state (stream)) != PA_STREAM_READY)
    {
        if (sstate == PA_STREAM_FAILED || sstate == PA_STREAM_TERMINATED)
        {
            REPORT ("pa_stream_connect_playback");
            return false;
        }

        poll_events (lock);
    }

    return true;
}

static bool subscribe_events (scoped_lock & lock)
{
    pa_context_set_subscribe_callback (context, subscribe_cb, nullptr);

    /* Now subscribe to events */
    int success = 0;
    CHECK (pa_context_subscribe, context, PA_SUBSCRIPTION_MASK_SINK_INPUT, context_success_cb);
    if (! success)
        return false;

    /* Now request the initial stream info */
    success = 0;
    CHECK (pa_context_get_sink_input_info, context, pa_stream_get_index (stream), info_cb);
    if (! success)
        return false;

    return true;
}

bool PulseOutput::open_audio (int fmt, int rate, int nch, String & error)
{
    scoped_lock lock (pulse_mutex);

    pa_sample_spec ss;
    if (! set_sample_spec (ss, fmt, rate, nch))
        return false;

    if (! create_context (lock) ||
        ! create_stream (lock, ss) ||
        ! subscribe_events (lock))
    {
        close_audio_locked (lock);
        return false;
    }

    connected = true;
    flushed = true;
    return true;
}

bool PulseOutput::init ()
{
    String error;
    if (! open_audio (FMT_S16_NE, 44100, 2, error))
        return false;

    close_audio ();
    return true;
}

const char PulseOutput::about[] =
 N_("Audacious PulseAudio Output Plugin\n\n"
    "This program is free software; you can redistribute it and/or modify "
    "it under the terms of the GNU General Public License as published by "
    "the Free Software Foundation; either version 2 of the License, or "
    "(at your option) any later version.\n\n"
    "This program is distributed in the hope that it will be useful, "
    "but WITHOUT ANY WARRANTY; without even the implied warranty of "
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
    "GNU General Public License for more details.\n\n"
    "You should have received a copy of the GNU General Public License "
    "along with this program; if not, write to the Free Software "
    "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, "
    "USA.");
