/*
 * albumart.c
 * Copyright 2012 John Lindgren
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

#include <audacious/drct.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <libaudcore/hook.h>
#include <libaudgui/libaudgui-gtk.h>

#include "config.h"

static void album_set_unscaled (GtkWidget * widget, GdkPixbuf * unscaled)
{
    GdkPixbuf * old;

    if ((old = g_object_get_data ((GObject *) widget, "pixbux-unscaled")))
        g_object_unref (old);
    if ((old = g_object_get_data ((GObject *) widget, "pixbux-scaled")))
        g_object_unref (old);

    g_object_set_data ((GObject *) widget, "pixbuf-unscaled", unscaled);
    g_object_set_data ((GObject *) widget, "pixbuf-scaled", NULL);

    gtk_widget_queue_draw (widget);
}

static GdkPixbuf * album_get_scaled (GtkWidget * widget, int maxwidth, int maxheight)
{
    GdkPixbuf * unscaled = g_object_get_data ((GObject *) widget, "pixbuf-unscaled");
    if (! unscaled)
        return NULL;

    int width = gdk_pixbuf_get_width (unscaled);
    int height = gdk_pixbuf_get_height (unscaled);

    if (width > maxwidth || height > maxheight)
    {
        if (width * maxheight > height * maxwidth)
        {
            height = height * maxwidth / width;
            width = maxwidth;
        }
        else
        {
            width = width * maxheight / height;
            height = maxheight;
        }
    }

    GdkPixbuf * scaled = g_object_get_data ((GObject *) widget, "pixbuf-scaled");
    if (scaled)
    {
        if (gdk_pixbuf_get_width (scaled) == width &&
         gdk_pixbuf_get_height (scaled) == height)
            return scaled;

        g_object_unref (scaled);
    }

    scaled = gdk_pixbuf_scale_simple (unscaled, width, height, GDK_INTERP_BILINEAR);
    g_object_set_data ((GObject *) widget, "pixbuf-scaled", scaled);
    return scaled;
}

static bool_t album_draw (GtkWidget * widget, cairo_t * cr)
{
    GdkRectangle rect;
    gtk_widget_get_allocation (widget, & rect);

    GdkPixbuf * scaled = album_get_scaled (widget, rect.width, rect.height);
    if (! scaled)
        return TRUE;

    int x = (rect.width - gdk_pixbuf_get_width (scaled)) / 2;
    int y = (rect.height - gdk_pixbuf_get_height (scaled)) / 2;

    gdk_cairo_set_source_pixbuf (cr, scaled, x, y);
    cairo_paint (cr);
    return TRUE;
}

static bool_t album_configure (GtkWidget * widget, GdkEventConfigure * event)
{
    gtk_widget_queue_draw (widget);
    return TRUE;
}

static void album_update (void * unused, GtkWidget * widget)
{
    if (! aud_drct_get_playing ())
        return;

    GdkPixbuf * unscaled = audgui_pixbuf_request_current ();
    album_set_unscaled (widget, unscaled ? unscaled : audgui_pixbuf_fallback ());
    gtk_widget_queue_draw (widget);
}

static void album_clear (void * unused, GtkWidget * widget)
{
    album_set_unscaled (widget, NULL);
    gtk_widget_queue_draw (widget);
}

static void album_cleanup (GtkWidget * widget)
{
    hook_dissociate_full ("playback begin", (HookFunction) album_update, widget);
    hook_dissociate_full ("current art ready", (HookFunction) album_update, widget);
    hook_dissociate_full ("playback stop", (HookFunction) album_clear, widget);
}

static void * album_get_widget (void)
{
    GtkWidget * widget = gtk_drawing_area_new ();
    gtk_widget_set_size_request (widget, 96, 96);

    g_signal_connect (widget, "draw", (GCallback) album_draw, NULL);
    g_signal_connect (widget, "configure-event", (GCallback) album_configure, NULL);
    g_signal_connect (widget, "destroy", (GCallback) album_cleanup, NULL);

    hook_associate ("playback begin", (HookFunction) album_update, widget);
    hook_associate ("current art ready", (HookFunction) album_update, widget);
    hook_associate ("playback stop", (HookFunction) album_clear, widget);

    return widget;
}

AUD_GENERAL_PLUGIN
(
    .name = N_("Album Art"),
    .domain = PACKAGE,
    .get_widget = album_get_widget
)
