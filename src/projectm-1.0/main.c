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
projectM_toggle_random(GtkToggleButton *button, gpointer unused)
{
    g_return_if_fail(projectm != NULL);

    gtk_projectm_toggle_preset_lock(projectm);
}

void
projectM_preset_prev(void)
{
    g_return_if_fail(projectm != NULL);

    gtk_projectm_preset_prev(projectm);
}

void
projectM_preset_next(void)
{
    g_return_if_fail(projectm != NULL);

    gtk_projectm_preset_next(projectm);
}

void
projectM_init(void)
{
    GtkWidget *vbox;
    GtkWidget *bbox;
    GtkWidget *button;

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

    bbox = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_START);
    gtk_box_pack_start(GTK_BOX(vbox), bbox, TRUE, TRUE, 0);
    gtk_widget_show(bbox);

    button = gtk_toggle_button_new_with_mnemonic(_("_Random"));
    gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(projectM_toggle_random), NULL);
    gtk_widget_show(button);

    button = gtk_button_new_from_stock(GTK_STOCK_GO_BACK);
    gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(projectM_preset_prev), NULL);
    gtk_widget_show(button);

    button = gtk_button_new_from_stock(GTK_STOCK_GO_FORWARD);
    gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(projectM_preset_next), NULL);
    gtk_widget_show(button);

    gtk_widget_show(window);
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
};

VisPlugin *projectM_vplist[] = { &projectM_vtable, NULL };

DECLARE_PLUGIN(projectm, NULL, NULL, NULL, NULL, NULL, NULL,
        projectM_vplist, NULL);
