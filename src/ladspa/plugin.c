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

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gmodule.h>
#include <gtk/gtk.h>

#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudcore/plugin.h>
#include <libaudcore/audstrings.h>
#include <libaudgui/libaudgui-gtk.h>

#include "plugin.h"

static const gchar * const ladspa_defaults[] = {
 "plugin_count", "0",
 NULL};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
char * module_path;
Index * modules; /* (void *) */
Index * plugins; /* (PluginData *) */
Index * loadeds; /* (LoadedPlugin *) */

GtkWidget * config_win;
GtkWidget * plugin_list;
GtkWidget * loaded_list;

static ControlData * parse_control (const LADSPA_Descriptor * desc, int port)
{
    g_return_val_if_fail (desc->PortNames[port], NULL);
    const LADSPA_PortRangeHint * hint = & desc->PortRangeHints[port];

    ControlData * control = g_slice_new (ControlData);
    control->port = port;
    control->name = g_strdup (desc->PortNames[port]);
    control->is_toggle = LADSPA_IS_HINT_TOGGLED (hint->HintDescriptor) ? 1 : 0;

    control->min = LADSPA_IS_HINT_BOUNDED_BELOW (hint->HintDescriptor) ? hint->LowerBound :
     LADSPA_IS_HINT_BOUNDED_ABOVE (hint->HintDescriptor) ? hint->UpperBound - 100 : -100;
    control->max = LADSPA_IS_HINT_BOUNDED_ABOVE (hint->HintDescriptor) ? hint->UpperBound :
     LADSPA_IS_HINT_BOUNDED_BELOW (hint->HintDescriptor) ? hint->LowerBound + 100 : 100;

    if (LADSPA_IS_HINT_SAMPLE_RATE (hint->HintDescriptor))
    {
        control->min *= 96000;
        control->max *= 96000;
    }

    if (LADSPA_IS_HINT_DEFAULT_0 (hint->HintDescriptor))
        control->def = 0;
    else if (LADSPA_IS_HINT_DEFAULT_1 (hint->HintDescriptor))
        control->def = 1;
    else if (LADSPA_IS_HINT_DEFAULT_100 (hint->HintDescriptor))
        control->def = 100;
    else if (LADSPA_IS_HINT_DEFAULT_440 (hint->HintDescriptor))
        control->def = 440;
    else if (LADSPA_IS_HINT_DEFAULT_MINIMUM (hint->HintDescriptor))
        control->def = control->min;
    else if (LADSPA_IS_HINT_DEFAULT_MAXIMUM (hint->HintDescriptor))
        control->def = control->max;
    else if (LADSPA_IS_HINT_DEFAULT_LOW (hint->HintDescriptor))
    {
        if (LADSPA_IS_HINT_LOGARITHMIC (hint->HintDescriptor))
            control->def = expf (0.75 * logf (control->min) + 0.25 * logf (control->max));
        else
            control->def = 0.75 * control->min + 0.25 * control->max;
    }
    else if (LADSPA_IS_HINT_DEFAULT_HIGH (hint->HintDescriptor))
    {
        if (LADSPA_IS_HINT_LOGARITHMIC (hint->HintDescriptor))
            control->def = expf (0.25 * logf (control->min) + 0.75 * logf (control->max));
        else
            control->def = 0.25 * control->min + 0.75 * control->max;
    }
    else
    {
        if (LADSPA_IS_HINT_LOGARITHMIC (hint->HintDescriptor))
            control->def = expf (0.5 * logf (control->min) + 0.5 * logf (control->max));
        else
            control->def = 0.5 * control->min + 0.5 * control->max;
    }

    return control;
}

static void free_control (ControlData * control)
{
    g_free (control->name);
    g_slice_free (ControlData, control);
}

static PluginData * open_plugin (const char * path, const LADSPA_Descriptor * desc)
{
    const char * slash = strrchr (path, G_DIR_SEPARATOR);
    g_return_val_if_fail (slash && slash[1], NULL);
    g_return_val_if_fail (desc->Label && desc->Name, NULL);

    PluginData * plugin = g_slice_new (PluginData);
    plugin->path = g_strdup (slash + 1);
    plugin->desc = desc;
    plugin->controls = index_new ();
    plugin->in_ports = g_array_new (0, 0, sizeof (int));
    plugin->out_ports = g_array_new (0, 0, sizeof (int));
    plugin->selected = 0;

    for (int i = 0; i < desc->PortCount; i ++)
    {
        if (LADSPA_IS_PORT_CONTROL (desc->PortDescriptors[i]))
        {
            ControlData * control = parse_control (desc, i);
            if (control)
                index_insert (plugin->controls, -1, control);
        }
        else if (LADSPA_IS_PORT_AUDIO (desc->PortDescriptors[i]) &&
         LADSPA_IS_PORT_INPUT (desc->PortDescriptors[i]))
            g_array_append_val (plugin->in_ports, i);
        else if (LADSPA_IS_PORT_AUDIO (desc->PortDescriptors[i]) &&
         LADSPA_IS_PORT_OUTPUT (desc->PortDescriptors[i]))
            g_array_append_val (plugin->out_ports, i);
    }

    return plugin;
}

