/*
 * Dynamic Range Compression Plugin for Audacious
 * Copyright 2010-2012 John Lindgren
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

#include <gtk/gtk.h>

#include "config.h"

#include <audacious/i18n.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>
#include <audacious/preferences.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include "compressor.h"

/* What is a "normal" volume?  Replay Gain stuff claims to use 89 dB, but what
 * does that translate to in our PCM range?  Does anybody even know? */
static const char * const compressor_defaults[] = {
 "center", "0.5",
 "range", "0.5",
 NULL};

static PreferencesWidget compressor_prefs_widgets[] = {
 {WIDGET_LABEL, N_("<b>Compression</b>")},
 {WIDGET_SPIN_BTN, N_("Center volume:"),
  .cfg_type = VALUE_FLOAT, .csect = "compressor", .cname = "center",
  .data = {.spin_btn = {0.1, 1, 0.1}}},
 {WIDGET_SPIN_BTN, N_("Dynamic range:"),
  .cfg_type = VALUE_FLOAT, .csect = "compressor", .cname = "range",
  .data = {.spin_btn = {0.0, 3.0, 0.1}}}};

static PluginPreferences compressor_prefs = {
 .domain = PACKAGE,
 .title = N_("Dynamic Range Compressor Settings"),
 .prefs = compressor_prefs_widgets,
 .n_prefs = G_N_ELEMENTS (compressor_prefs_widgets)};

static GtkWidget * about_window = NULL;

void compressor_config_load (void)
{
    aud_config_set_defaults ("compressor", compressor_defaults);
}

void compressor_config_save (void)
{
    if (about_window != NULL)
        gtk_widget_destroy (about_window);
}

static void compressor_about (void)
{
    audgui_simple_message (& about_window, GTK_MESSAGE_INFO, _("About Dynamic "
     "Range Compression Plugin"),
     "Dynamic Range Compression Plugin for Audacious\n"
     "Copyright 2010-2012 John Lindgren\n\n"
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
     "from the use of this software.");
}

AUD_EFFECT_PLUGIN
(
    .name = "Dynamic Range Compressor",
    .init = compressor_init,
    .cleanup = compressor_cleanup,
    .about = compressor_about,
    .settings = & compressor_prefs,
    .start = compressor_start,
    .process = compressor_process,
    .flush = compressor_flush,
    .finish = compressor_finish,
    .preserves_format = TRUE
)
