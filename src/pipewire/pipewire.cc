/*
 * PipeWire Output Plugin for Audacious
 * Copyright 2022 Thomas Lange
 *
 * Based on the PipeWire Output Plugin for Qmmp
 * Copyright 2021 Ilya Kotov
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

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/runtime.h>

class PipeWireOutput : public OutputPlugin
{
public:
    static const char about[];
    static const char * const defaults[];

    static constexpr PluginInfo info = {
        N_("PipeWire Output"),
        PACKAGE,
        about,
        nullptr
    };

    constexpr PipeWireOutput() : OutputPlugin(info, 8) {}

    bool init();
    void cleanup();

    StereoVolume get_volume();
    void set_volume(StereoVolume v);

    bool open_audio(int format, int rate, int channels, String & error);
    void close_audio();

    void period_wait();
    int write_audio(const void * data, int length);
    void drain();

    int get_delay();

    void pause(bool pause);
    void flush();

private:
    struct pw_stream * create_stream(int rate);
    bool connect_stream(int format, int rate, int channels);

    static void on_core_event_done(void * data, uint32_t id, int seq);
    static void on_registry_event_global(void * data, uint32_t id, uint32_t permissions,
                                         const char * type, uint32_t version,
                                         const struct spa_dict * props);
    static void on_state_changed(void * data, enum pw_stream_state old,
                                 enum pw_stream_state state, const char * error);
    static void on_process(void * data);
    static void on_drained(void * data);

    static enum spa_audio_format to_pipewire_format(int format);
    static void set_channel_map(struct spa_audio_info_raw * info, int channels);

    struct pw_thread_loop * m_loop = nullptr;
    struct pw_stream * m_stream = nullptr;
    struct pw_context * m_context = nullptr;
    struct pw_core * m_core = nullptr;
    struct pw_registry * m_registry = nullptr;

    struct spa_hook m_core_listener = {};
    struct spa_hook m_stream_listener = {};
    struct spa_hook m_registry_listener = {};

    bool m_inited = false;
    bool m_has_sinks = false;
    bool m_ignore_state_change = false;

    int m_core_init_seq = 0;

    unsigned char * m_buffer = nullptr;
    unsigned int m_buffer_at = 0;
    unsigned int m_buffer_size = 0;
    unsigned int m_frames = 0;
    unsigned int m_stride = 0;
    unsigned int m_rate = 0;
    unsigned int m_channels = 0;
};

EXPORT PipeWireOutput aud_plugin_instance;

const char PipeWireOutput::about[] =
 N_("PipeWire Output Plugin for Audacious\n"
    "Copyright 2022 Thomas Lange\n\n"
    "Based on the PipeWire Output Plugin for Qmmp\n"
    "Copyright 2021 Ilya Kotov");

const char * const PipeWireOutput::defaults[] = {
    "volume_left", "50",
    "volume_right", "50",
    nullptr
};

StereoVolume PipeWireOutput::get_volume()
{
    return {aud_get_int("pipewire", "volume_left"),
            aud_get_int("pipewire", "volume_right")};
}

void PipeWireOutput::set_volume(StereoVolume v)
{
    aud_set_int("pipewire", "volume_left", v.left);
    aud_set_int("pipewire", "volume_right", v.right);

    if (!m_loop)
        return;

    float values[m_channels];

    if (m_channels == 2)
    {
        values[0] = float(v.left) / 100.0;
        values[1] = float(v.right) / 100.0;
    }
    else
    {
        for (unsigned int i = 0; i < m_channels; i++)
            values[i] = float(aud::max(v.left, v.right)) / 100.0;
    }

    pw_thread_loop_lock(m_loop);
    pw_stream_set_control(m_stream, SPA_PROP_channelVolumes, m_channels, values, nullptr);
    pw_thread_loop_unlock(m_loop);
}

void PipeWireOutput::pause(bool pause)
{
    pw_thread_loop_lock(m_loop);
    pw_stream_set_active(m_stream, !pause);
    pw_thread_loop_unlock(m_loop);
}

int PipeWireOutput::get_delay()
{
    return (m_buffer_at / m_stride + m_frames) * 1000 / m_rate;
}

void PipeWireOutput::drain()
{
    pw_thread_loop_lock(m_loop);
    if (m_buffer_at > 0)
        pw_thread_loop_timed_wait(m_loop, 2);

    pw_stream_flush(m_stream, true);
    pw_thread_loop_timed_wait(m_loop, 2);
    pw_thread_loop_unlock(m_loop);
}

void PipeWireOutput::flush()
{
    pw_thread_loop_lock(m_loop);
    m_buffer_at = 0;
    pw_thread_loop_unlock(m_loop);
    pw_stream_flush(m_stream, false);
}

void PipeWireOutput::period_wait()
{
    // TODO: Implement
}

int PipeWireOutput::write_audio(const void * data, int length)
{
    pw_thread_loop_lock(m_loop);

    if (m_buffer_at == m_buffer_size)
    {
        if (pw_thread_loop_timed_wait(m_loop, 1) != 0)
        {
            pw_thread_loop_unlock(m_loop);
            return 0;
        }
    }

    auto size = aud::min<size_t>(m_buffer_size - m_buffer_at, length);
    memcpy(m_buffer + m_buffer_at, data, size);
    m_buffer_at += size;

    pw_thread_loop_unlock(m_loop);
    return size;
}

void PipeWireOutput::close_audio()
{
    if (m_stream)
    {
        pw_thread_loop_lock(m_loop);
        m_ignore_state_change = true;
        pw_stream_disconnect(m_stream);
        pw_stream_destroy(m_stream);
        m_ignore_state_change = false;
        m_stream = nullptr;
        pw_thread_loop_unlock(m_loop);
    }

    if (m_loop)
        pw_thread_loop_stop(m_loop);

    if (m_registry)
    {
        pw_proxy_destroy(reinterpret_cast<pw_proxy *>(m_registry));
        m_registry = nullptr;
    }

    if (m_core)
    {
        pw_core_disconnect(m_core);
        m_core = nullptr;
    }

    if (m_context)
    {
        pw_context_destroy(m_context);
        m_context = nullptr;
    }

    if (m_loop)
    {
        pw_thread_loop_destroy(m_loop);
        m_loop = nullptr;
    }

    if (m_buffer)
    {
        delete[] m_buffer;
        m_buffer = nullptr;
    }
}

bool PipeWireOutput::open_audio(int format, int rate, int channels, String & error)
{
    static const struct pw_core_events core_events = {
        .version = PW_VERSION_CORE_EVENTS,
        .done = PipeWireOutput::on_core_event_done
    };

    static const struct pw_registry_events registry_events = {
        .version = PW_VERSION_REGISTRY_EVENTS,
        .global = PipeWireOutput::on_registry_event_global
    };

    static const struct pw_stream_events stream_events = {
        .version = PW_VERSION_STREAM_EVENTS,
        .state_changed = PipeWireOutput::on_state_changed,
        .process = PipeWireOutput::on_process,
        .drained = PipeWireOutput::on_drained
    };

    if (!(m_loop = pw_thread_loop_new("pipewire-main-loop", nullptr)))
    {
        AUDERR("PipeWireOutput: unable to create main loop\n");
        return false;
    }

    if (!(m_context = pw_context_new(pw_thread_loop_get_loop(m_loop), nullptr, 0)))
    {
        AUDERR("PipeWireOutput: unable to create context\n");
        return false;
    }

    if (!(m_core = pw_context_connect(m_context, nullptr, 0)))
    {
        AUDERR("PipeWireOutput: unable to connect context\n");
        return false;
    }
    pw_core_add_listener(m_core, &m_core_listener, &core_events, this);

    if (!(m_registry = pw_core_get_registry(m_core, PW_VERSION_REGISTRY, 0)))
    {
        AUDERR("PipeWireOutput: unable to get registry interface\n");
        return false;
    }
    pw_registry_add_listener(m_registry, &m_registry_listener, &registry_events, this);

    m_core_init_seq = pw_core_sync(m_core, PW_ID_CORE, m_core_init_seq);

    if (pw_thread_loop_start(m_loop) != 0)
    {
        AUDERR("PipeWireOutput: unable to start loop\n");
        return false;
    }

    pw_thread_loop_lock(m_loop);
    while (!m_inited)
    {
        if (pw_thread_loop_timed_wait(m_loop, 2) != 0)
            break;
    }
    pw_thread_loop_unlock(m_loop);

    if (!m_inited || !m_has_sinks)
    {
        AUDERR("PipeWireOutput: unable to initialize loop\n");
        return false;
    }

    // TODO: Don't hardcode values, use aud_get_int("output_buffer_size")?
    //
    // m_stride = AudioParameters::sampleSize(format) * map.count();
    // m_frames = qBound(64, qCeil(2048.0 * rate / 48000.0), 8192);
    // m_buffer_size = m_frames * m_stride;
    // m_buffer = new unsigned char[m_buffer_size];
    m_stride = 4;
    m_frames = 128;
    m_buffer_size = 1024;
    m_buffer = new unsigned char[m_buffer_size];

    pw_thread_loop_lock(m_loop);

    if (!(m_stream = create_stream(rate)))
    {
        AUDERR("PipeWireOutput: unable to create stream\n");
        pw_thread_loop_unlock(m_loop);
        return false;
    }

    m_stream_listener = {};
    pw_stream_add_listener(m_stream, &m_stream_listener, &stream_events, this);

    if (!connect_stream(format, rate, channels))
    {
        pw_thread_loop_unlock(m_loop);
        AUDERR("PipeWireOutput: unable to connect stream");
        return false;
    }

    pw_thread_loop_unlock(m_loop);
    AUDINFO("PipeWireOutput: ready\n");

    return true;
}

bool PipeWireOutput::init()
{
    aud_config_set_defaults("pipewire", defaults);
    pw_init(nullptr, nullptr);
    return true;
}

void PipeWireOutput::cleanup()
{
    pw_deinit();
}

struct pw_stream * PipeWireOutput::create_stream(int rate)
{
    struct pw_properties * props =
        pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                          PW_KEY_MEDIA_CATEGORY, "Playback",
                          PW_KEY_MEDIA_ROLE, "Music",
                          PW_KEY_APP_ID, "audacious",
                          PW_KEY_APP_ICON_NAME, "audacious",
                          PW_KEY_APP_NAME, _("Audacious"),
                          nullptr);

    pw_properties_setf(props, PW_KEY_NODE_LATENCY, "%u/%d", m_frames, rate);

    return pw_stream_new(m_core, _("Playback"), props);
}

bool PipeWireOutput::connect_stream(int format, int rate, int channels)
{
    m_rate = rate;
    m_channels = channels;

    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof buffer);

    struct spa_audio_info_raw info = {
        .format = to_pipewire_format(format),
        .flags = SPA_AUDIO_FLAG_NONE,
        .rate = m_rate,
        .channels = m_channels
    };

    set_channel_map(&info, channels);

    const struct spa_pod * params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

    auto stream_flags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                                     PW_STREAM_FLAG_MAP_BUFFERS |
                                                     PW_STREAM_FLAG_RT_PROCESS);

    return pw_stream_connect(m_stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
                             stream_flags, params, aud::n_elems(params)) == 0;
}

void PipeWireOutput::on_core_event_done(void * data, uint32_t id, int seq)
{
    PipeWireOutput * o = static_cast<PipeWireOutput *>(data);

    if (id != PW_ID_CORE || seq != o->m_core_init_seq)
        return;

    spa_hook_remove(&o->m_registry_listener);
    spa_hook_remove(&o->m_core_listener);

    o->m_inited = true;
    pw_thread_loop_signal(o->m_loop, false);
}

void PipeWireOutput::on_registry_event_global(void * data, uint32_t id, uint32_t permissions,
                                              const char * type, uint32_t version,
                                              const struct spa_dict * props)
{
    PipeWireOutput * o = static_cast<PipeWireOutput *>(data);

    if (strcmp(type, PW_TYPE_INTERFACE_Node) != 0)
        return;

    const char * media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
    if (!media_class)
        return;

    if (strcmp(media_class, "Audio/Sink") != 0)
        return;

    o->m_has_sinks = true;
    o->m_core_init_seq = pw_core_sync(o->m_core, PW_ID_CORE, o->m_core_init_seq);
}

void PipeWireOutput::on_state_changed(void * data, enum pw_stream_state old,
                                      enum pw_stream_state state, const char * error)
{
    PipeWireOutput * o = static_cast<PipeWireOutput *>(data);

    if (o->m_ignore_state_change)
        return;

    if (state == PW_STREAM_STATE_UNCONNECTED ||
        state == PW_STREAM_STATE_PAUSED ||
        state == PW_STREAM_STATE_STREAMING)
    {
        pw_thread_loop_signal(o->m_loop, false);
    }
}

void PipeWireOutput::on_process(void * data)
{
    PipeWireOutput * o = static_cast<PipeWireOutput *>(data);

    if (!o->m_buffer_at)
    {
        pw_thread_loop_signal(o->m_loop, false);
        return;
    }

    struct pw_buffer * b = pw_stream_dequeue_buffer(o->m_stream);
    struct spa_buffer * buf = b->buffer;

    auto size = aud::min<uint32_t>(buf->datas[0].maxsize, o->m_buffer_at);
    memcpy(buf->datas[0].data, o->m_buffer, size);
    o->m_buffer_at -= size;
    memmove(o->m_buffer, o->m_buffer + size, o->m_buffer_at);

    b->buffer->datas[0].chunk->offset = 0;
    b->buffer->datas[0].chunk->size = o->m_buffer_size;
    b->buffer->datas[0].chunk->stride = o->m_stride;

    pw_stream_queue_buffer(o->m_stream, b);
    pw_thread_loop_signal(o->m_loop, false);
}

void PipeWireOutput::on_drained(void * data)
{
    PipeWireOutput * o = static_cast<PipeWireOutput *>(data);
    pw_thread_loop_signal(o->m_loop, false);
}

enum spa_audio_format PipeWireOutput::to_pipewire_format(int format)
{
    switch (format)
    {
        case FMT_FLOAT:   return SPA_AUDIO_FORMAT_F32_LE;

        case FMT_S8:      return SPA_AUDIO_FORMAT_S8;
        case FMT_U8:      return SPA_AUDIO_FORMAT_U8;

        case FMT_S16_LE:  return SPA_AUDIO_FORMAT_S16_LE;
        case FMT_S16_BE:  return SPA_AUDIO_FORMAT_S16_BE;
        case FMT_U16_LE:  return SPA_AUDIO_FORMAT_U16_LE;
        case FMT_U16_BE:  return SPA_AUDIO_FORMAT_U16_BE;

        case FMT_S24_LE:  return SPA_AUDIO_FORMAT_S24_32_LE;
        case FMT_S24_BE:  return SPA_AUDIO_FORMAT_S24_32_BE;
        case FMT_U24_LE:  return SPA_AUDIO_FORMAT_U24_32_LE;
        case FMT_U24_BE:  return SPA_AUDIO_FORMAT_U24_32_BE;

        case FMT_S24_3LE: return SPA_AUDIO_FORMAT_S24_LE;
        case FMT_S24_3BE: return SPA_AUDIO_FORMAT_S24_BE;
        case FMT_U24_3LE: return SPA_AUDIO_FORMAT_U24_LE;
        case FMT_U24_3BE: return SPA_AUDIO_FORMAT_U24_BE;

        case FMT_S32_LE:  return SPA_AUDIO_FORMAT_S32_LE;
        case FMT_S32_BE:  return SPA_AUDIO_FORMAT_S32_BE;
        case FMT_U32_LE:  return SPA_AUDIO_FORMAT_U32_LE;
        case FMT_U32_BE:  return SPA_AUDIO_FORMAT_U32_BE;

        default:          return SPA_AUDIO_FORMAT_UNKNOWN;
    }
}

void PipeWireOutput::set_channel_map(struct spa_audio_info_raw * info, int channels)
{
    switch (channels)
    {
        case 18:
            info->position[15] = SPA_AUDIO_CHANNEL_TRL;
            info->position[16] = SPA_AUDIO_CHANNEL_TRC;
            info->position[17] = SPA_AUDIO_CHANNEL_TRR;
            // Fall through
        case 15:
            info->position[12] = SPA_AUDIO_CHANNEL_TFL;
            info->position[13] = SPA_AUDIO_CHANNEL_TFC;
            info->position[14] = SPA_AUDIO_CHANNEL_TFR;
            // Fall through
        case 12:
            info->position[11] = SPA_AUDIO_CHANNEL_TC;
            // Fall through
        case 11:
            info->position[9]  = SPA_AUDIO_CHANNEL_SL;
            info->position[10] = SPA_AUDIO_CHANNEL_SR;
            // Fall through
        case 9:
            info->position[8] = SPA_AUDIO_CHANNEL_RC;
            // Fall through
        case 8:
            info->position[6] = SPA_AUDIO_CHANNEL_FLC;
            info->position[7] = SPA_AUDIO_CHANNEL_FRC;
            // Fall through
        case 6:
            info->position[4] = SPA_AUDIO_CHANNEL_RL;
            info->position[5] = SPA_AUDIO_CHANNEL_RR;
            // Fall through
        case 4:
            info->position[3] = SPA_AUDIO_CHANNEL_LFE;
            // Fall through
        case 3:
            info->position[2] = SPA_AUDIO_CHANNEL_FC;
            // Fall through
        case 2:
            info->position[0] = SPA_AUDIO_CHANNEL_FL;
            info->position[1] = SPA_AUDIO_CHANNEL_FR;
            break;
        case 1:
            info->position[0] = SPA_AUDIO_CHANNEL_MONO;
            break;
    }
}
