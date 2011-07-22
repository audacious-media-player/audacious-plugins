#ifndef AUD_LADSPA_PLUGIN_H
#define AUD_LADSPA_PLUGIN_H

#include <libaudcore/index.h>

#include "ladspa.h"

typedef struct {
    int port;
    char * name;
    float min, max, def;
} ControlData;

typedef struct {
    char * path;
    const LADSPA_Descriptor * desc;
    struct index * controls; /* (ControlData *) */
    char selected;
} PluginData;

typedef struct {
    PluginData * plugin;
    float * values;
    char selected;
} LoadedPlugin;

/* plugin.c */

/* The mutex needs to be locked when the main thread is writing to the data
 * structures below (but not when it is only reading from them) and when the
 * audio thread is reading from them. */

extern pthread_mutex_t mutex;
extern char * module_path;
extern struct index * modules; /* (void *) returned by dlopen() */
extern struct index * plugins; /* (PluginData *) */
extern struct index * loadeds; /* (LoadedPlugin *) */

LoadedPlugin * enable_plugin_locked (PluginData * plugin);
void disable_plugin_locked (int i);

/* plugin-list.c */

GtkWidget * create_plugin_list (void);
void update_plugin_list (GtkWidget * list);

/* loaded-list.c */

GtkWidget * create_loaded_list (void);
void update_loaded_list (GtkWidget * list);

#endif
