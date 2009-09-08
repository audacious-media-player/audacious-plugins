/*
 * main.cxx: plugin glue to libprojectm
 * Copyright (c) 2008 William Pitcock <nenolod@sacredspiral.co.uk>
 *
 * This program is free software; you may distribute it under the terms
 * of the GNU General Public License; version 2.
 */

#include "config.h"

#include <audacious/plugin.h>
#include <audacious/i18n.h>

#include "gtk_projectm_impl.h"

GtkWidget *projectm = NULL;
GtkWidget *window = NULL;

void
projectM_init(void)
{
    projectm = gtk_projectm_new();
    gtk_container_add(GTK_CONTAINER(window), projectm);
    gtk_widget_show(projectm);
}

GtkWidget *
projectM_get_widget(void)
{
    return projectm;
}

void
projectM_cleanup(void)
{
    g_return_if_fail(window != NULL);

    gtk_widget_destroy(window);
    window = NULL;
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
    .get_widget = projectM_get_widget,
};

VisPlugin *projectM_vplist[] = { &projectM_vtable, NULL };

DECLARE_PLUGIN(projectm, NULL, NULL, NULL, NULL, NULL, NULL,
        projectM_vplist, NULL);
