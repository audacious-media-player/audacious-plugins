/*
 * ALSA Gapless Output Plugin for Audacious
 * Copyright 2009 John Lindgren
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

/*
 * To do:
 *
 * Make devices configurable.
 */

#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>
#include <audacious/plugin.h>

#define SMALL_BUFFER 100 /* milliseconds */
#define LARGE_BUFFER 1000 /* milliseconds */

#define ERROR(...) fprintf (stderr, "alsa-gapless: " __VA_ARGS__)

#if 1
#define DEBUG(...) ERROR (__VA_ARGS__)
#else
#define DEBUG(...)
#endif

#define CHECK(function, ...) \
do { \
    gint error = function (__VA_ARGS__); \
    if (error < 0) \
    { \
        ERROR (#function " failed: %s.\n", snd_strerror (error)); \
        goto FAILED; \
    } \
} while (0)

static GMutex * alsa_mutex;
static snd_pcm_t * alsa_handle;
static GMutex * pump_mutex;
static GCond * pump_cond;

static snd_pcm_format_t alsa_format;
static gint alsa_channels, alsa_rate;
static gboolean alsa_paused;

static void * alsa_buffer;
static gint alsa_buffer_length, alsa_buffer_data_start, alsa_buffer_data_length;

static GThread * pump_thread;
static gboolean pump_quit;

static gint64 alsa_time; /* microseconds */
static gboolean alsa_leave_open;
static gint alsa_close_source;

static snd_mixer_t * alsa_mixer;
static snd_mixer_elem_t * alsa_mixer_elem;

static void send_audio (void * data, gint length)
{
    gint result = snd_pcm_writei (alsa_handle, data, snd_pcm_bytes_to_frames
     (alsa_handle, length));

    if (result < 0)
        CHECK (snd_pcm_recover, alsa_handle, result, 0);

FAILED:
    return;
}

/* pump_mutex must be locked */
static void * pump (void * unused)
{
    while (1)
    {
        snd_pcm_status_t * status;
        GTimeVal wake;
        gint writable;

        g_get_current_time (& wake);
        g_time_val_add (& wake, 1000 * SMALL_BUFFER / 2);
        g_cond_timed_wait (pump_cond, pump_mutex, & wake);

        if (pump_quit)
            break;

        g_mutex_lock (alsa_mutex);

        /* Bleh. Software pause for broken ALSA drivers. */
        if (alsa_paused)
            goto FAILED;

        snd_pcm_status_alloca (& status);
        CHECK (snd_pcm_status, alsa_handle, status);
        writable = snd_pcm_frames_to_bytes (alsa_handle,
         snd_pcm_status_get_avail (status));
        writable = MIN (writable, alsa_buffer_data_length);

        if (writable > alsa_buffer_length - alsa_buffer_data_start)
        {
            gint part = alsa_buffer_length - alsa_buffer_data_start;

            send_audio (alsa_buffer + alsa_buffer_data_start, part);
            send_audio (alsa_buffer, writable - part);

            alsa_buffer_data_length -= writable;
            alsa_buffer_data_start = writable - part;
        }
        else
        {
            send_audio (alsa_buffer + alsa_buffer_data_start, writable);

            alsa_buffer_data_length -= writable;
            alsa_buffer_data_start += writable;
        }

    FAILED:
        g_mutex_unlock (alsa_mutex);
        g_cond_signal (pump_cond);
    }

    return NULL;
}

/* pump_mutex must be locked */
static void check_pump_started (void)
{
    if (pump_thread == NULL)
    {
        DEBUG ("Starting playback.\n");
        pump_thread = g_thread_create (pump, NULL, TRUE, NULL);
        g_mutex_lock (pump_mutex); /* pump steals our first lock */
    }
}

/* alsa_mutex must be locked */
static gboolean real_open (snd_pcm_format_t format, gint rate, gint channels)
{
    snd_pcm_hw_params_t * params;
    guint time = 1000 * SMALL_BUFFER;

    DEBUG ("Opening audio: %s, %d channels, %d Hz.\n", snd_pcm_format_name
     (format), channels, rate);
    CHECK (snd_pcm_open, & alsa_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);

    snd_pcm_hw_params_alloca (& params);
    CHECK (snd_pcm_hw_params_any, alsa_handle, params);
    CHECK (snd_pcm_hw_params_set_access, alsa_handle, params,
     SND_PCM_ACCESS_RW_INTERLEAVED);
    CHECK (snd_pcm_hw_params_set_format, alsa_handle, params, format);
    CHECK (snd_pcm_hw_params_set_channels, alsa_handle, params, channels);
    CHECK (snd_pcm_hw_params_set_rate, alsa_handle, params, rate, 0);
    CHECK (snd_pcm_hw_params_set_buffer_time_min, alsa_handle, params, & time, 0);
    CHECK (snd_pcm_hw_params, alsa_handle, params);
    CHECK (snd_pcm_prepare, alsa_handle);

    alsa_format = format;
    alsa_channels = channels;
    alsa_rate = rate;
    alsa_paused = FALSE;

    alsa_buffer_length = snd_pcm_frames_to_bytes (alsa_handle, (gint64)
     LARGE_BUFFER * rate / 1000);
    alsa_buffer = g_malloc (alsa_buffer_length);
    alsa_buffer_data_start = 0;
    alsa_buffer_data_length = 0;

    pump_thread = NULL;
    pump_quit = FALSE;

    alsa_time = 0;
    alsa_leave_open = FALSE;
    alsa_close_source = 0;

    return TRUE;

FAILED:
    if (alsa_handle != NULL)
    {
        snd_pcm_close (alsa_handle);
        alsa_handle = NULL;
    }

    return FALSE;
}

/* alsa_mutex must be locked */
/* pump_mutex must be locked BEFORE alsa_mutex to prevent lockup */
static void real_close (void)
{
    DEBUG ("Closing audio.\n");

    if (alsa_close_source)
        g_source_remove (alsa_close_source);

    if (pump_thread != NULL)
    {
        pump_quit = TRUE;
        g_cond_signal (pump_cond);
        g_mutex_unlock (pump_mutex);
        g_thread_join (pump_thread); /* pump_mutex remains locked */
    }

    g_free (alsa_buffer);

    snd_pcm_close (alsa_handle);
    alsa_handle = NULL;
}

/* alsa_mutex must be locked */
/* pump_mutex must be locked */
static gboolean real_buffer_playing (void)
{
    snd_pcm_status_t * status;

    snd_pcm_status_alloca (& status);
    CHECK (snd_pcm_status, alsa_handle, status);

    if (alsa_buffer_data_length > 0 || snd_pcm_status_get_delay (status) > 0)
        return TRUE;

FAILED:
    return FALSE;
}

static OutputPluginInitStatus alsa_init (void)
{
    snd_mixer_selem_id_t * selem_id;

    DEBUG ("Initialize.\n");
    alsa_mutex = g_mutex_new ();
    alsa_handle = NULL;
    pump_mutex = g_mutex_new ();
    pump_cond = g_cond_new ();

    CHECK (snd_mixer_open, & alsa_mixer, 0);
    CHECK (snd_mixer_attach, alsa_mixer, "default");
    CHECK (snd_mixer_selem_register, alsa_mixer, NULL, NULL);
    CHECK (snd_mixer_load, alsa_mixer);

    snd_mixer_selem_id_alloca (& selem_id);
    snd_mixer_selem_id_set_name (selem_id, "PCM");
    alsa_mixer_elem = snd_mixer_find_selem (alsa_mixer, selem_id);

    if (alsa_mixer_elem == NULL)
        ERROR ("PCM mixer element not found, volume control disabled.\n");

    CHECK (snd_mixer_selem_set_playback_volume_range, alsa_mixer_elem, 0, 100);

    return OUTPUT_PLUGIN_INIT_FOUND_DEVICES;

FAILED:
    return OUTPUT_PLUGIN_INIT_NO_DEVICES;
}

static void alsa_cleanup (void)
{
    DEBUG ("Cleanup.\n");
    g_mutex_lock (pump_mutex);
    g_mutex_lock (alsa_mutex);

    if (alsa_handle != NULL)
        real_close ();

    snd_mixer_detach (alsa_mixer, "default");
    snd_mixer_close (alsa_mixer);

    g_mutex_unlock (alsa_mutex);
    g_mutex_unlock (pump_mutex);

    g_mutex_free (alsa_mutex);
    g_mutex_free (pump_mutex);
    g_cond_free (pump_cond);
}

static snd_pcm_format_t convert_aud_format (AFormat aud_format)
{
    const struct
    {
        AFormat aud_format;
        snd_pcm_format_t format;
    }
    table[] =
    {
        {FMT_FLOAT, SND_PCM_FORMAT_FLOAT},
        {FMT_S8, SND_PCM_FORMAT_S8},
        {FMT_U8, SND_PCM_FORMAT_U8},
        {FMT_S16_NE, SND_PCM_FORMAT_S16},
        {FMT_S16_LE, SND_PCM_FORMAT_S16_LE},
        {FMT_S16_BE, SND_PCM_FORMAT_S16_BE},
        {FMT_U16_NE, SND_PCM_FORMAT_U16},
        {FMT_U16_LE, SND_PCM_FORMAT_U16_LE},
        {FMT_U16_BE, SND_PCM_FORMAT_U16_BE},
        {FMT_S24_NE, SND_PCM_FORMAT_S24},
        {FMT_S24_LE, SND_PCM_FORMAT_S24_LE},
        {FMT_S24_BE, SND_PCM_FORMAT_S24_BE},
        {FMT_U24_NE, SND_PCM_FORMAT_U24},
        {FMT_U24_LE, SND_PCM_FORMAT_U24_LE},
        {FMT_U24_BE, SND_PCM_FORMAT_U24_BE},
        {FMT_S32_NE, SND_PCM_FORMAT_S32},
        {FMT_S32_LE, SND_PCM_FORMAT_S32_LE},
        {FMT_S32_BE, SND_PCM_FORMAT_S32_BE},
        {FMT_U32_NE, SND_PCM_FORMAT_U32},
        {FMT_U32_LE, SND_PCM_FORMAT_U32_LE},
        {FMT_U32_BE, SND_PCM_FORMAT_U32_BE},
    };

    gint count;

    for (count = 0; count < G_N_ELEMENTS (table); count ++)
    {
         if (table[count].aud_format == aud_format)
             return table[count].format;
    }

    return SND_PCM_FORMAT_UNKNOWN;
}

static gint alsa_open_audio (AFormat aud_format, gint rate, gint channels)
{
    snd_pcm_format_t format = convert_aud_format (aud_format);
    gint result;

    g_mutex_lock (pump_mutex);
    g_mutex_lock (alsa_mutex);

    if (alsa_handle != NULL)
    {
        g_source_remove (alsa_close_source);
        alsa_close_source = 0;

        if (format == alsa_format && channels == alsa_channels && rate ==
         alsa_rate)
        {
            DEBUG ("Audio already open and in requested format.\n");
            alsa_time = 0;
            alsa_leave_open = FALSE;
        }
        else
        {
            DEBUG ("Audio already open but not in requested format.\n");
            check_pump_started ();

            while (real_buffer_playing ())
            {
                g_mutex_unlock (alsa_mutex);
                g_cond_wait (pump_cond, pump_mutex);
                g_mutex_lock (alsa_mutex);
            }

            real_close ();
        }
    }

    result = (alsa_handle != NULL || real_open (format, rate, channels)) ? 1 :
     -1;

    g_mutex_unlock (alsa_mutex);
    g_mutex_unlock (pump_mutex);

    return result;
}

static gboolean close_cb (void * unused)
{
    gboolean playing;

    g_mutex_lock (pump_mutex);
    g_mutex_lock (alsa_mutex);

    check_pump_started ();
    playing = real_buffer_playing ();

    if (! playing)
    {
        DEBUG ("Buffer empty, closing audio.\n");
        real_close ();
    }

    g_mutex_unlock (alsa_mutex);
    g_mutex_unlock (pump_mutex);
    return playing;
}

static void alsa_close_audio (void)
{
    g_mutex_lock (pump_mutex);
    g_mutex_lock (alsa_mutex);

    DEBUG ("Close requested.\n");

    if (alsa_leave_open && ! alsa_paused)
        alsa_close_source = g_timeout_add (300, close_cb, NULL);
    else
        real_close ();

    g_mutex_unlock (alsa_mutex);
    g_mutex_unlock (pump_mutex);
}

static void alsa_write_audio (void * data, gint length)
{
    g_mutex_lock (pump_mutex);

    while (1)
    {
        g_mutex_lock (alsa_mutex);

        gint writable = MIN (alsa_buffer_length - alsa_buffer_data_length,
         length);
        gint start = (alsa_buffer_data_start + alsa_buffer_data_length) %
         alsa_buffer_length;

        if (writable > alsa_buffer_length - start)
        {
            gint part = alsa_buffer_length - start;

            memcpy ((gint8 *) alsa_buffer + start, data, part);
            memcpy (alsa_buffer, (gint8 *) data + part, writable - part);
        }
        else
            memcpy ((gint8 *) alsa_buffer + start, data, writable);

        data = (gint8 *) data + writable;
        length -= writable;

        alsa_buffer_data_length += writable;
        alsa_time += (gint64) snd_pcm_bytes_to_frames (alsa_handle, writable) *
         1000000 / alsa_rate;

        g_mutex_unlock (alsa_mutex);

        if (! length)
            break;

        check_pump_started ();
        g_cond_wait (pump_cond, pump_mutex);
    }

    g_mutex_unlock (pump_mutex);
}

static gint alsa_written_time (void)
{
    gint time;

    g_mutex_lock (alsa_mutex);
    time = alsa_time / 1000;
    g_mutex_unlock (alsa_mutex);

    return time;
}

/*
 * Note:
 *
 * "output_time" has to return sanely (zero) even when audio is not open, since
 * it is difficult for Audacious core to know if a decoder plugin has opened
 * audio at any given time.
 */

static gint alsa_output_time (void)
{
    snd_pcm_status_t * status;
    gint time = 0;

    g_mutex_lock (alsa_mutex);

    if (alsa_handle == NULL)
        goto FAILED;

    snd_pcm_status_alloca (& status);
    CHECK (snd_pcm_status, alsa_handle, status);
    time = (alsa_time - (gint64) (snd_pcm_bytes_to_frames (alsa_handle,
     alsa_buffer_data_length) + snd_pcm_status_get_delay (status)) * 1000000 /
     alsa_rate) / 1000;
    time = MAX (0, time); /* still finishing previous song? */

FAILED:
    g_mutex_unlock (alsa_mutex);
    return time;
}

/*
 * Hack #1.
 *
 * Normally, Audacious core polls "buffer_free" as it passes audio from the
 * decoder plugin to the output plugin. However, we already poll as we pass data
 * from our buffer to the ALSA buffer, and our "write_audio" wants to poll our
 * own buffer in sync with that to reduce CPU wakeups. To keep Audacious core
 * from polling "buffer_free", we pretend that we always have free buffer space.
 */

static gint alsa_buffer_free (void)
{
    return 1048576; /* no decoder should ever pass 1 MB at once */
}

/*
 * Hack #2.
 *
 * "buffer_playing" is used by decoder plugins at the end of a song to wait
 * until the audio buffer has emptied before closing audio. We pretend that the
 * buffer has emptied when there is in fact still data in it so that the decoder
 * will exit and Audacious will start playing the next song. Since
 * "buffer_playing" will normally NOT be called during a user-initiated stop, we
 * also use it as our clue to let the buffer keep playing.
 */

static gint alsa_buffer_playing (void)
{
    g_mutex_lock (alsa_mutex);

    DEBUG ("Song ending, not closing audio.\n");
    alsa_leave_open = TRUE;

    g_mutex_unlock (alsa_mutex);
    return 0;
}

static void alsa_flush (gint time)
{
    g_mutex_lock (alsa_mutex);

    DEBUG ("Seek requested, discarding buffer.\n");
    alsa_time = (gint64) time * 1000;
    alsa_buffer_data_start = 0;
    alsa_buffer_data_length = 0;

    CHECK (snd_pcm_drop, alsa_handle);
    CHECK (snd_pcm_prepare, alsa_handle);

FAILED:
    g_mutex_unlock (alsa_mutex);
}

static void alsa_pause (gshort pause)
{
    g_mutex_lock (alsa_mutex);

    DEBUG ("%sause.\n", pause ? "P" : "Unp");
    alsa_paused = pause;
    CHECK (snd_pcm_pause, alsa_handle, pause);

FAILED:
    g_mutex_unlock (alsa_mutex);
}

static void alsa_get_volume (gint * left, gint * right)
{
    glong left_l = 0, right_l = 0;

    if (alsa_mixer_elem == NULL)
        goto FAILED;

    CHECK (snd_mixer_handle_events, alsa_mixer);

    if (snd_mixer_selem_is_playback_mono (alsa_mixer_elem))
    {
        CHECK (snd_mixer_selem_get_playback_volume, alsa_mixer_elem,
         SND_MIXER_SCHN_MONO, & left_l);
        right_l = left_l;
    }
    else
    {
        CHECK (snd_mixer_selem_get_playback_volume, alsa_mixer_elem,
         SND_MIXER_SCHN_FRONT_LEFT, & left_l);
        CHECK (snd_mixer_selem_get_playback_volume, alsa_mixer_elem,
         SND_MIXER_SCHN_FRONT_RIGHT, & right_l);
    }

FAILED:
    * left = left_l;
    * right = right_l;
}

static void alsa_set_volume (gint left, gint right)
{
    if (alsa_mixer_elem == NULL)
        goto FAILED;

    if (snd_mixer_selem_is_playback_mono (alsa_mixer_elem))
        CHECK (snd_mixer_selem_set_playback_volume, alsa_mixer_elem,
         SND_MIXER_SCHN_MONO, MAX (left, right));
    else
    {
        CHECK (snd_mixer_selem_set_playback_volume, alsa_mixer_elem,
         SND_MIXER_SCHN_FRONT_LEFT, left);
        CHECK (snd_mixer_selem_set_playback_volume, alsa_mixer_elem,
         SND_MIXER_SCHN_FRONT_RIGHT, right);
    }

    CHECK (snd_mixer_handle_events, alsa_mixer);

FAILED:
    return;
}

static void about_response (GtkDialog * dialog, gint response, void * unused)
{
    gtk_widget_destroy ((GtkWidget *) dialog);
}

void alsa_about (void)
{
    const char markup[] = "<b>ALSA Gapless Output Plugin for Audacious</b>\n"
     "Copyright 2009 John Lindgren\n\n"
     "My thanks to William Pitcock, author of the ALSA Output Plugin NG, whose "
     "code served as a reference when the ALSA manual was not enough.\n\n"
     "Redistribution and use in source and binary forms, with or without "
     "modification, are permitted provided that the following conditions are "
     "met:\n\n"
     "1. Redistributions of source code must retain the above copyright "
     "notice, this list of conditions, and the following disclaimer.\n\n"
     "2. Redistributions in binary form must reproduce the above copyright "
     "notice, this list of conditions, and the following disclaimer in the "
     "documentation provided with the distribution.\n\n"
     "This software is provided \"as is\" and without any warranty, express or "
     "implied. In no event shall the authors be liable for any damages arising "
     "from the use of this software.";

    static GtkWidget * window = NULL;

    if (window == NULL)
    {
        window = gtk_message_dialog_new_with_markup (NULL, 0, GTK_MESSAGE_INFO,
         GTK_BUTTONS_OK, markup);
        g_signal_connect ((GObject *) window, "response", (GCallback)
         about_response, NULL);
        g_signal_connect ((GObject *) window, "destroy", (GCallback)
         gtk_widget_destroyed, & window);
    }

    gtk_window_present ((GtkWindow *) window);
}

static OutputPlugin alsa_plugin =
{
    .description = "ALSA Gapless Output Plugin (experimental)",
    .probe_priority = 0, /* lowest -- it's experimental */
    .init = alsa_init,
    .cleanup = alsa_cleanup,
    .open_audio = alsa_open_audio,
    .close_audio = alsa_close_audio,
    .write_audio = alsa_write_audio,
    .written_time = alsa_written_time,
    .output_time = alsa_output_time,
    .buffer_free = alsa_buffer_free,
    .buffer_playing = alsa_buffer_playing,
    .flush = alsa_flush,
    .pause = alsa_pause,
    .set_volume = alsa_set_volume,
    .get_volume = alsa_get_volume,
    .about = alsa_about,
    #if 0 /* not written yet */
    .configure = alsa_configure,
    #endif
};

OutputPlugin * alsa_plugin_list[] = {& alsa_plugin, NULL};
SIMPLE_OUTPUT_PLUGIN (alsa-gapless, alsa_plugin_list);
