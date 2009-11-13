/*
 * gtk_projectm_impl.h: GTK+ ProjectM Implementation.
 * Copyright (c) 2008 William Pitcock <nenolod@sacredspiral.co.uk>
 * Portions copyright (c) 2004-2006 Peter Sperl
 *
 * This program is free software; you may distribute it under the terms
 * of the GNU General Public License; version 2.
 */

#include <stdio.h>
#include <string.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <gtk/gtkgl.h>

G_BEGIN_DECLS

GtkWidget *gtk_projectm_new(void);
void gtk_projectm_add_pcm_data(GtkWidget *widget, gint16 pcm_data[2][512]);
void gtk_projectm_toggle_preset_lock(GtkWidget *widget);
void gtk_projectm_preset_prev(GtkWidget *widget);
void gtk_projectm_preset_next(GtkWidget *widget);

G_END_DECLS