static void close_plugin (PluginData * plugin)
{
    g_free (plugin->path);
    index_free_full (plugin->controls, (IndexFreeFunc) free_control);
    g_array_free (plugin->in_ports, 1);
    g_array_free (plugin->out_ports, 1);
    g_slice_free (PluginData, plugin);
}

static void * open_module (const char * path)
{
    GModule * handle = g_module_open (path, G_MODULE_BIND_LOCAL);
    if (! handle)
    {
        fprintf (stderr, "ladspa: Failed to open module %s: %s\n", path, g_module_error ());
        return NULL;
    }

    void * sym;
    if (! g_module_symbol (handle, "ladspa_descriptor", & sym))
    {
        fprintf (stderr, "ladspa: Not a valid LADSPA module: %s\n", path);
        g_module_close (handle);
        return NULL;
    }

    LADSPA_Descriptor_Function descfun = (LADSPA_Descriptor_Function) sym;

    const LADSPA_Descriptor * desc;
    for (int i = 0; (desc = descfun (i)); i ++)
    {
        PluginData * plugin = open_plugin (path, desc);
        if (plugin)
            index_insert (plugins, -1, plugin);
    }

    return handle;
}

static void open_modules_for_path (const char * path)
{
    GDir * folder = g_dir_open (path, 0, NULL);
    if (! folder)
    {
        fprintf (stderr, "ladspa: Failed to read folder %s: %s\n", path, strerror (errno));
        return;
    }

    const char * name;
    while ((name = g_dir_read_name (folder)))
    {
        if (! str_has_suffix_nocase (name, G_MODULE_SUFFIX))
            continue;

        char * filename = filename_build (path, name);
        void * handle = open_module (filename);

        if (handle)
            index_insert (modules, -1, handle);

        str_unref (filename);
    }

    g_dir_close (folder);
}

static void open_modules_for_paths (const char * paths)
{
    if (! paths || ! paths[0])
        return;

    char * * split = g_strsplit (paths, ":", -1);

    for (int i = 0; split[i]; i ++)
        open_modules_for_path (split[i]);

    g_strfreev (split);
}

static void open_modules (void)
{
    open_modules_for_paths (getenv ("LADSPA_PATH"));
    open_modules_for_paths (module_path);
}

static void close_modules (void)
{
    index_delete_full (plugins, 0, -1, (IndexFreeFunc) close_plugin);
    index_delete_full (modules, 0, -1, (IndexFreeFunc) g_module_close);
}

LoadedPlugin * enable_plugin_locked (PluginData * plugin)
{
    LoadedPlugin * loaded = g_slice_new (LoadedPlugin);
    loaded->plugin = plugin;
    loaded->selected = 0;

    int count = index_count (plugin->controls);
    loaded->values = g_malloc (sizeof (float) * count);

    for (int i = 0; i < count; i ++)
    {
        ControlData * control = index_get (plugin->controls, i);
        loaded->values[i] = control->def;
    }

    loaded->active = 0;
    loaded->instances = NULL;
    loaded->in_bufs = NULL;
    loaded->out_bufs = NULL;

    loaded->settings_win = NULL;

    index_insert (loadeds, -1, loaded);
    return loaded;
}

void disable_plugin_locked (int i)
{
    g_return_if_fail (i >= 0 && i < index_count (loadeds));
    LoadedPlugin * loaded = index_get (loadeds, i);

    if (loaded->settings_win)
        gtk_widget_destroy (loaded->settings_win);

    shutdown_plugin_locked (loaded);

    g_free (loaded->values);
    g_slice_free (LoadedPlugin, loaded);
    index_delete (loadeds, i, 1);
}

static PluginData * find_plugin (const char * path, const char * label)
{
    int count = index_count (plugins);
    for (int i = 0; i < count; i ++)
    {
        PluginData * plugin = index_get (plugins, i);
        if (! strcmp (plugin->path, path) && ! strcmp (plugin->desc->Label, label))
            return plugin;
    }

    return NULL;
}

