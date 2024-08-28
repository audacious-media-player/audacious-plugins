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

#include <pulse/pulseaudio.h>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>
#include <libaudcore/threads.h>
#include <libaudcore/preferences.h>

class PulseOutput : public OutputPlugin
{
public:
    static const char about[];
    static const PreferencesWidget widgets[];
    static const PluginPreferences prefs;
    static const char * const prefs_defaults[];
    static const String default_context_name;
    static const String default_stream_name;

    static constexpr PluginInfo info = {
        N_("PulseAudio Output"),
        PACKAGE,
        about,
        & prefs
    };

    /*
     * Set a higher priority to that of PipeWire. Under a PulseAudio setup,
     * the PipeWire output may be selected by default, which would promptly
     * fail to work, as there is no PipeWire server/proxy running.
     *
     * On the other hand, under a PipeWire setup, PulseAudio output would
     * still work normally, as pipewire-pulse acts as a PulseAudio server.
     *
     * Making PulseAudio the top preference allows for better compatibility
     * for different setups, especially for distributions that have not (yet)
     * switched to PipeWire as the default audio server.
     */
    constexpr PulseOutput () : OutputPlugin (info, 9) {}

    bool init ();
    void cleanup ();

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

const PreferencesWidget PulseOutput::widgets[] = {
    WidgetEntry (N_("Context name:"),
        WidgetString ("pulse", "context_name")),
    WidgetEntry (N_("Stream name:"),
        WidgetString ("pulse", "stream_name")),
};

const PluginPreferences PulseOutput::prefs = {{widgets}};

const String PulseOutput::default_context_name = String ("Audacious");
const String PulseOutput::default_stream_name = String ("Audacious");

const char * const PulseOutput::prefs_defaults[] = {
    "context_name", PulseOutput::default_context_name,
    "stream_name", PulseOutput::default_stream_name,
    nullptr
};

static aud::mutex pulse_mutex;
static aud::condvar pulse_cond;

static pa_context * context = nullptr;
static pa_stream * stream = nullptr;
static pa_mainloop * mainloop = nullptr;

static bool connected, flushed, polling;

static pa_cvolume volume;

static StereoVolume saved_volume = {0, 0};
static bool saved_volume_changed = false;

/* Check whether the connection is still alive. */
static bool alive ()
{
    return pa_context_get_state (context) == PA_CONTEXT_READY &&
     pa_stream_get_state (stream) == PA_STREAM_READY;
}

/* Cooperative polling method.  Only one thread calls the actual poll function,
 * and dispatches the events received.  Any other threads simply wait for the
 * first thread to finish. */
static void poll_events (aud::mutex::holder & lock)
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
static bool finish (pa_operation * op, aud::mutex::holder & lock)
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

static void get_volume_locked (aud::mutex::holder & lock)
{
    if (! polling)
    {
        /* read any pending events to get the lastest volume */
        while (pa_mainloop_iterate (mainloop, 0, nullptr) > 0)
            continue;
    }

    if (volume.channels == 2)
    {
        saved_volume.left = aud::rescale<int> (volume.values[0], PA_VOLUME_NORM, 100);
        saved_volume.right = aud::rescale<int> (volume.values[1], PA_VOLUME_NORM, 100);
    }
    else
    {
        saved_volume.left = aud::rescale<int> (pa_cvolume_avg (& volume), PA_VOLUME_NORM, 100);
        saved_volume.right = saved_volume.left;
    }

    saved_volume_changed = false;
}

StereoVolume PulseOutput::get_volume ()
{
    auto lock = pulse_mutex.take ();

    if (connected)
        get_volume_locked (lock);

    return saved_volume;
}

static void set_volume_locked (aud::mutex::holder & lock)
{
    if (volume.channels != 1)
    {
        volume.values[0] = aud::rescale<int> (saved_volume.left, 100, PA_VOLUME_NORM);
        volume.values[1] = aud::rescale<int> (saved_volume.right, 100, PA_VOLUME_NORM);
        volume.channels = 2;
    }
    else
    {
        int mono = aud::max (saved_volume.left, saved_volume.right);
        volume.values[0] = aud::rescale<int> (mono, 100, PA_VOLUME_NORM);
        volume.channels = 1;
    }

    int success = 0;
    CHECK (pa_context_set_sink_input_volume, context,
     pa_stream_get_index (stream), & volume, context_success_cb);

    saved_volume_changed = false;
}

void PulseOutput::set_volume (StereoVolume v)
{
    auto lock = pulse_mutex.take ();

    saved_volume = v;
    saved_volume_changed = true;

    if (connected)
        set_volume_locked (lock);
}

void PulseOutput::pause (bool pause)
{
    auto lock = pulse_mutex.take ();

    int success = 0;
    CHECK (pa_stream_cork, stream, pause, stream_success_cb);
}

int PulseOutput::get_delay ()
{
    auto lock = pulse_mutex.take ();

    pa_usec_t usec;
    int neg;

    if (pa_stream_get_latency (stream, & usec, & neg) == PA_OK)
        return usec / 1000;
    else
        return 0;
}

void PulseOutput::drain ()
{
    auto lock = pulse_mutex.take ();

    int success = 0;
    CHECK (pa_stream_drain, stream, stream_success_cb);
}

void PulseOutput::flush ()
{
    auto lock = pulse_mutex.take ();

    int success = 0;
    CHECK (pa_stream_flush, stream, stream_success_cb);

    /* wake up period_wait() */
    flushed = true;
    if (polling)
        pa_mainloop_wakeup (mainloop);
}

void PulseOutput::period_wait ()
{
    auto lock = pulse_mutex.take ();

    int success = 0;
    CHECK (pa_stream_trigger, stream, stream_success_cb);

    /* if the connection dies, wait until flush() is called */
    while ((! pa_stream_writable_size (stream) || ! alive ()) && ! flushed)
        poll_events (lock);
}

int PulseOutput::write_audio (const void * ptr, int length)
{
    auto lock = pulse_mutex.take ();
    int ret = 0;

    length = aud::min ((size_t) length, pa_stream_writable_size (stream));

    if (pa_stream_write (stream, ptr, length, nullptr, 0, PA_SEEK_RELATIVE) < 0)
        REPORT ("pa_stream_write");
    else
        ret = length;

    flushed = false;
    return ret;
}

static void close_audio_locked (aud::mutex::holder & lock)
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
    auto lock = pulse_mutex.take ();
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
    int buffer_ms = aud_get_int ("output_buffer_size");
    size_t buffer_size = pa_usec_to_bytes ((pa_usec_t) 1000 * buffer_ms, & ss);

