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

static GtkWidget *projectm = NULL;

void /* GtkWidget */ *
projectM_get_widget(void)
{
    if (projectm == NULL)
    {
        projectm = gtk_projectm_new ();
        g_signal_connect (projectm, "destroy", (GCallback) gtk_widget_destroyed,
         & projectm);
    }

    return projectm;
}

void
projectM_render_pcm(gint16 pcm_data[2][512])
{
    g_return_if_fail(projectm != NULL);

    gtk_projectm_add_pcm_data(projectm, pcm_data);
}

AUD_VIS_PLUGIN
(
    .name = "projectM",
    .num_pcm_chs_wanted = 2,
    .render_pcm = projectM_render_pcm,
    .get_widget = projectM_get_widget,
)