static void save_enabled_to_config (void)
{
    int count = index_count (loadeds);
    int old_count = aud_get_int ("ladspa", "plugin_count");
    aud_set_int ("ladspa", "plugin_count", count);

    for (int i = 0; i < count; i ++)
    {
        LoadedPlugin * loaded = index_get (loadeds, 0);
        char key[32];

        snprintf (key, sizeof key, "plugin%d_path", i);
        aud_set_str ("ladspa", key, loaded->plugin->path);

        snprintf (key, sizeof key, "plugin%d_label", i);
        aud_set_str ("ladspa", key, loaded->plugin->desc->Label);

        snprintf (key, sizeof key, "plugin%d_controls", i);

        int ccount = index_count (loaded->plugin->controls);
        double temp[ccount];

        for (int ci = 0; ci < ccount; ci ++)
            temp[ci] = loaded->values[ci];

        char * controls = double_array_to_str (temp, ccount);
        aud_set_str ("ladspa", key, controls);
        str_unref (controls);

        disable_plugin_locked (0);
    }

    for (int i = count; i < old_count; i ++)
    {
        char key[32];

        snprintf (key, sizeof key, "plugin%d_path", i);
        aud_set_str ("ladspa", key, "");

        snprintf (key, sizeof key, "plugin%d_label", i);
        aud_set_str ("ladspa", key, "");

        snprintf (key, sizeof key, "plugin%d_controls", i);
        aud_set_str ("ladspa", key, "");
    }
}

static void load_enabled_from_config (void)
{
    int count = aud_get_int ("ladspa", "plugin_count");

    for (int i = 0; i < count; i ++)
    {
        char key[32];

        snprintf (key, sizeof key, "plugin%d_path", i);
        char * path = aud_get_str ("ladspa", key);

        snprintf (key, sizeof key, "plugin%d_label", i);
        char * label = aud_get_str ("ladspa", key);

        PluginData * plugin = find_plugin (path, label);
        if (plugin)
        {
            LoadedPlugin * loaded = enable_plugin_locked (plugin);

            snprintf (key, sizeof key, "plugin%d_controls", i);

            int ccount = index_count (loaded->plugin->controls);
            double temp[ccount];

            char * controls = aud_get_str ("ladspa", key);

            if (str_to_double_array (controls, temp, ccount))
            {
                for (int ci = 0; ci < ccount; ci ++)
                    loaded->values[ci] = temp[ci];
            }
            else
            {
                /* migrate from old config format */
                for (int ci = 0; ci < ccount; ci ++)
                {
                    snprintf (key, sizeof key, "plugin%d_control%d", i, ci);
                    loaded->values[ci] = aud_get_double ("ladspa", key);
                    aud_set_str ("ladspa", key, "");
                }
            }

            str_unref (controls);
        }

        str_unref (path);
        str_unref (label);
    }
}

static int init (void)
{
    pthread_mutex_lock (& mutex);

    modules = index_new ();
    plugins = index_new ();
    loadeds = index_new ();

    aud_config_set_defaults ("ladspa", ladspa_defaults);

    module_path = aud_get_str ("ladspa", "module_path");

    open_modules ();
    load_enabled_from_config ();

    pthread_mutex_unlock (& mutex);
    return 1;
}

static void cleanup (void)
{
    if (config_win)
        gtk_widget_destroy (config_win);

    pthread_mutex_lock (& mutex);

    aud_set_str ("ladspa", "module_path", module_path);
    save_enabled_to_config ();
    close_modules ();

    index_free (modules);
    modules = NULL;
    index_free (plugins);
    plugins = NULL;
    index_free (loadeds);
    loadeds = NULL;

    str_unref (module_path);
    module_path = NULL;

    pthread_mutex_unlock (& mutex);
}

static void set_module_path (GtkEntry * entry)
{
    pthread_mutex_lock (& mutex);

    save_enabled_to_config ();
    close_modules ();

    str_unref (module_path);
    module_path = str_get (gtk_entry_get_text (entry));

    open_modules ();
    load_enabled_from_config ();

    pthread_mutex_unlock (& mutex);

    if (plugin_list)
        update_plugin_list (plugin_list);
    if (loaded_list)
        update_loaded_list (loaded_list);
}

