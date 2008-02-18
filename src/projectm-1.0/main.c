/*
 * main.cxx: plugin glue to libprojectm
 * Copyright (c) 2008 William Pitcock <nenolod@sacredspiral.co.uk>
 * Portions copyright (c) 2004-2006 Peter Sperl
 *
 * This program is free software; you may distribute it under the terms
 * of the GNU General Public License; version 2.
 */

#include <audacious/plugin.h>

#include "gtk_projectm_impl.h"

GtkWidget *projectm = NULL;
GtkWidget *window = NULL;

void
projectM_init(void)
{
    GtkWidget *vbox;

    if (window)
        return;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "ProjectM");
    gtk_container_set_reallocate_redraws(GTK_CONTAINER(window), TRUE);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_widget_show(vbox);

    projectm = gtk_projectm_new();
    gtk_box_pack_start(GTK_BOX(vbox), projectm, TRUE, TRUE, 0);
    gtk_widget_show(projectm);

    gtk_widget_show(window);
}

void
projectM_cleanup(void)
{
    gtk_widget_destroy(window);
}

void
projectM_render_pcm(gint16 pcm_data[2][512])
{
    g_return_if_fail(projectm != NULL);

    gtk_projectm_add_pcm_data(projectm, pcm_data);
}

VisPlugin projectM_vtable = {
    .description = "projectM",
    .num_pcm_chs_wanted = 2,
    .init = projectM_init,
    .cleanup = projectM_cleanup,
    .render_pcm = projectM_render_pcm,
};

VisPlugin *projectM_vplist[] = { &projectM_vtable, NULL };

DECLARE_PLUGIN(projectm, NULL, NULL, NULL, NULL, NULL, NULL,
        projectM_vplist, NULL);