    buffer.maxlength = (uint32_t) -1;
    buffer.tlength = buffer_size;
    buffer.prebuf = (uint32_t) -1;
    buffer.minreq = (uint32_t) -1;
    buffer.fragsize = buffer_size;
}

static String get_context_name ()
{
    String context_name = aud_get_str ("pulse", "context_name");
    if (context_name == String (""))
    {
        return PulseOutput::default_context_name;
    }
    return context_name;
}

static bool create_context (aud::mutex::holder & lock)
{
    if (! (mainloop = pa_mainloop_new ()))
    {
        AUDERR ("Failed to allocate main loop\n");
        return false;
    }

    if (! (context = pa_context_new (pa_mainloop_get_api (mainloop), get_context_name ())))
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

static String get_stream_name ()
{
    String stream_name = aud_get_str ("pulse", "stream_name");
    if (stream_name == String (""))
    {
        return PulseOutput::default_stream_name;
    }
    return stream_name;
}

static bool create_stream (aud::mutex::holder & lock, const pa_sample_spec & ss)
{
    if (! (stream = pa_stream_new (context, get_stream_name (), & ss, nullptr)))
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

static bool subscribe_events (aud::mutex::holder & lock)
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
    auto lock = pulse_mutex.take ();

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

    if (saved_volume_changed)
        set_volume_locked (lock);
    else
        get_volume_locked (lock);

    return true;
}

bool PulseOutput::init ()
{
    aud_config_set_defaults ("pulse", prefs_defaults);

    /* check for a running server and get initial volume */
    String error;
    if (! open_audio (FMT_S16_NE, 44100, 2, error))
        return false;

    close_audio ();
    return true;
}

void PulseOutput::cleanup ()
{
    if (saved_volume_changed)
    {
        /* save final volume */
        String error;
        if (open_audio (FMT_S16_NE, 44100, 2, error))
            close_audio ();
    }
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