static void enable_selected (void)
{
    pthread_mutex_lock (& mutex);

    int count = index_count (plugins);
    for (int i = 0; i < count; i ++)
    {
        PluginData * plugin = index_get (plugins, i);
        if (plugin->selected)
            enable_plugin_locked (plugin);
    }

    pthread_mutex_unlock (& mutex);

    if (loaded_list)
        update_loaded_list (loaded_list);
}

static void disable_selected (void)
{
    pthread_mutex_lock (& mutex);

    int count = index_count (loadeds);
    int offset = 0;
    for (int i = 0; i < count; i ++)
    {
        LoadedPlugin * loaded = index_get (loadeds, i - offset);
        if (loaded->selected)
        {
            disable_plugin_locked (i - offset);
            offset ++;
        }
    }

    pthread_mutex_unlock (& mutex);

    if (loaded_list)
        update_loaded_list (loaded_list);
}

static void control_toggled (GtkToggleButton * toggle, float * value)
{
    pthread_mutex_lock (& mutex);
    * value = gtk_toggle_button_get_active (toggle) ? 1 : 0;
    pthread_mutex_unlock (& mutex);
}

static void control_changed (GtkSpinButton * spin, float * value)
{
    pthread_mutex_lock (& mutex);
    * value = gtk_spin_button_get_value (spin);
    pthread_mutex_unlock (& mutex);
}

static void configure_plugin (LoadedPlugin * loaded)
{
    if (loaded->settings_win)
    {
        gtk_window_present ((GtkWindow *) loaded->settings_win);
        return;
    }

    PluginData * plugin = loaded->plugin;
    char buf[200];

    snprintf (buf, sizeof buf, _("%s Settings"), plugin->desc->Name);
    loaded->settings_win = gtk_dialog_new_with_buttons (buf, (GtkWindow *)
     config_win, GTK_DIALOG_DESTROY_WITH_PARENT, _("_Close"),
     GTK_RESPONSE_CLOSE, NULL);
    gtk_window_set_resizable ((GtkWindow *) loaded->settings_win, 0);

    GtkWidget * vbox = gtk_dialog_get_content_area ((GtkDialog *) loaded->settings_win);

    int count = index_count (plugin->controls);
    for (int i = 0; i < count; i ++)
    {
        ControlData * control = index_get (plugin->controls, i);

        GtkWidget * hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_pack_start ((GtkBox *) vbox, hbox, 0, 0, 0);

        if (control->is_toggle)
        {
            GtkWidget * toggle = gtk_check_button_new_with_label (control->name);
            gtk_toggle_button_set_active ((GtkToggleButton *) toggle, (loaded->values[i] > 0) ? 1 : 0);
            gtk_box_pack_start ((GtkBox *) hbox, toggle, 0, 0, 0);

            g_signal_connect (toggle, "toggled", (GCallback) control_toggled, & loaded->values[i]);
        }
        else
        {
            snprintf (buf, sizeof buf, "%s:", control->name);
            GtkWidget * label = gtk_label_new (buf);
            gtk_box_pack_start ((GtkBox *) hbox, label, 0, 0, 0);

            GtkWidget * spin = gtk_spin_button_new_with_range (control->min, control->max, 0.01);
            gtk_spin_button_set_value ((GtkSpinButton *) spin, loaded->values[i]);
            gtk_box_pack_start ((GtkBox *) hbox, spin, 0, 0, 0);

            g_signal_connect (spin, "value-changed", (GCallback) control_changed, & loaded->values[i]);
        }
    }

    g_signal_connect (loaded->settings_win, "response", (GCallback) gtk_widget_destroy, NULL);
    g_signal_connect (loaded->settings_win, "destroy", (GCallback)
     gtk_widget_destroyed, & loaded->settings_win);

    gtk_widget_show_all (loaded->settings_win);
}

static void configure_selected (void)
{
    pthread_mutex_lock (& mutex);

    int count = index_count (loadeds);
    for (int i = 0; i < count; i ++)
    {
        LoadedPlugin * loaded = index_get (loadeds, i);
        if (loaded->selected)
            configure_plugin (loaded);
    }

    pthread_mutex_unlock (& mutex);
}

