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

#include "ladspa.h"

#define LADSPA_BUFLEN 1024

typedef struct {
    int port;
    char * name;
    char is_toggle;
    float min, max, def;
} ControlData;

typedef struct {
    char * path;
    const LADSPA_Descriptor * desc;
    Index<ControlData *> controls;
    GArray * in_ports, * out_ports; /* (int) */
    char selected;
} PluginData;

typedef struct {
    PluginData * plugin;
    float * values;
    char selected;
    char active;
    Index<LADSPA_Handle> instances;
    float * * in_bufs, * * out_bufs;
    GtkWidget * settings_win;
} LoadedPlugin;

/* plugin.c */

/* The mutex needs to be locked when the main thread is writing to the data
 * structures below (but not when it is only reading from them) and when the
 * audio thread is reading from them. */

extern pthread_mutex_t mutex;
extern char * module_path;
extern Index<GModule *> modules;
extern Index<PluginData *> plugins;
extern Index<LoadedPlugin *> loadeds;

extern GtkWidget * about_win;
extern GtkWidget * config_win;
extern GtkWidget * plugin_list;
extern GtkWidget * loaded_list;

LoadedPlugin * enable_plugin_locked (PluginData * plugin);
void disable_plugin_locked (int i);

/* effect.c */

void shutdown_plugin_locked (LoadedPlugin * loaded);

void ladspa_start (gint * channels, gint * rate);
void ladspa_process (gfloat * * data, gint * samples);
void ladspa_flush (void);
void ladspa_finish (gfloat * * data, gint * samples);

/* plugin-list.c */

GtkWidget * create_plugin_list (void);
void update_plugin_list (GtkWidget * list);

/* loaded-list.c */

GtkWidget * create_loaded_list (void);
void update_loaded_list (GtkWidget * list);

#endif
