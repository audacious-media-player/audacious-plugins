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

#include "ladspa.h"
#include "plugin.h"

#include <libaudcore/runtime.h>

static int ladspa_channels, ladspa_rate;

static void start_plugin (LoadedPlugin & loaded)
{
    if (loaded.active)
        return;

    loaded.active = 1;

    PluginData & plugin = loaded.plugin;
    const LADSPA_Descriptor & desc = plugin.desc;

    int ports = plugin.in_ports.len ();

    if (ports == 0 || ports != plugin.out_ports.len ())
    {
        AUDERR ("Plugin has unusable port configuration: %s\n", desc.Name);
        return;
    }

    if (ladspa_channels % ports != 0)
    {
        AUDERR ("Plugin cannot be used with %d channels: %s\n",
         ladspa_channels, desc.Name);
        return;
    }

    int instances = ladspa_channels / ports;

    loaded.in_bufs.insert (0, ladspa_channels);
    loaded.out_bufs.insert (0, ladspa_channels);

    for (int i = 0; i < instances; i ++)
    {
        LADSPA_Handle handle = desc.instantiate (& desc, ladspa_rate);
        loaded.instances.append (handle);

        int controls = plugin.controls.len ();
        for (int c = 0; c < controls; c ++)
            desc.connect_port (handle, plugin.controls[c].port, & loaded.values[c]);

        for (int p = 0; p < ports; p ++)
        {
            int channel = ports * i + p;

            Index<float> & in = loaded.in_bufs[channel];
            in.insert (0, LADSPA_BUFLEN);
            desc.connect_port (handle, plugin.in_ports[p], in.begin ());

            Index<float> & out = loaded.out_bufs[channel];
            out.insert (0, LADSPA_BUFLEN);
            desc.connect_port (handle, plugin.out_ports[p], out.begin ());
        }

        if (desc.activate)
            desc.activate (handle);
    }
}

static void run_plugin (LoadedPlugin & loaded, float * data, int samples)
{
    if (! loaded.instances.len ())
        return;

    PluginData & plugin = loaded.plugin;
    const LADSPA_Descriptor & desc = plugin.desc;

    int ports = plugin.in_ports.len ();
    int instances = loaded.instances.len ();
    assert (ports * instances == ladspa_channels);

    while (samples / ladspa_channels > 0)
    {
        int frames = aud::min (samples / ladspa_channels, LADSPA_BUFLEN);

        for (int i = 0; i < instances; i ++)
        {
            LADSPA_Handle handle = loaded.instances[i];

            for (int p = 0; p < ports; p ++)
            {
                int channel = ports * i + p;
                float * get = data + channel;
                float * in = loaded.in_bufs[channel].begin ();
                float * in_end = in + frames;

                while (in < in_end)
                {
                    * in ++ = * get;
                    get += ladspa_channels;
                }
            }

            desc.run (handle, frames);

            for (int p = 0; p < ports; p ++)
            {
                int channel = ports * i + p;
                float * set = data + channel;
                float * out = loaded.out_bufs[channel].begin ();
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

static void flush_plugin (LoadedPlugin & loaded)
{
    if (! loaded.instances.len ())
        return;

    PluginData & plugin = loaded.plugin;
    const LADSPA_Descriptor & desc = plugin.desc;

    int instances = loaded.instances.len ();
    for (int i = 0; i < instances; i ++)
    {
        LADSPA_Handle handle = loaded.instances[i];

        if (desc.deactivate)
            desc.deactivate (handle);
        if (desc.activate)
            desc.activate (handle);
    }
}

void shutdown_plugin_locked (LoadedPlugin & loaded)
{
    loaded.active = 0;

    if (! loaded.instances.len ())
        return;

    PluginData & plugin = loaded.plugin;
    const LADSPA_Descriptor & desc = plugin.desc;

    int instances = loaded.instances.len ();
    for (int i = 0; i < instances; i ++)
    {
        LADSPA_Handle handle = loaded.instances[i];

        if (desc.deactivate)
            desc.deactivate (handle);

        desc.cleanup (handle);
    }

    loaded.instances.clear ();
    loaded.in_bufs.clear ();
    loaded.out_bufs.clear ();
}

void LADSPAHost::start (int & channels, int & rate)
{
    pthread_mutex_lock (& mutex);

    for (auto & loaded : loadeds)
        shutdown_plugin_locked (* loaded);

    ladspa_channels = channels;
    ladspa_rate = rate;

    pthread_mutex_unlock (& mutex);
}

Index<float> & LADSPAHost::process (Index<float> & data)
{
    pthread_mutex_lock (& mutex);

    for (auto & loaded : loadeds)
    {
        start_plugin (* loaded);
        run_plugin (* loaded, data.begin (), data.len ());
    }

    pthread_mutex_unlock (& mutex);
    return data;
}

bool LADSPAHost::flush (bool force)
{
    pthread_mutex_lock (& mutex);

    for (auto & loaded : loadeds)
        flush_plugin (* loaded);

    pthread_mutex_unlock (& mutex);
    return true;
}

Index<float> & LADSPAHost::finish (Index<float> & data, bool end_of_playlist)
{
    pthread_mutex_lock (& mutex);

    for (auto & loaded : loadeds)
    {
        start_plugin (* loaded);
        run_plugin (* loaded, data.begin (), data.len ());

        if (end_of_playlist)
            shutdown_plugin_locked (* loaded);
    }

    pthread_mutex_unlock (& mutex);
    return data;
}