static void configure (void)
{
    if (config_win)
    {
        gtk_window_present ((GtkWindow *) config_win);
        return;
    }

    config_win = gtk_dialog_new_with_buttons (_("LADSPA Host Settings"), NULL,
     0, _("_Close"), GTK_RESPONSE_CLOSE, NULL);
    gtk_window_set_default_size ((GtkWindow *) config_win, 480, 360);

    GtkWidget * vbox = gtk_dialog_get_content_area ((GtkDialog *) config_win);

    GtkWidget * hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start ((GtkBox *) vbox, hbox, 0, 0, 0);

    GtkWidget * label = gtk_label_new (_("Module paths:"));
    gtk_box_pack_start ((GtkBox *) hbox, label, 0, 0, 0);

    label = gtk_label_new (0);
    gtk_label_set_markup ((GtkLabel *) label,
     _("<small>Separate multiple paths with a colon.\n"
     "These paths are searched in addition to LADSPA_PATH.\n"
     "After adding new paths, press Enter to scan for new plugins.</small>"));
    gtk_misc_set_padding ((GtkMisc *) label, 12, 6);
    gtk_misc_set_alignment ((GtkMisc *) label, 0, 0);
    gtk_box_pack_start ((GtkBox *) vbox, label, 0, 0, 0);

    GtkWidget * entry = gtk_entry_new ();
    gtk_box_pack_start ((GtkBox *) hbox, entry, 1, 1, 0);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start ((GtkBox *) vbox, hbox, 1, 1, 0);

    GtkWidget * vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start ((GtkBox *) hbox, vbox2, 1, 1, 0);

    label = gtk_label_new (_("Available plugins:"));
    gtk_box_pack_start ((GtkBox *) vbox2, label, 0, 0, 0);

    GtkWidget * scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) scrolled, GTK_SHADOW_IN);
    gtk_box_pack_start ((GtkBox *) vbox2, scrolled, 1, 1, 0);

    plugin_list = create_plugin_list ();
    gtk_container_add ((GtkContainer *) scrolled, plugin_list);

    GtkWidget * hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start ((GtkBox *) vbox2, hbox2, 0, 0, 0);

    GtkWidget * enable_button = gtk_button_new_with_label (_("Enable"));
    gtk_box_pack_end ((GtkBox *) hbox2, enable_button, 0, 0, 0);

    vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start ((GtkBox *) hbox, vbox2, 1, 1, 0);

    label = gtk_label_new (_("Enabled plugins:"));
    gtk_box_pack_start ((GtkBox *) vbox2, label, 0, 0, 0);

    scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) scrolled, GTK_SHADOW_IN);
    gtk_box_pack_start ((GtkBox *) vbox2, scrolled, 1, 1, 0);

    loaded_list = create_loaded_list ();
    gtk_container_add ((GtkContainer *) scrolled, loaded_list);

    hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start ((GtkBox *) vbox2, hbox2, 0, 0, 0);

    GtkWidget * disable_button = gtk_button_new_with_label (_("Disable"));
    gtk_box_pack_end ((GtkBox *) hbox2, disable_button, 0, 0, 0);

    GtkWidget * settings_button = gtk_button_new_with_label (_("Settings"));
    gtk_box_pack_end ((GtkBox *) hbox2, settings_button, 0, 0, 0);

    if (module_path)
        gtk_entry_set_text ((GtkEntry *) entry, module_path);

    g_signal_connect (config_win, "response", (GCallback) gtk_widget_destroy, NULL);
    g_signal_connect (config_win, "destroy", (GCallback) gtk_widget_destroyed, & config_win);
    g_signal_connect (entry, "activate", (GCallback) set_module_path, NULL);
    g_signal_connect (plugin_list, "destroy", (GCallback) gtk_widget_destroyed, & plugin_list);
    g_signal_connect (enable_button, "clicked", (GCallback) enable_selected, NULL);
    g_signal_connect (loaded_list, "destroy", (GCallback) gtk_widget_destroyed, & loaded_list);
    g_signal_connect (disable_button, "clicked", (GCallback) disable_selected, NULL);
    g_signal_connect (settings_button, "clicked", (GCallback) configure_selected, NULL);

    gtk_widget_show_all (config_win);
}

static const char about[] =
 N_("LADSPA Host for Audacious\n"
    "Copyright 2011 John Lindgren");

#define AUD_PLUGIN_NAME        N_("LADSPA Host")
#define AUD_PLUGIN_ABOUT       about
#define AUD_PLUGIN_INIT        init
#define AUD_PLUGIN_CLEANUP     cleanup
#define AUD_PLUGIN_CONFIGWIN   configure
#define AUD_EFFECT_START       ladspa_start
#define AUD_EFFECT_PROCESS     ladspa_process
#define AUD_EFFECT_FLUSH       ladspa_flush
#define AUD_EFFECT_FINISH      ladspa_finish
#define AUD_EFFECT_SAME_FMT    1

#define AUD_DECLARE_EFFECT
#include <libaudcore/plugin-declare.h>
