/*
 * LADSPA Host for Audacious
 * Copyright 2011 John Lindgren
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
#include <stdio.h>

#include "ladspa.h"
#include "plugin.h"

static int ladspa_channels, ladspa_rate;

static void start_plugin (LoadedPlugin * loaded)
{
    if (loaded->active)
        return;

    loaded->active = 1;

    PluginData * plugin = loaded->plugin;
    const LADSPA_Descriptor * desc = plugin->desc;

    int ports = plugin->in_ports->len;

    if (ports == 0 || ports != plugin->out_ports->len)
    {
        fprintf (stderr, "Plugin has unusable port configuration: %s\n", desc->Name);
        return;
    }

    if (ladspa_channels % ports != 0)
    {
        fprintf (stderr, "Plugin cannot be used with %d channels: %s\n",
         ladspa_channels, desc->Name);
        return;
    }

    int instances = ladspa_channels / ports;

    loaded->instances = index_new ();
    loaded->in_bufs = g_malloc (sizeof (float *) * ladspa_channels);
    loaded->out_bufs = g_malloc (sizeof (float *) * ladspa_channels);

    for (int i = 0; i < instances; i ++)
    {
        LADSPA_Handle handle = desc->instantiate (desc, ladspa_rate);
        index_append (loaded->instances, handle);

        int controls = index_count (plugin->controls);
        for (int c = 0; c < controls; c ++)
        {
            ControlData * control = index_get (plugin->controls, c);
            desc->connect_port (handle, control->port, & loaded->values[c]);
        }

        for (int p = 0; p < ports; p ++)
        {
            int channel = ports * i + p;

            float * in = g_malloc (sizeof (float) * LADSPA_BUFLEN);
            loaded->in_bufs[channel] = in;
            int in_port = g_array_index (plugin->in_ports, int, p);
            desc->connect_port (handle, in_port, in);

            float * out = g_malloc (sizeof (float) * LADSPA_BUFLEN);
            loaded->out_bufs[channel] = out;
            int out_port = g_array_index (plugin->out_ports, int, p);
            desc->connect_port (handle, out_port, out);
        }

        if (desc->activate)
            desc->activate (handle);
    }
}

static void run_plugin (LoadedPlugin * loaded, float * data, int samples)
{
    if (! loaded->instances)
        return;

    PluginData * plugin = loaded->plugin;
    const LADSPA_Descriptor * desc = plugin->desc;

    int ports = plugin->in_ports->len;
    int instances = index_count (loaded->instances);
    assert (ports * instances == ladspa_channels);

    while (samples / ladspa_channels > 0)
    {
        int frames = MIN (samples / ladspa_channels, LADSPA_BUFLEN);

        for (int i = 0; i < instances; i ++)
        {
            LADSPA_Handle * handle = index_get (loaded->instances, i);

            for (int p = 0; p < ports; p ++)
            {
                int channel = ports * i + p;
                float * get = data + channel;
                float * in = loaded->in_bufs[channel];
                float * in_end = in + frames;

                while (in < in_end)
                {
                    * in ++ = * get;
                    get += ladspa_channels;
                }
            }

            desc->run (handle, frames);

            for (int p = 0; p < ports; p ++)
            {
                int channel = ports * i + p;
                float * set = data + channel;
                float * out = loaded->out_bufs[channel];
                float * out_end = out + frames;

                while (out < out_end)
                {
                    * set = * out ++;
                    set += ladspa_channels;
                }
            }
        }

        data += ladspa_channels * frames;
        samples -= ladspa_channels * frames;
    }
}

static void flush_plugin (LoadedPlugin * loaded)
{
    if (! loaded->instances)
        return;

    PluginData * plugin = loaded->plugin;
    const LADSPA_Descriptor * desc = plugin->desc;

    int instances = index_count (loaded->instances);
    for (int i = 0; i < instances; i ++)
    {
        LADSPA_Handle * handle = index_get (loaded->instances, i);

        if (desc->deactivate)
            desc->deactivate (handle);
        if (desc->activate)
            desc->activate (handle);
    }
}

void shutdown_plugin_locked (LoadedPlugin * loaded)
{
    loaded->active = 0;

    if (! loaded->instances)
        return;

    PluginData * plugin = loaded->plugin;
    const LADSPA_Descriptor * desc = plugin->desc;

    int instances = index_count (loaded->instances);
    for (int i = 0; i < instances; i ++)
    {
        LADSPA_Handle * handle = index_get (loaded->instances, i);

        if (desc->deactivate)
            desc->deactivate (handle);

        desc->cleanup (handle);
    }

    for (int channel = 0; channel < ladspa_channels; channel ++)
    {
        g_free (loaded->in_bufs[channel]);
        g_free (loaded->out_bufs[channel]);
    }

    index_free (loaded->instances);
    loaded->instances = NULL;
    g_free (loaded->in_bufs);
    loaded->in_bufs = NULL;
    g_free (loaded->out_bufs);
    loaded->out_bufs = NULL;
}

void ladspa_start (int * channels, int * rate)
{
    pthread_mutex_lock (& mutex);

    int count = index_count (loadeds);
    for (int i = 0; i < count; i ++)
    {
        LoadedPlugin * loaded = index_get (loadeds, i);
        shutdown_plugin_locked (loaded);
    }

    ladspa_channels = * channels;
    ladspa_rate = * rate;

    pthread_mutex_unlock (& mutex);
}

void ladspa_process (float * * data, int * samples)
{
    pthread_mutex_lock (& mutex);

    int count = index_count (loadeds);
    for (int i = 0; i < count; i ++)
    {
        LoadedPlugin * loaded = index_get (loadeds, i);
        start_plugin (loaded);
        run_plugin (loaded, * data, * samples);
    }

    pthread_mutex_unlock (& mutex);
}

void ladspa_flush (void)
{
    pthread_mutex_lock (& mutex);

    int count = index_count (loadeds);
    for (int i = 0; i < count; i ++)
    {
        LoadedPlugin * loaded = index_get (loadeds, i);
        flush_plugin (loaded);
    }

    pthread_mutex_unlock (& mutex);
}

void ladspa_finish (float * * data, int * samples)
{
    pthread_mutex_lock (& mutex);

    int count = index_count (loadeds);
    for (int i = 0; i < count; i ++)
    {
        LoadedPlugin * loaded = index_get (loadeds, i);
        start_plugin (loaded);
        run_plugin (loaded, * data, * samples);
        shutdown_plugin_locked (loaded);
    }

    pthread_mutex_unlock (& mutex);
}
