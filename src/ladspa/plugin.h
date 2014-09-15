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

#ifndef AUD_LADSPA_PLUGIN_H
#define AUD_LADSPA_PLUGIN_H

#include <pthread.h>
#include <gtk/gtk.h>

#include <libaudcore/index.h>
#include <libaudcore/objects.h>

#include "ladspa.h"

#define LADSPA_BUFLEN 1024

struct ControlData {
    int port;
    String name;
    bool is_toggle;
    float min, max, def;
};

struct PluginData
{
    String path;
    const LADSPA_Descriptor & desc;
    Index<ControlData> controls;
    Index<int> in_ports, out_ports;
    bool selected = false;

    PluginData (const char * path, const LADSPA_Descriptor & desc) :
        path (path),
        desc (desc) {}
};

struct LoadedPlugin
{
    PluginData & plugin;
    Index<float> values;
    bool selected = false;
    bool active = false;
    Index<LADSPA_Handle> instances;
    Index<Index<float>> in_bufs, out_bufs;
    GtkWidget * settings_win = nullptr;

    LoadedPlugin (PluginData & plugin) :
        plugin (plugin) {}
};

/* plugin.c */

/* The mutex needs to be locked when the main thread is writing to the data
 * structures below (but not when it is only reading from them) and when the
 * audio thread is reading from them. */

extern pthread_mutex_t mutex;
extern String module_path;
extern Index<GModule *> modules;
extern Index<SmartPtr<PluginData>> plugins;
extern Index<SmartPtr<LoadedPlugin>> loadeds;

extern GtkWidget * about_win;
extern GtkWidget * config_win;
extern GtkWidget * plugin_list;
extern GtkWidget * loaded_list;

LoadedPlugin & enable_plugin_locked (PluginData & plugin);
void disable_plugin_locked (LoadedPlugin & loaded);

/* effect.c */

void shutdown_plugin_locked (LoadedPlugin & loaded);

void ladspa_start (int * channels, int * rate);
void ladspa_process (float * * data, int * samples);
void ladspa_flush (void);
void ladspa_finish (float * * data, int * samples);

/* plugin-list.c */

GtkWidget * create_plugin_list (void);
void update_plugin_list (GtkWidget * list);

/* loaded-list.c */

GtkWidget * create_loaded_list (void);
void update_loaded_list (GtkWidget * list);

#endif
