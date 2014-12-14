/*
 * layout.h
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

#ifndef AUD_GTKUI_LAYOUT_H
#define AUD_GTKUI_LAYOUT_H

#include <gtk/gtk.h>

void layout_load ();
void layout_save ();
void layout_cleanup ();

GtkWidget * layout_new ();
void layout_add_center (GtkWidget * widget);
void layout_add (PluginHandle * plugin, GtkWidget * widget);
void layout_remove (PluginHandle * plugin);
void layout_focus (PluginHandle * plugin);

#endif
